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
#include <WiFiClientSecure.h>
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
// 5. FLASH PREFERENCES (NVS PERSISTENT STORAGE) & CREDENTIALS
// ==============================================================================
Preferences preferences;

// Kredensial WiFi Default (Bisa diganti dinamis via Bluetooth HC-06 & Flash NVS)
const char* default_ssid     = "GALAXY A33 5G";
const char* default_password = "cicing77";

String savedSSID       = "";
String savedPass       = "";
String pendingSSID     = "";
// Endpoint Telemetri Resmi Web Django Live Cloudflare
String savedServerUrl  = "https://farm-slim-yea-history.trycloudflare.com/api/telemetry/";
String savedStationId  = "EWS-BDL-01";
String savedStationName= "Posko EWS ITERA - Bandar Lampung";
float  savedLat        = -5.4267;
float  savedLng        = 105.3179;
float  savedElevation  = 124.0; // mdpl (Meter Diatas Permukaan Laut)

// ==============================================================================
// 6. WIFI STATE MANAGEMENT (SISTEM NON-BLOCKING DENGAN BROWNOUT-PROTECTION 11dBm)
// ==============================================================================
enum WifiState {
    WIFI_IDLE,
    WIFI_CONNECTING,
    WIFI_CONNECTED
};

WifiState wifiState = WIFI_IDLE;
unsigned long wifiStart = 0;
unsigned long wifiRetry = 0;
unsigned long dotTimer  = 0;
int wifiAttempt = 0;

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
String wifiStatus(wl_status_t s);
String getActiveSSID();
String getActivePass();
void startWifi();
void updateWifi();
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
  Serial.println("   Non-Blocking State-Machine WiFi (11dBm TX Power)");
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
  if (savedServerUrl.indexOf("10.11.207.118") != -1) {
    savedServerUrl = "https://farm-slim-yea-history.trycloudflare.com/api/telemetry/";
  }
  savedStationId  = preferences.getString("station_id", savedStationId);
  savedStationName= preferences.getString("station_name", savedStationName);
  savedLat        = preferences.getFloat("lat", savedLat);
  savedLng        = preferences.getFloat("lng", savedLng);
  savedElevation  = preferences.getFloat("elev", savedElevation);
  preferences.end();

  // Kalibrasi MPU6050 saat posisi diam (Baseline)
  calibrateMPU6050Baseline();

  // Inisialisasi WiFi State Machine Non-Blocking
  wifiRetry = millis() - 2000;
  wifiState = WIFI_IDLE;

  // Aktifkan Bluetooth HC-06 sebagai Access Point Provisioning Gateway
  startBluetooth();
}

// ==============================================================================
// LOOP UTAMA
// ==============================================================================
unsigned long lastTelemetryTime = 0;
const unsigned long TELEMETRY_INTERVAL_MS = 1000; // Kirim tiap 1 detik

