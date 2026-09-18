/*
 * ==============================================================================
 * Project: GeoShield EWS - ESP32 / ESP32-C3 Master Disaster Station Firmware
 * Bluetooth Provisioning (HC-06) + Multi-Sensor EWS + HTTP REST API Client
 * Author: GeoShield EWS Open Source & ITERA Team
 *
 * FITUR UTAMA:
 * 1. Provisioning WiFi Interaktif via Bluetooth HC-06 / Serial Monitor:
 *    - Mendukung perintah: 'menu', 'masukan wifi', 'set:SSID,PASS', 'server:URL', 'status', 'sensor', 'restart'.
 *    - Menyimpan SSID, Password, dan Server URL ke memori flash permanen (NVS Preferences).
 *    - Otomatis restart (ESP.restart()) setelah WiFi baru disimpan.
 * 2. Driver RF WiFi Super Stabil:
 *    - Mode STA dengan TX Power 11dBm (mencegah brownout / drop tegangan ESP32-C3).
 *    - Auto-reconnect jika koneksi internet terputus.
 * 3. Manajemen Daya Cerdas Bluetooth:
 *    - Bluetooth HC-06 aktif saat belum ada koneksi WiFi atau jika WiFi terputus.
 *    - Setelah WiFi terhubung sukses, Bluetooth dapat dinonaktifkan otomatis untuk menghemat daya.
 * 4. Multi-Sensor Multi-Bencana Lengkap:
 *    - MPU-6050: Sensor Gempa bumi & percepatan getaran seismik (PGA / Gal / MMI).
 *    - HC-SR04: Sensor Ketinggian Muka Air / Deteksi Dini Banjir Sungai.
 *    - FC-37 / YL-83: Sensor Intensitas Curah Hujan (Analog ADC).
 *    - Analog TDS Meter: Sensor Kualitas Kejernihan Air Terlarut (ppm).
 *    - 5V Active Buzzer & LED: Sirine darurat lokal saat parameter melewati batas aman.
 * 5. Pengiriman Data Telemetri HTTP POST:
 *    - Mengirim payload JSON standar ke endpoint REST API Django: /api/telemetry/
 *    - Frekuensi pengiriman periodik (default 1000ms / 1 detik).
 * ==============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Preferences.h>

// ==============================================================================
// 1. PINOUT HARDWARE (MENDUKUNG ESP32-C3 & ESP32 DEVKIT V1 OTOMATIS)
// ==============================================================================
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  // Pinout Khusus ESP32-C3 (RISC-V Single Core)
  #define HC06_RX_PIN        20   // Pin RX ESP32-C3 <-- TX HC-06
  #define HC06_TX_PIN        21   // Pin TX ESP32-C3 --> RX HC-06
  #define I2C_SDA_PIN        8    // MPU6050 SDA
  #define I2C_SCL_PIN        9    // MPU6050 SCL
  #define HC_TRIG_PIN        6    // Ultrasonic HC-SR04 Trig
  #define HC_ECHO_PIN        7    // Ultrasonic HC-SR04 Echo
  #define RAIN_ANALOG_PIN    0    // FC-37 Raindrop (ADC1 CH0)
  #define TDS_ANALOG_PIN     1    // TDS Meter (ADC1 CH1)
  #define BUZZER_PIN         3    // Active Buzzer EWS
  #define LED_STATUS_PIN     2    // Onboard Status LED
  HardwareSerial HC06(0);         // UART HC-06
#else
  // Pinout ESP32 DevKit V1 (30 Pin / 38 Pin) Standar
  #define HC06_RX_PIN        16   // Pin RX2 ESP32 <-- TX HC-06
  #define HC06_TX_PIN        17   // Pin TX2 ESP32 --> RX HC-06
  #define I2C_SDA_PIN        21   // MPU6050 SDA
  #define I2C_SCL_PIN        22   // MPU6050 SCL
  #define HC_TRIG_PIN        5    // Ultrasonic HC-SR04 Trig
  #define HC_ECHO_PIN        18   // Ultrasonic HC-SR04 Echo
  #define RAIN_ANALOG_PIN    34   // FC-37 Raindrop (ADC1 CH6)
  #define TDS_ANALOG_PIN     35   // TDS Meter (ADC1 CH7)
  #define BUZZER_PIN         4    // Active Buzzer EWS
  #define LED_STATUS_PIN     2    // Onboard Status LED
  HardwareSerial HC06(2);         // UART2 HC-06
#endif

// ==============================================================================
// 2. REGISTER & KONSTANTA SENSOR
// ==============================================================================
#define MPU6050_ADDR             0x68
#define MPU6050_PWR_MGMT_1       0x6B
#define MPU6050_ACCEL_XOUT_H     0x3B

// Kalibrasi jarak sensor HC-SR04 ke dasar sungai saat kering (cm)
const float DISTANCE_TO_RIVER_BED_CM = 250.0;

// Batas Ambang Darurat EWS Lokal
const float THRESHOLD_PGA_DANGER     = 0.080; // ~78 Gal (Gempa Terasa Nyata/Merusak)
const float THRESHOLD_WATER_DANGER   = 150.0; // Ketinggian air 150 cm (Siaga Banjir)

Preferences preferences;

// ==============================================================================
// 3. VARIABEL STATE & TELEMETRI
// ==============================================================================
enum SetupState {
  STATE_NORMAL,
  STATE_MENU,
  STATE_INPUT_SSID,
  STATE_INPUT_PASS,
  STATE_INPUT_SERVER
};

SetupState currentState = STATE_NORMAL;

String inputBuffer = "";
unsigned long lastCharTime = 0;
const unsigned long BUFFER_TIMEOUT_MS = 250;

String savedSSID = "";
String savedPass = "";
String savedServerUrl = "http://192.168.1.100:8000/api/telemetry/";
String savedStationId = "ST-01-ESP32";
String savedStationName = "Pendeteksi Gempa EWS ITERA - ESP32 C3";
float savedLat = -5.35824;
float savedLng = 105.31465;

String tempSSID = "";
String tempPass = "";

bool isBluetoothActive = false;
bool mpuAvailable = false;

unsigned long lastTelemetryMillis = 0;
const unsigned long TELEMETRY_INTERVAL_MS = 1000; // Kirim telemetri setiap 1 detik

// Prototipe Fungsi
void initSensors();
void initMPU6050();
float readSeismicPga();
float readWaterLevel();
float mapRainfall(int rawAnalog);
float readTdsPpm();
void checkEmergencyAlert(float pga, float waterLevel);
void sendTelemetryHttp(float pga, float waterLevel, int rainRaw, float rainRate, float tds);

void startBluetooth();
void stopBluetooth();
void printBoth(String msg);
void printlnBoth(String msg);
String cleanString(String raw);
void showMainMenu();
void processCommand(String input);
bool tryConnectSavedWiFi();
void saveAndRestart(String ssid, String pass);
void printAllSensorReadings();

// ==============================================================================
// 4. SETUP
// ==============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================================");
  Serial.println("🚀 GEOSHIELD EWS - ESP32 / ESP32-C3 DISASTER MASTER");
  Serial.println("   Stasiun Pemantau Gempa, Banjir, Cuaca & Kualitas Air");
  Serial.println("========================================================");

  // Inisialisasi Sensor & GPIO
  initSensors();

  // 1. Baca data konfigurasi dari Flash NVS (Preferences)
  preferences.begin("geoshield-cfg", true);
  savedSSID = cleanString(preferences.getString("ssid", ""));
  savedPass = cleanString(preferences.getString("pass", ""));
  savedServerUrl = cleanString(preferences.getString("server_url", "http://192.168.1.100:8000/api/telemetry/"));
  savedStationId = cleanString(preferences.getString("station_id", "ST-01-ESP32"));
  savedStationName = cleanString(preferences.getString("station_name", "Stasiun EWS ITERA - ESP32 C3"));
  savedLat = preferences.getFloat("lat", -5.4267);
  savedLng = preferences.getFloat("lng", 105.3179);
  preferences.end();

  Serial.println("[CONFIG] Stasiun ID : " + savedStationId);
  Serial.println("[CONFIG] Nama Posko : " + savedStationName);
  Serial.println("[CONFIG] Endpoint   : " + savedServerUrl);

  // 2. Coba koneksi WiFi jika SSID tersimpan ada
  if (savedSSID.length() > 0) {
    Serial.println("[BOOT] Ditemukan WiFi tersimpan: \"" + savedSSID + "\"");
    Serial.println("[BOOT] Menghubungkan menggunakan driver RF stabil (11dBm)...");

    bool connected = tryConnectSavedWiFi();

    if (connected) {
      Serial.println("\n========================================================");
      Serial.println("✅ [SUKSES] ESP32 TERHUBUNG KE JARINGAN WIFI!");
      Serial.println("   • SSID       : " + WiFi.SSID());
      Serial.println("   • IP Address : " + WiFi.localIP().toString());
      Serial.println("   • RSSI       : " + String(WiFi.RSSI()) + " dBm");
      Serial.println("   • Server API : " + savedServerUrl);
      Serial.println("========================================================\n");
      // Stop Bluetooth untuk hemat daya saat WiFi aktif
      stopBluetooth();
      return;
    } else {
      Serial.println("\n❌ [GAGAL] Tidak dapat terhubung ke WiFi tersimpan.");
      Serial.println("[INFO] Menyalakan Bluetooth HC-06 untuk konfigurasi ulang...\n");
    }
  } else {
    Serial.println("[BOOT] Belum ada konfigurasi WiFi tersimpan di NVS Flash.");
    Serial.println("[INFO] Menyalakan Bluetooth HC-06...\n");
  }

  // 3. Hidupkan Bluetooth jika belum terhubung WiFi
  startBluetooth();
}

// ==============================================================================
// 5. LOOP UTAMA (DUAL TASK: TELEMETRI SENSOR + BLUETOOTH CLI)
// ==============================================================================
void loop() {
  // A. BACA SENSOR & KIRIM TELEMETRI KE SERVER DJANGO JIKA TERKONEKSI
  if (WiFi.status() == WL_CONNECTED) {
    // 1. Baca semua sensor fisik
    float pga_g = readSeismicPga();
    float water_level_cm = readWaterLevel();
    int rain_raw = analogRead(RAIN_ANALOG_PIN);
    float rain_rate_mmh = mapRainfall(rain_raw);
    float tds_ppm = readTdsPpm();

    // 2. Evaluasi Ambang Darurat Lokal (Sirine Buzzer & LED)
    checkEmergencyAlert(pga_g, water_level_cm);

    // 3. Kirim data periodik ke Backend GeoShield
    if (millis() - lastTelemetryMillis >= TELEMETRY_INTERVAL_MS) {
      lastTelemetryMillis = millis();
      sendTelemetryHttp(pga_g, water_level_cm, rain_raw, rain_rate_mmh, tds_ppm);
    }
  } else {
    // Jika WiFi terputus saat monitoring, aktifkan kembali Bluetooth HC-06
    if (!isBluetoothActive) {
      Serial.println("\n⚠️ [WIFI TERPUTUS] Mengaktifkan kembali Bluetooth HC-06...");
      startBluetooth();
    }
  }

  // B. LAYANI INTERAKSI BLUETOOTH HC-06 DARI APLIKASI WEB / HP
  if (isBluetoothActive) {
    while (HC06.available()) {
      char c = (char)HC06.read();
      Serial.write(c);
      lastCharTime = millis();

      if (c == '\r' || c == '\n') {
        if (inputBuffer.length() > 0) {
          processCommand(cleanString(inputBuffer));
          inputBuffer = "";
        }
      } else {
        inputBuffer += c;
      }
    }
  }

  // C. LAYANI INPUT SERIAL DARI PC / DEBUGGER
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (isBluetoothActive) HC06.write(c);
    lastCharTime = millis();

    if (c == '\r' || c == '\n') {
      if (inputBuffer.length() > 0) {
        processCommand(cleanString(inputBuffer));
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }

  // D. AUTO-PROCESS JIKA CLIENT TIDAK MENGIRIM NEWLINE
  if (inputBuffer.length() > 0 && (millis() - lastCharTime > BUFFER_TIMEOUT_MS)) {
    processCommand(cleanString(inputBuffer));
    inputBuffer = "";
  }

  delay(20);
}

// ==============================================================================
// 6. INISIALISASI & PEMBACAAN SENSOR FISIK
// ==============================================================================
void initSensors() {
  pinMode(HC_TRIG_PIN, OUTPUT);
  pinMode(HC_ECHO_PIN, INPUT);
  pinMode(RAIN_ANALOG_PIN, INPUT);
  pinMode(TDS_ANALOG_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_STATUS_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_STATUS_PIN, LOW);

  // Inisialisasi I2C Bus untuk MPU-6050
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  initMPU6050();
}

void initMPU6050() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(MPU6050_PWR_MGMT_1);
  Wire.write(0x00); // Bangunkan MPU6050 dari sleep mode
  byte status = Wire.endTransmission();

  if (status == 0) {
    mpuAvailable = true;
    Serial.println("[MPU6050] Sensor Seismik I2C Terhubung & Siap!");
  } else {
    mpuAvailable = false;
    Serial.println("[MPU6050] Gagal terhubung via I2C. Periksa kabel SDA/SCL.");
  }
}

float readSeismicPga() {
  if (!mpuAvailable) return 0.002;

  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(MPU6050_ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU6050_ADDR, (size_t)6, true);

  if (Wire.available() >= 6) {
    int16_t rawX = Wire.read() << 8 | Wire.read();
    int16_t rawY = Wire.read() << 8 | Wire.read();
    int16_t rawZ = Wire.read() << 8 | Wire.read();

    // Sensitivitas skala default +/- 2g = 16384 LSB/g
    float ax = (float)rawX / 16384.0;
    float ay = (float)rawY / 16384.0;
    float az = (float)rawZ / 16384.0;

    // Vektor akselerasi getaran dinamis tanpa gravitasi statis (az - 1.0g)
    float dynamicPga = sqrt(ax * ax + ay * ay + (az - 1.0) * (az - 1.0));
    return dynamicPga;
  }
  return 0.002;
}

float readWaterLevel() {
  digitalWrite(HC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(HC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(HC_TRIG_PIN, LOW);

  long duration = pulseIn(HC_ECHO_PIN, HIGH, 30000); // 30ms timeout
  if (duration == 0) return 0.0;

  // Kecepatan suara ~0.0343 cm/us
  float distance_sensor_to_water = (duration * 0.0343) / 2.0;
  float water_level_cm = DISTANCE_TO_RIVER_BED_CM - distance_sensor_to_water;
  if (water_level_cm < 0) water_level_cm = 0.0;

  return water_level_cm;
}

float mapRainfall(int rawAnalog) {
  // ADC ESP32: 4095 (Kering) -> 0 (Basah lebat)
  if (rawAnalog >= 4000) return 0.0;
  float rainRate = ((4095.0 - (float)rawAnalog) / 4095.0) * 100.0; // mm/jam
  return rainRate;
}

float readTdsPpm() {
  int rawTds = analogRead(TDS_ANALOG_PIN);
  float voltage = (rawTds / 4095.0) * 3.3; // Tegangan ESP32 ADC
  // Rumus kalibrasi sensor analog TDS V1.0
  float tdsPpm = (133.42 * voltage * voltage * voltage - 255.86 * voltage * voltage + 857.39 * voltage) * 0.5;
  if (tdsPpm < 0) tdsPpm = 0;
  return tdsPpm;
}

void checkEmergencyAlert(float pga, float waterLevel) {
  bool isEmergency = (pga >= THRESHOLD_PGA_DANGER || waterLevel >= THRESHOLD_WATER_DANGER);
  if (isEmergency) {
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_STATUS_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_STATUS_PIN, LOW);
  }
}

// ==============================================================================
// 7. PENGIRIMAN TELEMETRI HTTP REST API (DJANGO BACKEND)
// ==============================================================================
void sendTelemetryHttp(float pga, float waterLevel, int rainRaw, float rainRate, float tds) {
  if (WiFi.status() != WL_CONNECTED || savedServerUrl.length() == 0) return;

  HTTPClient http;
  http.begin(savedServerUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);

  // Buat Payload JSON standar yang dikenali Django views.py
  String json = "{";
  json += "\"station_id\":\"" + savedStationId + "\",";
  json += "\"station_name\":\"" + savedStationName + "\",";
  json += "\"location\":{\"lat\":" + String(savedLat, 6) + ",\"lng\":" + String(savedLng, 6) + "},";
  json += "\"seismic\":{\"pga\":" + String(pga, 4) + "},";
  json += "\"flood\":{\"waterLevelCm\":" + String(waterLevel, 1) + "},";
  json += "\"rain\":{\"rawAnalog\":" + String(rainRaw) + ",\"rateMmh\":" + String(rainRate, 1) + "},";
  json += "\"tds\":{\"ppm\":" + String(tds, 1) + "}";
  json += "}";

  int httpCode = http.POST(json);
  if (httpCode == HTTP_CODE_CREATED || httpCode == HTTP_CODE_OK) {
    // Sukses kirim telemetri
    // Serial.println("📡 [TELEMETRI OK] POST berhasil.");
  } else {
    Serial.println("⚠️ [TELEMETRI ERROR] Kode HTTP: " + String(httpCode) + " ke " + savedServerUrl);
  }
  http.end();
}

// ==============================================================================
// 8. DRIVER WIFI STABIL (11dBm TX POWER TEMPLATE)
// ==============================================================================
bool tryConnectSavedWiFi() {
  if (savedSSID.length() == 0) return false;

  Serial.println("\n[WIFI] Menghubungkan ke: " + savedSSID);

  WiFi.disconnect(true);
  delay(40);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.setTxPower(WIFI_POWER_11dBm); // Mencegah lonjakan daya RF / Brownout

  if (savedPass.length() > 0) {
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
  } else {
    WiFi.begin(savedSSID.c_str());
  }

  unsigned long startAttempt = millis();
  unsigned long dotTimer = millis();

  // Tunggu hingga 12 detik
  while (millis() - startAttempt < 12000) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    if (millis() - dotTimer > 400) {
      Serial.print(".");
      if (isBluetoothActive) HC06.print(".");
      dotTimer = millis();
    }
    delay(50);
  }

  return (WiFi.status() == WL_CONNECTED);
}

// ==============================================================================
// 9. KENDALI BLUETOOTH HC-06 (HIDUP / MATI)
// ==============================================================================
void startBluetooth() {
  if (isBluetoothActive) return;

  pinMode(HC06_RX_PIN, INPUT_PULLUP);
  pinMode(HC06_TX_PIN, OUTPUT);
  HC06.begin(9600, SERIAL_8N1, HC06_RX_PIN, HC06_TX_PIN);
  isBluetoothActive = true;

  delay(200);

  printlnBoth("\r\n========================================================");
  printlnBoth("📶 [BLUETOOTH HC-06 AKTIF] SIAP MENERIMA KONFIGURASI");
  printlnBoth("========================================================");
  if (savedSSID.length() > 0) {
    printlnBoth("⚠️ Status: KONEKSI KE \"" + savedSSID + "\" GAGAL / TERPUTUS.");
    printlnBoth("👉 Ketik 'masukan wifi' untuk mengganti password/SSID.");
  } else {
    printlnBoth("👉 Ketik 'masukan wifi' atau 'menu' untuk membuka form setup.");
  }
  printlnBoth("========================================================\r\n");
}

void stopBluetooth() {
  if (!isBluetoothActive) return;
  HC06.flush();
  HC06.end();
  isBluetoothActive = false;
  Serial.println(F("[BLUETOOTH] Modul HC-06 Dinonaktifkan (Mode Hemat Daya & Bebas Interferensi)."));
}

// ==============================================================================
// 10. PEMBERSIH STRING TERSEMBUNYI (CR/LF/SPASI)
// ==============================================================================
String cleanString(String raw) {
  String cleaned = "";
  for (unsigned int i = 0; i < raw.length(); i++) {
    char c = raw.charAt(i);
    if (c >= 32 && c <= 126) {
      cleaned += c;
    }
  }
  cleaned.trim();
  return cleaned;
}

// ==============================================================================
// 11. PENGOLAHAN COMMAND & LOGIN VIA BLUETOOTH / SERIAL
// ==============================================================================
void processCommand(String input) {
  input = cleanString(input);
  if (input.length() == 0) return;

  String lower = input;
  lower.toLowerCase();

  switch (currentState) {
    case STATE_NORMAL: {
      if (lower.indexOf("wifi") != -1 || lower.indexOf("menu") != -1 || 
          lower.indexOf("masuk") != -1 || lower.indexOf("login") != -1 || 
          lower == "1" || lower == "help") {
        showMainMenu();
      } else if (lower.startsWith("set:")) {
        // Format Pintas: set:NamaWiFi,PasswordWiFi
        int commaIndex = input.indexOf(',');
        if (commaIndex != -1) {
          String s = cleanString(input.substring(4, commaIndex));
          String p = cleanString(input.substring(commaIndex + 1));
          saveAndRestart(s, p);
        } else {
          printlnBoth("\r\n[ERROR] Format salah! Gunakan: set:NamaWiFi,PasswordWiFi\r\n");
        }
      } else if (lower.startsWith("server:")) {
        // Format Pintas Ubah Server: server:http://192.168.1.50:8000/api/telemetry/
        String newUrl = cleanString(input.substring(7));
        if (newUrl.length() > 5) {
          preferences.begin("geoshield-cfg", false);
          preferences.putString("server_url", newUrl);
          preferences.end();
          savedServerUrl = newUrl;
          printlnBoth("\r\n✅ [SERVER URL DISIMPAN]: " + savedServerUrl + "\r\n");
        } else {
          printlnBoth("\r\n[ERROR] Format URL salah!\r\n");
        }
      } else if (lower == "sensor" || lower == "read" || lower == "baca") {
        printAllSensorReadings();
      } else if (lower == "restart" || lower == "reboot") {
        printlnBoth("\r\n[INFO] Merestart ESP32...");
        delay(1000);
        ESP.restart();
      } else {
        printlnBoth("\r\n[?] Perintah diterima: \"" + input + "\"");
        printlnBoth("💡 Ketik 'masukan wifi' atau 'menu' untuk membuka menu interaktif.\r\n");
      }
      break;
    }

    case STATE_MENU: {
      if (input == "1" || lower.indexOf("masuk") != -1 || lower.indexOf("wifi") != -1) {
        currentState = STATE_INPUT_SSID;
        printlnBoth("\r\n--------------------------------------------------------");
        printlnBoth("🔑 [HALAMAN LOGIN WIFI ESP32]");
        printlnBoth("--------------------------------------------------------");
        printlnBoth("Langkah 1/2: Masukkan Nama WiFi (SSID):");
        printBoth("SSID -> ");
      } else if (input == "2" || lower.indexOf("status") != -1) {
        printlnBoth("\r\n--------------------------------------------------------");
        printlnBoth("📊 STATUS PROFIL STASIUN & KONEKSI:");
        printlnBoth("   • Stasiun ID     : " + savedStationId);
        printlnBoth("   • Nama Posko     : " + savedStationName);
        printlnBoth("   • Koordinat      : " + String(savedLat, 6) + ", " + String(savedLng, 6));
        printlnBoth("   • SSID Tersimpan : " + (savedSSID.length() > 0 ? savedSSID : "[Belum Ada]"));
        printlnBoth("   • Status WiFi    : " + String(WiFi.status() == WL_CONNECTED ? "TERHUBUNG 🟢" : "TERPUTUS 🔴"));
        if (WiFi.status() == WL_CONNECTED) {
          printlnBoth("   • IP Address     : " + WiFi.localIP().toString());
          printlnBoth("   • RSSI Sinyal    : " + String(WiFi.RSSI()) + " dBm");
        }
        printlnBoth("   • Server API URL : " + savedServerUrl);
        printlnBoth("--------------------------------------------------------\r\n");
        showMainMenu();
      } else if (input == "3" || lower.indexOf("server") != -1) {
        currentState = STATE_INPUT_SERVER;
        printlnBoth("\r\n--------------------------------------------------------");
        printlnBoth("🌐 [ATUR URL ENDPOINT SERVER DJANGO]");
        printlnBoth("--------------------------------------------------------");
        printlnBoth("URL Saat ini: " + savedServerUrl);
        printlnBoth("Masukkan URL baru (contoh: http://192.168.1.50:8000/api/telemetry/):");
        printBoth("URL -> ");
      } else if (input == "4" || lower.indexOf("sensor") != -1) {
        printAllSensorReadings();
        showMainMenu();
      } else if (input == "5" || lower.indexOf("hapus") != -1 || lower.indexOf("reset") != -1) {
        preferences.begin("geoshield-cfg", false);
        preferences.remove("ssid");
        preferences.remove("pass");
        preferences.end();
        savedSSID = "";
        savedPass = "";
        printlnBoth("\r\n[SUCCESS] Kredensial WiFi berhasil dihapus!");
        showMainMenu();
      } else if (input == "6" || lower.indexOf("restart") != -1) {
        printlnBoth("\r\n[INFO] Merestart ESP32...");
        delay(1000);
        ESP.restart();
      } else {
        printlnBoth("[!] Pilihan tidak valid. Silakan ketik angka 1 sampai 6.");
      }
      break;
    }

    case STATE_INPUT_SSID: {
      tempSSID = cleanString(input);
      currentState = STATE_INPUT_PASS;
      printlnBoth("\r\n✅ [SSID DITERIMA]: \"" + tempSSID + "\"");
      printlnBoth("Langkah 2/2: Masukkan Password WiFi (Ketik 'none' jika tanpa password):");
      printBoth("Password -> ");
      break;
    }

    case STATE_INPUT_PASS: {
      tempPass = cleanString(input);
      if (tempPass.equalsIgnoreCase("none") || tempPass.equalsIgnoreCase("kosong") || tempPass == "-") {
        tempPass = "";
      }

      currentState = STATE_NORMAL;
      saveAndRestart(tempSSID, tempPass);
      break;
    }

    case STATE_INPUT_SERVER: {
      String newServer = cleanString(input);
      if (newServer.length() > 5) {
        preferences.begin("geoshield-cfg", false);
        preferences.putString("server_url", newServer);
        preferences.end();
        savedServerUrl = newServer;
        printlnBoth("\r\n✅ [BERHASIL] URL Server disimpan: " + savedServerUrl);
      } else {
        printlnBoth("\r\n[!] Input tidak valid. URL server tidak diubah.");
      }
      currentState = STATE_NORMAL;
      showMainMenu();
      break;
    }
  }
}

// ==============================================================================
// 12. CETAK BACAAN LIVE SENSOR KE BLUETOOTH & SERIAL
// ==============================================================================
void printAllSensorReadings() {
  float pga = readSeismicPga();
  float water = readWaterLevel();
  int rainRaw = analogRead(RAIN_ANALOG_PIN);
  float rainRate = mapRainfall(rainRaw);
  float tds = readTdsPpm();

  printlnBoth("\r\n========================================================");
  printlnBoth("📈 LIVE TELEMETRI MULTI-SENSOR FISIK ESP32:");
  printlnBoth("   • Gempa / PGA Seismik : " + String(pga, 4) + " g (" + String(pga * 980.665, 2) + " Gal)");
  printlnBoth("   • Ketinggian Air Banjir: " + String(water, 1) + " cm");
  printlnBoth("   • Curah Hujan (Rain)  : " + String(rainRate, 1) + " mm/jam (ADC: " + String(rainRaw) + ")");
  printlnBoth("   • Kualitas Air (TDS)  : " + String(tds, 1) + " ppm");
  printlnBoth("   • Sirine EWS Lokal    : " + String((pga >= THRESHOLD_PGA_DANGER || water >= THRESHOLD_WATER_DANGER) ? "🚨 BUNYI AKTIF!" : "AMAN (SIAGA)"));
  printlnBoth("========================================================\r\n");
}

// ==============================================================================
// 13. SIMPAN KE FLASH NVS & RESTART OTOMATIS
// ==============================================================================
void saveAndRestart(String ssid, String pass) {
  printlnBoth("\r\n========================================================");
  printlnBoth("📋 KONFIRMASI PENYIMPANAN KREDENSIAL WIFI:");
  printlnBoth("   • SSID     : [" + ssid + "]");
  printlnBoth("   • Password : " + (pass.length() > 0 ? "******** (" + String(pass.length()) + " karakter)" : "[Tanpa Password]"));
  printlnBoth("========================================================");
  printlnBoth("💾 Menyimpan kredensial ke Flash NVS ESP32...");

  // Simpan ke Flash Preferences
  preferences.begin("geoshield-cfg", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  preferences.end();

  printlnBoth("✅ [BERHASIL DISIMPAN]");
  printlnBoth("🔄 ESP32 akan merestart otomatis dalam 2 detik untuk menghubungkan WiFi...");
  printlnBoth("📡 Bluetooth HC-06 akan otomatis dinonaktifkan saat WiFi terkoneksi.");
  printlnBoth("========================================================\r\n");

  delay(2000);
  ESP.restart(); // Restart ESP32 otomatis
}

// ==============================================================================
// 14. TAMPILAN MENU UTAMA
// ==============================================================================
void showMainMenu() {
  currentState = STATE_MENU;
  printlnBoth("\r\n========================================================");
  printlnBoth("        📶 MENU PENGATURAN GEOSHIELD EWS ESP32        ");
  printlnBoth("========================================================");
  printlnBoth(" [1] 🔑 Masukkan Nama WiFi & Password Baru");
  printlnBoth(" [2] 📊 Cek Status & Profil Koneksi Saat Ini");
  printlnBoth(" [3] 🌐 Atur URL Endpoint Server Dashboard Django");
  printlnBoth(" [4] 📈 Baca Live Telemetri Semua Sensor Fisik");
  printlnBoth(" [5] 🗑️  Hapus Data WiFi Tersimpan (Reset)");
  printlnBoth(" [6] 🔄 Restart ESP32");
  printlnBoth("========================================================");
  printBoth("Pilih opsi (1-6) -> ");
}

// ==============================================================================
// 15. HELPER DUAL OUTPUT (SERIAL & BLUETOOTH)
// ==============================================================================
void printBoth(String msg) {
  Serial.print(msg);
  if (isBluetoothActive) HC06.print(msg);
}

void printlnBoth(String msg) {
  Serial.println(msg);
  if (isBluetoothActive) HC06.println(msg);
}
