/*
 * ==============================================================================
 * Project: GeoShield EWS - ESP32-C3 Master Disaster Early Warning System
 * Microcontroller: ESP32-C3 (RISC-V) / ESP32 DevKit V1
 * Sensors: MPU-6050 (Seismik Gempa), TDS & pH Air, FC-37 Hujan, HC-SR04 Water Level
 * Actuators: LCD I2C 16x2, Active Buzzer, LED Alert
 * Connectivity: Bluetooth HC-06 (WiFi Manager & Login AP) + HTTP REST API Client
 * Author: GeoShield EWS - ITERA & Bandar Lampung Monitoring Team
 * ==============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Preferences.h>
#include <LiquidCrystal_I2C.h>

// ==============================================================================
// 1. PINOUT HARDWARE (MENDUKUNG ESP32-C3 & ESP32 STANDARD OTOMATIS)
// ==============================================================================
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  // Pinout Khusus ESP32-C3 (RISC-V Core)
  #define HC06_RX_PIN        20   // Pin RX ESP32-C3 <-- TX HC-06
  #define HC06_TX_PIN        21   // Pin TX ESP32-C3 --> RX HC-06
  #define I2C_SDA_PIN        8    // MPU6050 & LCD I2C SDA
  #define I2C_SCL_PIN        9    // MPU6050 & LCD I2C SCL
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
  #define I2C_SDA_PIN        21   // MPU6050 & LCD I2C SDA
  #define I2C_SCL_PIN        22   // MPU6050 & LCD I2C SCL
  #define HC_TRIG_PIN        5    // Ultrasonic HC-SR04 Trig
  #define HC_ECHO_PIN        18   // Ultrasonic HC-SR04 Echo
  #define RAIN_ANALOG_PIN    34   // FC-37 Raindrop (ADC1 CH6)
  #define TDS_ANALOG_PIN     35   // TDS Meter (ADC1 CH7)
  #define BUZZER_PIN         4    // Active Buzzer EWS
  #define LED_STATUS_PIN     2    // Onboard Status LED
  HardwareSerial HC06(1);         // UART HC-06
#endif

// ==============================================================================
// 2. KONFIGURASI MPU-6050 & DEAD-BAND SENSITIVITAS GEMPA
// ==============================================================================
#define MPU6050_ADDR          0x68
#define MPU6050_PWR_MGMT_1    0x6B
#define MPU6050_ACCEL_XOUT_H  0x3B

// Deadband Filter: Getaran di bawah ambang batas ini dianggap noise (baca 0.000 g)
const float SEISMIC_NOISE_DEADBAND_G = 0.018; // ~17.6 Gal (noise meja/kaki = 0)
const float SEISMIC_ALPHA_FILTER      = 0.35;  // Filter Low-Pass Exponential Smoothing

float baseAccelX = 0.0, baseAccelY = 0.0, baseAccelZ = 1.0;
float currentFilteredPga = 0.0;
bool mpuAvailable = false;

// ==============================================================================
// 3. LCD I2C 16x2 DUAL DISPLAY (SAFE RUNTIME SCANNER)
// ==============================================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);
bool lcdAvailable = false;
unsigned long lastLcdSwitchTime = 0;
int currentLcdPage = 0; // 0 = Gempa & Seismik, 1 = Kualitas Air & Hujan

// ==============================================================================
// 4. BATAS AMBANG DARURAT (THRESHOLDS)
// ==============================================================================
const float THRESHOLD_PGA_WARNING     = 0.025; // Mulai terasa gempa ringan
const float THRESHOLD_PGA_DANGER      = 0.060; // Gempa merusak (MMI VI+)
const float THRESHOLD_WATER_WARNING   = 100.0; // cm
const float THRESHOLD_WATER_DANGER    = 150.0; // cm
const float DISTANCE_TO_RIVER_BED_CM  = 200.0; // Tinggi sensor ke dasar saluran

// ==============================================================================
// 5. FLASH PREFERENCES (NVS PERSISTENT STORAGE)
// ==============================================================================
Preferences preferences;

String savedSSID       = "";
String savedPass       = "";
String savedServerUrl  = "http://10.11.207.118:8000/api/telemetry/";
String savedStationId  = "EWS-BDL-01";
String savedStationName= "Posko EWS ITERA - Bandar Lampung";
float  savedLat        = -5.4267;
float  savedLng        = 105.3179;
float  savedElevation  = 124.0; // mdpl (Meter Diatas Permukaan Laut)

// State Machine Setup WiFi via Bluetooth
enum SetupState {
  STATE_NORMAL,
  STATE_MENU,
  STATE_INPUT_SSID,
  STATE_INPUT_PASS
};
SetupState currentState = STATE_NORMAL;

String inputBuffer = "";
unsigned long lastCharTime = 0;
const unsigned long BUFFER_TIMEOUT_MS = 250;
bool isBluetoothActive = false;

// Prototipe Fungsi
void startBluetooth();
void stopBluetooth();
void printBoth(String msg);
void printlnBoth(String msg);
String cleanString(String raw);
void showMainMenu();
void processCommand(String input);
bool tryConnectSavedWiFi();
void saveAndRestart(String ssid, String pass);
void initSensors();
void initMPU6050();
void calibrateMPU6050Baseline();
float readSeismicPga(float &outGal, String &outMmi, String &outDangerScale);
float readWaterLevel(String &outWaterStatus);
float mapRainfall(int rawAnalog, String &outRainStatus);
float readTdsPpm(float &outPh, String &outWaterQuality);
void updateLcdDisplay(float pga, float gal, String mmi, String dangerScale, float tds, float ph, float waterLevel, String rainStatus);
void sendTelemetryHttp(float pga, float gal, String mmi, float waterLevel, int rainRaw, float rainRate, float tds, float ph);
void checkEmergencyAlert(float pga, float waterLevel);

// ==============================================================================
// SETUP UTAMA
// ==============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==================================================");
  Serial.println("🌋 GEOSHIELD EWS - ESP32-C3 BANDAR LAMPUNG EDITION");
  Serial.println("   MPU-6050 Seismik + TDS & pH + Rain + Water Level");
  Serial.println("   Bluetooth HC-06 Auto-Provisioning & LCD I2C 16x2");
  Serial.println("==================================================\n");

  // Inisialisasi I2C Bus ESP32-C3 (SDA=8, SCL=9)
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  delay(100);

  // Inisialisasi LCD I2C 16x2 (Cek alamat 0x27 dan 0x3F otomatis)
  Wire.beginTransmission(0x27);
  if (Wire.endTransmission() == 0) {
    lcd = LiquidCrystal_I2C(0x27, 16, 2);
    lcd.init();
    lcd.backlight();
    lcdAvailable = true;
  } else {
    Wire.beginTransmission(0x3F);
    if (Wire.endTransmission() == 0) {
      lcd = LiquidCrystal_I2C(0x3F, 16, 2);
      lcd.init();
      lcd.backlight();
      lcdAvailable = true;
    }
  }

  if (lcdAvailable) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("GeoShield EWS");
    lcd.setCursor(0, 1);
    lcd.print("Booting Sistem..");
  }

  // Inisialisasi Sensor Fisik
  initSensors();

  // Baca kredensial WiFi dari Flash NVS
  preferences.begin("geoshield-cfg", true);
  savedSSID       = cleanString(preferences.getString("ssid", ""));
  savedPass       = cleanString(preferences.getString("pass", ""));
  savedServerUrl  = preferences.getString("server", savedServerUrl);
  savedStationId  = preferences.getString("station_id", savedStationId);
  savedStationName= preferences.getString("station_name", savedStationName);
  savedLat        = preferences.getFloat("lat", savedLat);
  savedLng        = preferences.getFloat("lng", savedLng);
  savedElevation  = preferences.getFloat("elev", savedElevation);
  preferences.end();

  // Kalibrasi MPU6050 saat posisi diam (Baseline)
  calibrateMPU6050Baseline();

  // Coba hubungkan ke WiFi tersimpan
  if (savedSSID.length() > 0) {
    Serial.println("[BOOT] Menghubungkan ke WiFi tersimpan: \"" + savedSSID + "\"...");
    if (lcdAvailable) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Konek WiFi:");
      lcd.setCursor(0, 1);
      lcd.print(savedSSID.substring(0, 16));
    }

    bool connected = tryConnectSavedWiFi();

    if (connected) {
      Serial.println("\n==================================================");
      Serial.println("✅ [SUKSES] ESP32 TERHUBUNG KE WIFI!");
      Serial.println("   • SSID       : " + WiFi.SSID());
      Serial.println("   • IP Address : " + WiFi.localIP().toString());
      Serial.println("   • RSSI       : " + String(WiFi.RSSI()) + " dBm");
      Serial.println("==================================================\n");

      if (lcdAvailable) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi Terhubung!");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP().toString());
        delay(1500);
      }

      // Bluetooth dimatikan untuk hemat daya & kestabilan RF
      stopBluetooth();
      return;
    } else {
      Serial.println("\n❌ [GAGAL] Tidak dapat terhubung ke WiFi tersimpan.");
      Serial.println("[INFO] Mengaktifkan Bluetooth HC-06 untuk konfigurasi...\n");
    }
  } else {
    Serial.println("[BOOT] Belum ada konfigurasi WiFi tersimpan.");
    Serial.println("[INFO] Mengaktifkan Bluetooth HC-06...\n");
  }

  // Jika belum terkoneksi, aktifkan Bluetooth HC-06
  startBluetooth();
}

// ==============================================================================
// LOOP UTAMA
// ==============================================================================
unsigned long lastTelemetryTime = 0;
const unsigned long TELEMETRY_INTERVAL_MS = 1000; // Kirim tiap 1 detik

void loop() {
  // 1. Baca Sensor Seismik & Fisik Secara Real-Time
  float gal = 0.0;
  String mmi = "I (Tidak Terasa)";
  String dangerScale = "AMAN";
  float pga = readSeismicPga(gal, mmi, dangerScale);

  String waterStatus = "NORMAL";
  float waterLevel = readWaterLevel(waterStatus);

  int rainRaw = analogRead(RAIN_ANALOG_PIN);
  String rainStatus = "Cerah";
  float rainRate = mapRainfall(rainRaw, rainStatus);

  float ph = 7.2;
  String waterQuality = "Air Bersih";
  float tdsPpm = readTdsPpm(ph, waterQuality);

  // 2. Evaluasi Alarm Darurat Lokal (Buzzer & LED)
  checkEmergencyAlert(pga, waterLevel);

  // 3. Update Tampilan LCD I2C 16x2 (Bergantian tiap 2.5 detik)
  updateLcdDisplay(pga, gal, mmi, dangerScale, tdsPpm, ph, waterLevel, rainStatus);

  // 4. Kirim Telemetri ke Django Backend Jika WiFi Terkoneksi
  if (WiFi.status() == WL_CONNECTED) {
    if (millis() - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
      sendTelemetryHttp(pga, gal, mmi, waterLevel, rainRaw, rainRate, tdsPpm, ph);
      lastTelemetryTime = millis();
    }
  } else {
    // Jika WiFi offline, pastikan Bluetooth aktif untuk menerima konfigurasi
    if (!isBluetoothActive) {
      startBluetooth();
    }
  }

  // 5. Layani Perintah Bluetooth HC-06 & USB Serial
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

  if (inputBuffer.length() > 0 && (millis() - lastCharTime > BUFFER_TIMEOUT_MS)) {
    processCommand(cleanString(inputBuffer));
    inputBuffer = "";
  }

  delay(20);
}

// ==============================================================================
// 6. SENSOR MPU-6050: SUPER STABIL & DEADBAND NOISE FILTER
// ==============================================================================
void initMPU6050() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(MPU6050_PWR_MGMT_1);
  Wire.write(0x00); // Wake up MPU6050
  byte status = Wire.endTransmission();

  if (status == 0) {
    mpuAvailable = true;
    Serial.println("✅ [MPU-6050] Sensor Seismik I2C Terhubung & Aktif!");
  } else {
    mpuAvailable = false;
    Serial.println("⚠️ [MPU-6050] Gagal terhubung via I2C. Periksa kabel SDA/SCL.");
  }
}

void calibrateMPU6050Baseline() {
  if (!mpuAvailable) return;
  Serial.println("[MPU-6050] Memulai kalibrasi baseline getaran (jangan goyang alat)...");
  
  float sumX = 0, sumY = 0, sumZ = 0;
  const int SAMPLES = 60;
  
  for (int i = 0; i < SAMPLES; i++) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(MPU6050_ACCEL_XOUT_H);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)MPU6050_ADDR, (size_t)6, true);

    if (Wire.available() >= 6) {
      int16_t rawX = Wire.read() << 8 | Wire.read();
      int16_t rawY = Wire.read() << 8 | Wire.read();
      int16_t rawZ = Wire.read() << 8 | Wire.read();
      sumX += (float)rawX / 16384.0;
      sumY += (float)rawY / 16384.0;
      sumZ += (float)rawZ / 16384.0;
    }
    delay(15);
  }

  baseAccelX = sumX / SAMPLES;
  baseAccelY = sumY / SAMPLES;
  baseAccelZ = sumZ / SAMPLES;

  Serial.println("✅ [MPU-6050] Kalibrasi Selesai. Baseline Z = " + String(baseAccelZ, 4) + "g");
}

float readSeismicPga(float &outGal, String &outMmi, String &outDangerScale) {
  if (!mpuAvailable) {
    outGal = 0.0;
    outMmi = "I (Tidak Terasa)";
    outDangerScale = "AMAN";
    return 0.0;
  }

  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(MPU6050_ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU6050_ADDR, (size_t)6, true);

  if (Wire.available() >= 6) {
    int16_t rawX = Wire.read() << 8 | Wire.read();
    int16_t rawY = Wire.read() << 8 | Wire.read();
    int16_t rawZ = Wire.read() << 8 | Wire.read();

    float ax = (float)rawX / 16384.0 - baseAccelX;
    float ay = (float)rawY / 16384.0 - baseAccelY;
    float az = (float)rawZ / 16384.0 - baseAccelZ;

    // Hitung magnitudo vektor percepatan dinamis
    float instantPga = sqrt(ax * ax + ay * ay + az * az);

    // FITUR REQUEST: Deadband filter getaran minim (noise meja/kaki tetap membaca 0)
    if (instantPga < SEISMIC_NOISE_DEADBAND_G) {
      instantPga = 0.0;
    }

    // Filter Exponential Smoothing untuk stabilitas tinggi
    currentFilteredPga = (SEISMIC_ALPHA_FILTER * instantPga) + ((1.0 - SEISMIC_ALPHA_FILTER) * currentFilteredPga);
    if (currentFilteredPga < 0.003) currentFilteredPga = 0.0;

    // Hitung Akselerasi Gal (1 g = 980.665 cm/s²)
    outGal = currentFilteredPga * 980.665;

    // Klasifikasi Skala MMI & Tingkat Bahaya Gempa
    if (outGal < 1.4) {
      outMmi = "I (Tidak Terasa)";
      outDangerScale = "AMAN";
    } else if (outGal < 9.0) {
      outMmi = "II - III (Getaran Ringan)";
      outDangerScale = "WASPADA";
    } else if (outGal < 30.0) {
      outMmi = "IV - V (Sedang / Nyata)";
      outDangerScale = "SIAGA";
    } else {
      outMmi = "VI+ (Kuat / Merusak)";
      outDangerScale = "BAHAYA";
    }

    return currentFilteredPga;
  }

  return 0.0;
}

// ==============================================================================
// 7. SENSOR TDS & pH AIR REAL-TIME
// ==============================================================================
float readTdsPpm(float &outPh, String &outWaterQuality) {
  int rawTds = analogRead(TDS_ANALOG_PIN);
  float voltage = (rawTds / 4095.0) * 3.3; // ADC ESP32

  // Formula konversi tegangan ke partikel terlarut PPM
  float tdsPpm = (133.42 * voltage * voltage * voltage - 255.86 * voltage * voltage + 857.39 * voltage) * 0.5;
  if (tdsPpm < 0) tdsPpm = 0;

  // Estimasi parameter pH korelasi konduktivitas air
  outPh = 7.3 - ((tdsPpm - 120.0) / 1000.0) * 1.6;
  if (outPh < 4.5) outPh = 4.5;
  if (outPh > 9.0) outPh = 9.0;

  // Klasifikasi Kondisi Air Real-Time
  if (tdsPpm <= 50) {
    outWaterQuality = "Air Minum Murni (Sangat Bersih)";
  } else if (tdsPpm <= 300) {
    outWaterQuality = "Air Bersih Standar PDAM (Layak)";
  } else if (tdsPpm <= 600) {
    outWaterQuality = "Air Tercemar Sedang";
  } else if (tdsPpm <= 1000) {
    outWaterQuality = "Air Tercemar Berat";
  } else {
    outWaterQuality = "Air Keruh / Lumpur Banjir (Bahaya)";
  }

  return tdsPpm;
}

// ==============================================================================
// 8. SENSOR HUJAN & WATER LEVEL (BANJIR)
// ==============================================================================
float mapRainfall(int rawAnalog, String &outRainStatus) {
  // Sensor FC-37: 4095 (Kering) -> 0 (Basah lebat)
  if (rawAnalog >= 4000) {
    outRainStatus = "Cerah / Kering";
    return 0.0;
  }

  float rainRate = ((4095.0 - (float)rawAnalog) / 4095.0) * 80.0; // mm/jam
  if (rainRate < 2.5) {
    outRainStatus = "Hujan Rintik-Rintik";
  } else if (rainRate < 15.0) {
    outRainStatus = "Hujan Sedang";
  } else if (rainRate < 50.0) {
    outRainStatus = "Hujan Lebat (Waspada)";
  } else {
    outRainStatus = "Hujan Sangat Ekstrem (Bahaya Banjir)";
  }

  return rainRate;
}

float readWaterLevel(String &outWaterStatus) {
  digitalWrite(HC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(HC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(HC_TRIG_PIN, LOW);

  long duration = pulseIn(HC_ECHO_PIN, HIGH, 25000); // 25ms timeout
  if (duration == 0) return 0.0;

  float distance = (duration * 0.0343) / 2.0;
  float levelCm = DISTANCE_TO_RIVER_BED_CM - distance;
  if (levelCm < 0) levelCm = 0.0;

  if (levelCm >= THRESHOLD_WATER_DANGER) {
    outWaterStatus = "BAHAYA (AWAS)";
  } else if (levelCm >= THRESHOLD_WATER_WARNING) {
    outWaterStatus = "SIAGA BANJIR";
  } else if (levelCm >= 70.0) {
    outWaterStatus = "WASPADA";
  } else {
    outWaterStatus = "NORMAL";
  }

  return levelCm;
}

// ==============================================================================
// 9. UPDATE TAMPILAN LCD I2C 16x2
// ==============================================================================
void updateLcdDisplay(float pga, float gal, String mmi, String dangerScale, float tds, float ph, float waterLevel, String rainStatus) {
  if (!lcdAvailable) return;

  // Ganti halaman tiap 2.5 detik
  if (millis() - lastLcdSwitchTime > 2500) {
    currentLcdPage = (currentLcdPage + 1) % 2;
    lastLcdSwitchTime = millis();
    lcd.clear();
  }

  if (currentLcdPage == 0) {
    // Halaman 1: Pemantauan Gempa Seismik
    lcd.setCursor(0, 0);
    lcd.print("GEMPA: ");
    lcd.print(pga, 3);
    lcd.print("g [");
    lcd.print(dangerScale.substring(0, 4));
    lcd.print("]");

    lcd.setCursor(0, 1);
    lcd.print("GAL:");
    lcd.print(gal, 1);
    lcd.print(" MMI:");
    lcd.print(mmi.substring(0, 4));
  } else {
    // Halaman 2: Pemantauan Kualitas Air & Hujan
    lcd.setCursor(0, 0);
    lcd.print("AIR:");
    lcd.print((int)tds);
    lcd.print("ppm pH:");
    lcd.print(ph, 1);

    lcd.setCursor(0, 1);
    lcd.print("LVL:");
    lcd.print(waterLevel, 0);
    lcd.print("cm ");
    lcd.print(rainStatus.substring(0, 7));
  }
}

// ==============================================================================
// 10. ALARM DARURAT
// ==============================================================================
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
// 11. PENGIRIMAN TELEMETRI HTTP POST KE DJANGO BACKEND
// ==============================================================================
void sendTelemetryHttp(float pga, float gal, String mmi, float waterLevel, int rainRaw, float rainRate, float tds, float ph) {
  if (WiFi.status() != WL_CONNECTED || savedServerUrl.length() == 0) return;

  HTTPClient http;
  http.begin(savedServerUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(2500);

  String json = "{";
  json += "\"station_id\":\"" + savedStationId + "\",";
  json += "\"station_name\":\"" + savedStationName + "\",";
  json += "\"location\":{\"lat\":" + String(savedLat, 6) + ",\"lng\":" + String(savedLng, 6) + ",\"elevation\":" + String(savedElevation, 1) + "},";
  json += "\"seismic\":{\"pga\":" + String(pga, 4) + ",\"gal\":" + String(gal, 2) + ",\"mmi\":\"" + mmi + "\"},";
  json += "\"flood\":{\"waterLevelCm\":" + String(waterLevel, 1) + "},";
  json += "\"rain\":{\"rawAnalog\":" + String(rainRaw) + ",\"rateMmh\":" + String(rainRate, 1) + "},";
  json += "\"water_quality\":{\"tds_ppm\":" + String(tds, 1) + ",\"ph\":" + String(ph, 1) + "}";
  json += "}";

  int code = http.POST(json);
  if (code != HTTP_CODE_OK && code != HTTP_CODE_CREATED) {
    // Log error jika gagal
    // Serial.println("⚠️ HTTP Telemetry Error: " + String(code));
  }
  http.end();
}

// ==============================================================================
// 12. KONEKSI WIFI & BLUETOOTH PROVISIONING (HC-06)
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
  WiFi.setTxPower(WIFI_POWER_11dBm); // Mencegah lonjakan daya RF / Brownout pada ESP32-C3

  if (savedPass.length() > 0) {
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
  } else {
    WiFi.begin(savedSSID.c_str());
  }

  unsigned long startAttempt = millis();
  while (millis() - startAttempt < 12000) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    Serial.print(".");
    if (isBluetoothActive) HC06.print(".");
    delay(400);
  }

  return (WiFi.status() == WL_CONNECTED);
}

void startBluetooth() {
  if (isBluetoothActive) return;

  pinMode(HC06_RX_PIN, INPUT_PULLUP);
  pinMode(HC06_TX_PIN, OUTPUT);
  HC06.begin(9600, SERIAL_8N1, HC06_RX_PIN, HC06_TX_PIN);
  isBluetoothActive = true;

  delay(200);

  printlnBoth("\r\n==========================================");
  printlnBoth("📶 [BLUETOOTH AKTIF] PROVISIONING ESP32-C3");
  printlnBoth("==========================================");
  printlnBoth("👉 Ketik 'set:NamaWiFi,PasswordWiFi' atau");
  printlnBoth("   ketik 'menu' untuk membuka form setup.");
  printlnBoth("==========================================\r\n");
}

void stopBluetooth() {
  if (!isBluetoothActive) return;
  HC06.flush();
  HC06.end();
  isBluetoothActive = false;
  Serial.println(F("[BLUETOOTH] Modul HC-06 Dinonaktifkan (Hemat Daya & Bebas Interferensi)."));
}

String cleanString(String raw) {
  String cleaned = "";
  for (unsigned int i = 0; i < raw.length(); i++) {
    char c = raw.charAt(i);
    if (c >= 32 && c <= 126) cleaned += c;
  }
  cleaned.trim();
  return cleaned;
}

void processCommand(String input) {
  input = cleanString(input);
  if (input.length() == 0) return;

  String lower = input;
  lower.toLowerCase();

  switch (currentState) {
    case STATE_NORMAL: {
      if (lower.startsWith("set:")) {
        // Format Ringkas: set:NamaWiFi,PasswordWiFi
        int commaIndex = input.indexOf(',');
        if (commaIndex != -1) {
          String s = cleanString(input.substring(4, commaIndex));
          String p = cleanString(input.substring(commaIndex + 1));
          saveAndRestart(s, p);
        } else {
          printlnBoth("\r\n[ERROR] Format salah! Gunakan: set:NamaWiFi,PasswordWiFi\r\n");
        }
      } else if (lower.indexOf("wifi") != -1 || lower.indexOf("menu") != -1 || lower == "1") {
        showMainMenu();
      } else if (lower == "restart" || lower == "reboot") {
        printlnBoth("\r\n[INFO] Merestart ESP32-C3...");
        delay(1000);
        ESP.restart();
      } else if (lower == "status" || lower == "2") {
        printlnBoth("\r\n📊 STATUS SISTEM ESP32-C3:");
        printlnBoth("   • WiFi SSID  : " + (savedSSID.length() > 0 ? savedSSID : "[Belum Ada]"));
        printlnBoth("   • IP Address : " + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Offline"));
        printlnBoth("   • Posko      : " + savedStationName + " (" + String(savedElevation) + " mdpl)");
        printlnBoth("   • Endpoint   : " + savedServerUrl);
      } else {
        printlnBoth("\r\n[?] Perintah diterima: \"" + input + "\"");
        printlnBoth("💡 Ketik 'set:NamaWiFi,Password' atau 'menu' untuk mengatur WiFi.\r\n");
      }
      break;
    }

    case STATE_MENU: {
      if (input == "1" || lower.indexOf("wifi") != -1) {
        currentState = STATE_INPUT_SSID;
        printlnBoth("\r\n🔑 Masukkan Nama WiFi (SSID):");
        printBoth("SSID -> ");
      } else if (input == "2") {
        currentState = STATE_NORMAL;
        printlnBoth("\r\n📊 Status WiFi: " + String(WiFi.status() == WL_CONNECTED ? "TERHUBUNG 🟢" : "OFFLINE 🔴"));
      } else if (input == "3") {
        preferences.begin("geoshield-cfg", false);
        preferences.clear();
        preferences.end();
        savedSSID = "";
        savedPass = "";
        printlnBoth("\r\n[OK] Kredensial WiFi berhasil dihapus!");
        currentState = STATE_NORMAL;
      } else if (input == "4") {
        printlnBoth("\r\n[INFO] Merestart ESP32...");
        delay(1000);
        ESP.restart();
      }
      break;
    }

    case STATE_INPUT_SSID: {
      String tempSSID = cleanString(input);
      currentState = STATE_INPUT_PASS;
      printlnBoth("\r\n✅ SSID: \"" + tempSSID + "\"");
      printlnBoth("Masukkan Password WiFi (Ketik 'none' jika tanpa sandi):");
      printBoth("Password -> ");
      break;
    }

    case STATE_INPUT_PASS: {
      String tempPass = cleanString(input);
      if (tempPass.equalsIgnoreCase("none")) tempPass = "";
      currentState = STATE_NORMAL;
      saveAndRestart(savedSSID, tempPass);
      break;
    }
  }
}

void saveAndRestart(String ssid, String pass) {
  printlnBoth("\r\n==========================================");
  printlnBoth("💾 MENYIMPAN KREDENSIAL WIFI KE FLASH NVS:");
  printlnBoth("   • SSID     : [" + ssid + "]");
  printlnBoth("   • Password : " + (pass.length() > 0 ? "********" : "[Tanpa Sandi]"));
  printlnBoth("==========================================");

  preferences.begin("geoshield-cfg", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  preferences.end();

  printlnBoth("✅ [BERHASIL DISIMPAN]");
  printlnBoth("🔄 ESP32-C3 akan merestart otomatis dalam 2 detik untuk menghubungkan ke WiFi...");

  if (lcdAvailable) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Disimpan!");
    lcd.setCursor(0, 1);
    lcd.print("Rebooting ESP...");
  }

  delay(2000);
  ESP.restart(); // Auto-restart
}

void showMainMenu() {
  currentState = STATE_MENU;
  printlnBoth("\r\n==========================================");
  printlnBoth("      📶 MENU SETUP WIFI ESP32-C3         ");
  printlnBoth("==========================================");
  printlnBoth(" [1] 🔑 Input Nama WiFi (SSID) & Password");
  printlnBoth(" [2] 📊 Cek Status Koneksi Saat Ini");
  printlnBoth(" [3] 🗑️  Reset Kredensial WiFi Tersimpan");
  printlnBoth(" [4] 🔄 Restart ESP32");
  printlnBoth("==========================================");
  printBoth("Pilih nomor (1-4) -> ");
}

void printBoth(String msg) {
  Serial.print(msg);
  if (isBluetoothActive) HC06.print(msg);
}

void printlnBoth(String msg) {
  Serial.println(msg);
  if (isBluetoothActive) HC06.println(msg);
}