void loop() {
  // 1. Update State Machine WiFi secara non-blocking
  updateWifi();

  // 2. Baca Sensor Seismik & Fisik Secara Real-Time
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

  // 3. Evaluasi Alarm Darurat Lokal (Buzzer & LED)
  checkEmergencyAlert(pga, waterLevel);

  // 4. Update Tampilan LCD I2C 16x2 (Bergantian tiap 2.5 detik)
  updateLcdDisplay(pga, gal, mmi, dangerScale, tdsPpm, ph, waterLevel, rainStatus);

  // 5. Kirim Telemetri ke Django Backend Jika WiFi Terkoneksi
  if (wifiState == WIFI_CONNECTED && WiFi.status() == WL_CONNECTED) {
    if (millis() - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
      sendTelemetryHttp(pga, gal, mmi, waterLevel, rainRaw, rainRate, tdsPpm, ph);
      lastTelemetryTime = millis();
    }
  }

  // 6. Layani Perintah Bluetooth HC-06 & USB Serial
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

  yield();
  delay(1);
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

  { char _buf[80]; snprintf(_buf, sizeof(_buf), "✅ [MPU-6050] Kalibrasi Selesai. Baseline Z = %.4fg", baseAccelZ); Serial.println(_buf); }
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
  if (savedServerUrl.startsWith("https://")) {
    WiFiClientSecure secureClient;
    secureClient.setInsecure(); // Bypass verifikasi sertifikat SSL untuk kecepatan & fleksibilitas IoT
    http.begin(secureClient, savedServerUrl);
  } else {
    WiFiClient client;
    http.begin(client, savedServerUrl);
  }

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);

  char jsonBuf[512];
  snprintf(jsonBuf, sizeof(jsonBuf),
    "{\"station_id\":\"%s\",\"station_name\":\"%s\","
    "\"location\":{\"lat\":%.6f,\"lng\":%.6f,\"elevation\":%.1f},"
    "\"seismic\":{\"pga\":%.4f,\"gal\":%.2f,\"mmi\":\"%s\"},"
    "\"flood\":{\"waterLevelCm\":%.1f},"
    "\"rain\":{\"rawAnalog\":%d,\"rateMmh\":%.1f},"
    "\"water_quality\":{\"tds_ppm\":%.1f,\"ph\":%.1f}}",
    savedStationId.c_str(), savedStationName.c_str(),
    savedLat, savedLng, savedElevation,
    pga, gal, mmi.c_str(),
    waterLevel,
    rainRaw, rainRate,
    tds, ph);
  String json = String(jsonBuf);

  int code = http.POST(json);
  if (code == HTTP_CODE_OK || code == HTTP_CODE_CREATED) {
    Serial.println("📡 [TELEMETRY] Data Berhasil Terkirim ke Web GIS Monitoring!");
  } else {
    Serial.print("⚠️ [TELEMETRY ERROR] HTTP Status: "); Serial.println(code);
  }
  http.end();
}

// ==============================================================================
// 12. KONEKSI WIFI STATE MACHINE (NON-BLOCKING DENGAN BROWNOUT-SAFE 11dBm)
// ==============================================================================
String wifiStatus(wl_status_t s) {
    switch (s) {
        case WL_CONNECTED:        return "CONNECTED";
        case WL_NO_SSID_AVAIL:    return "NO SSID";
        case WL_CONNECT_FAILED:   return "FAILED";
        case WL_DISCONNECTED:     return "DISCONNECTED";
        default:                  return String((int)s);
    }
}

String getActiveSSID() {
    return (savedSSID.length() > 0) ? savedSSID : String(default_ssid);
}

String getActivePass() {
    return (savedPass.length() > 0) ? savedPass : String(default_password);
}

void startWifi() {
    wifiAttempt++;
    String currentSsid = getActiveSSID();
    String currentPass = getActivePass();

    Serial.print("\n[WIFI] Menghubungkan ke: "); Serial.println(currentSsid);
    if (isBluetoothActive) {
        HC06.print("\n[WIFI] Menghubungkan ke: "); HC06.println(currentSsid);
    }

    WiFi.disconnect(true);
    delay(100);

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);

    // FIX: TX power diturunkan dari 19.5dBm ke 11dBm.
    // ESP32-C3 Super Mini punya regulator tegangan pas-pasan;
    // TX power maksimal sering menyebabkan brownout/reset diam-diam
    // saat modul transmit, yang terasa sebagai "susah connect".
    WiFi.setTxPower(WIFI_POWER_11dBm);

    if (currentPass.length() > 0) {
        WiFi.begin(currentSsid.c_str(), currentPass.c_str());
    } else {
        WiFi.begin(currentSsid.c_str());
    }

    wifiStart = millis();
    dotTimer  = millis();
    wifiState = WIFI_CONNECTING;
}

void updateWifi() {
    switch (wifiState) {
        case WIFI_IDLE:
            if (millis() - wifiRetry > 2000) {
                startWifi();
            }
            break;

        case WIFI_CONNECTING: {
            wl_status_t status = WiFi.status();
            if (status == WL_CONNECTED) {
                Serial.println("\n[WIFI] TERHUBUNG!");
                Serial.print("[WIFI] IP: ");
                Serial.println(WiFi.localIP());

                wifiState = WIFI_CONNECTED;
                wifiAttempt = 0;

                if (isBluetoothActive) {
                    HC06.print("\n✅ [WIFI] TERHUBUNG! IP: "); HC06.println(WiFi.localIP().toString());
                }

                if (lcdAvailable) {
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print("WiFi Terhubung!");
                    lcd.setCursor(0, 1);
                    lcd.print(WiFi.localIP().toString());
                }
            } else {
                if (millis() - dotTimer > 500) {
                    Serial.print(".");
                    if (isBluetoothActive) HC06.print(".");
                    dotTimer = millis();
                }

                if (millis() - wifiStart > 10000) {
                    Serial.print("\n[WIFI] Gagal/Timeout: "); Serial.println(wifiStatus(status));
                    if (isBluetoothActive) {
                        HC06.print("\n[WIFI] Gagal/Timeout: "); HC06.println(wifiStatus(status));
                    }
                    WiFi.disconnect(true);
                    wifiState = WIFI_IDLE;
                    wifiRetry = millis();

                    if (!isBluetoothActive) {
                        startBluetooth();
                    }
                }
            }
            break;
        }

        case WIFI_CONNECTED:
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("\n[WIFI] TERPUTUS! Reconnecting...");
                if (isBluetoothActive) {
                    HC06.println("\n[WIFI] TERPUTUS! Reconnecting...");
                }
                wifiState = WIFI_IDLE;
                wifiRetry = millis();

                if (!isBluetoothActive) {
                    startBluetooth();
                }
            }
            break;
    }
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
        printBoth("   • WiFi SSID  : "); printlnBoth(getActiveSSID());
        printBoth("   • WiFi State : "); printlnBoth(wifiStatus(WiFi.status()));
        printBoth("   • IP Address : "); printlnBoth(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Offline");
        { char _sb[80]; snprintf(_sb, sizeof(_sb), "   • Posko      : %s (%.0f mdpl)", savedStationName.c_str(), savedElevation); printlnBoth(_sb); }
        printBoth("   • Endpoint   : "); printlnBoth(savedServerUrl);
      } else {
        printBoth("\r\n[?] Perintah diterima: \""); printBoth(input); printlnBoth("\"");
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
        { char _wb[80]; snprintf(_wb, sizeof(_wb), "\r\n📊 Status WiFi: %s (%s)", (WiFi.status() == WL_CONNECTED ? "TERHUBUNG" : "OFFLINE"), wifiStatus(WiFi.status()).c_str()); printlnBoth(_wb); }
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
      pendingSSID = tempSSID;
      currentState = STATE_INPUT_PASS;
      printBoth("\r\n✅ SSID: \""); printBoth(tempSSID); printlnBoth("\"");
      printlnBoth("Masukkan Password WiFi (Ketik 'none' jika tanpa sandi):");
      printBoth("Password -> ");
      break;
    }

    case STATE_INPUT_PASS: {
      String tempPass = cleanString(input);
      if (tempPass.equalsIgnoreCase("none")) tempPass = "";
      currentState = STATE_NORMAL;
      saveAndRestart(pendingSSID, tempPass);
      break;
    }
  }
}

void saveAndRestart(String ssid, String pass) {
  printlnBoth("\r\n==========================================");
  printlnBoth("💾 MENYIMPAN KREDENSIAL WIFI KE FLASH NVS:");
  printBoth("   • SSID     : ["); printBoth(ssid); printlnBoth("]");
  printBoth("   • Password : ");
  if (pass.length() > 0) { printlnBoth("********"); } else { printlnBoth("[Tanpa Sandi]"); }
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
