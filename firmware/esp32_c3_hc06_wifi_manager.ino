/*
 * ==============================================================================
 * GeoShield EWS - ESP32-C3 Firmware Ringkas & Efisien
 * Fitur Inti:
 * 1. Bluetooth HC-06 WiFi Manager (Provisioning - Bebas Hardcode)
 * 2. Sensor Seismik MPU-6050 (PGA, GAL, MMI & Deadband Noise Filter)
 * 3. Sensor Lingkungan: HC-SR04 (Level Air), FC-37 (Hujan), TDS & pH
 * 4. Display LCD 16x2 I2C & Alarm Buzzer/LED
 * 5. Pengiriman Telemetri HTTP/HTTPS POST ke Web Django Backend
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
// 1. PINOUT HARDWARE (ESP32-C3 & ESP32 STANDAR)
// ==============================================================================
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #define HC06_RX_PIN        20
  #define HC06_TX_PIN        21
  #define I2C_SDA_PIN        8
  #define I2C_SCL_PIN        9
  #define HC_TRIG_PIN        6
  #define HC_ECHO_PIN        7
  #define RAIN_ANALOG_PIN    0
  #define TDS_ANALOG_PIN     1
  #define BUZZER_PIN         3
  #define LED_STATUS_PIN     2
  HardwareSerial HC06(0);
#else
  #define HC06_RX_PIN        16
  #define HC06_TX_PIN        17
  #define I2C_SDA_PIN        21
  #define I2C_SCL_PIN        22
  #define HC_TRIG_PIN        5
  #define HC_ECHO_PIN        18
  #define RAIN_ANALOG_PIN    34
  #define TDS_ANALOG_PIN     35
  #define BUZZER_PIN         4
  #define LED_STATUS_PIN     2
  HardwareSerial HC06(1);
#endif

// ==============================================================================
// 2. KONFIGURASI SENSOR & AMBANG BATAS
// ==============================================================================
#define MPU6050_ADDR          0x68
#define MPU6050_PWR_MGMT_1    0x6B
#define MPU6050_ACCEL_XOUT_H  0x3B

const float SEISMIC_DEADBAND_G   = 0.018; // Di bawah ini noise diabaikan
const float SEISMIC_ALPHA        = 0.35;
const float THRESHOLD_PGA_DANGER = 0.060; // Bahaya gempa (MMI VI+)
const float THRESHOLD_WATER_WARN = 100.0; // cm
const float THRESHOLD_WATER_DGR  = 150.0; // cm
const float DISTANCE_TO_BED_CM   = 200.0; // Tinggi sensor ke dasar

float baseAccelX = 0, baseAccelY = 0, baseAccelZ = 1.0;
float filteredPga = 0.0;
bool mpuAvailable = false;

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);
bool lcdAvailable = false;
unsigned long lastLcdSwitch = 0;
int lcdPage = 0;

// NVS Storage
Preferences prefs;
String savedSSID = "";
String savedPass = "";
String serverUrl = "https://farm-slim-yea-history.trycloudflare.com/api/telemetry/";
String stationId = "STA-BDL-01";
String stationName = "Posko Utama 01";
float stationLat = -5.3589;
float stationLng = 105.3156;
float stationElev = 125.0;

// Status WiFi & Timer
enum WifiState { WF_IDLE, WF_CONNECTING, WF_CONNECTED };
WifiState wifiState = WF_IDLE;
unsigned long wifiTimer = 0;
unsigned long lastTelemetry = 0;
const unsigned long TELEMETRY_INTERVAL_MS = 3000;

// Buffer Komunikasi Bluetooth & Serial
String cmdBuffer = "";
unsigned long lastCharTime = 0;

// ==============================================================================
// 3. FUNGSI BANTU UTILITAS
// ==============================================================================
void printBoth(const String &msg) {
  Serial.print(msg);
  HC06.print(msg);
}

void printlnBoth(const String &msg) {
  Serial.println(msg);
  HC06.println(msg);
}

String cleanStr(String s) {
  String out = "";
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    if (c >= 32 && c <= 126) out += c;
  }
  out.trim();
  return out;
}

// ==============================================================================
// 4. INISIALISASI & KALIBRASI MPU-6050
// ==============================================================================
void initMPU6050() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(MPU6050_PWR_MGMT_1);
  Wire.write(0x00);
  mpuAvailable = (Wire.endTransmission() == 0);

  if (mpuAvailable) {
    Serial.println("[MPU] MPU-6050 Terhubung. Kalibrasi baseline...");
    float sx = 0, sy = 0, sz = 0;
    for (int i = 0; i < 50; i++) {
      Wire.beginTransmission(MPU6050_ADDR);
      Wire.write(MPU6050_ACCEL_XOUT_H);
      Wire.endTransmission(false);
      Wire.requestFrom((uint8_t)MPU6050_ADDR, (size_t)6, true);
      if (Wire.available() >= 6) {
        int16_t rx = Wire.read() << 8 | Wire.read();
        int16_t ry = Wire.read() << 8 | Wire.read();
        int16_t rz = Wire.read() << 8 | Wire.read();
        sx += (float)rx / 16384.0;
        sy += (float)ry / 16384.0;
        sz += (float)rz / 16384.0;
      }
      delay(10);
    }
    baseAccelX = sx / 50.0;
    baseAccelY = sy / 50.0;
    baseAccelZ = sz / 50.0;
    Serial.println("[MPU] Kalibrasi selesai.");
  } else {
    Serial.println("[MPU] MPU-6050 Tidak Terdeteksi!");
  }
}

float readSeismic(float &outGal, String &outMmi, String &outDanger) {
  if (!mpuAvailable) {
    outGal = 0.0; outMmi = "I"; outDanger = "AMAN";
    return 0.0;
  }

  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(MPU6050_ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU6050_ADDR, (size_t)6, true);

  if (Wire.available() >= 6) {
    int16_t rx = Wire.read() << 8 | Wire.read();
    int16_t ry = Wire.read() << 8 | Wire.read();
    int16_t rz = Wire.read() << 8 | Wire.read();

    float ax = (float)rx / 16384.0 - baseAccelX;
    float ay = (float)ry / 16384.0 - baseAccelY;
    float az = (float)rz / 16384.0 - baseAccelZ;
    float instantPga = sqrt(ax * ax + ay * ay + az * az);

    if (instantPga < SEISMIC_DEADBAND_G) instantPga = 0.0;
    filteredPga = (SEISMIC_ALPHA * instantPga) + ((1.0 - SEISMIC_ALPHA) * filteredPga);
    if (filteredPga < 0.003) filteredPga = 0.0;

    outGal = filteredPga * 980.665;
    if (outGal < 1.4)       { outMmi = "I";       outDanger = "AMAN"; }
    else if (outGal < 9.0)  { outMmi = "II-III";  outDanger = "WASPADA"; }
    else if (outGal < 30.0) { outMmi = "IV-V";    outDanger = "SIAGA"; }
    else                    { outMmi = "VI+";     outDanger = "BAHAYA"; }

    return filteredPga;
  }
  return 0.0;
}

// ==============================================================================
// 5. PEMBACAAN SENSOR LINGKUNGAN
// ==============================================================================
float readWaterLevel(String &status) {
  digitalWrite(HC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(HC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(HC_TRIG_PIN, LOW);

  long duration = pulseIn(HC_ECHO_PIN, HIGH, 25000);
  if (duration == 0) return 0.0;

  float dist = (duration * 0.0343) / 2.0;
  float lvl = DISTANCE_TO_BED_CM - dist;
  if (lvl < 0) lvl = 0.0;

  if (lvl >= THRESHOLD_WATER_DGR)       status = "BAHAYA";
  else if (lvl >= THRESHOLD_WATER_WARN) status = "SIAGA";
  else                                  status = "NORMAL";
  return lvl;
}

float readRain(int &rawAnalog, String &status) {
  rawAnalog = analogRead(RAIN_ANALOG_PIN);
  if (rawAnalog >= 4000) {
    status = "Cerah";
    return 0.0;
  }
  float rate = ((4095.0 - (float)rawAnalog) / 4095.0) * 80.0;
  if (rate < 2.5)       status = "Rintik";
  else if (rate < 15.0) status = "Sedang";
  else                  status = "Lebat";
  return rate;
}

float readWaterQuality(float &ph) {
  int raw = analogRead(TDS_ANALOG_PIN);
  float v = (raw / 4095.0) * 3.3;
  float tds = (133.42 * v * v * v - 255.86 * v * v + 857.39 * v) * 0.5;
  if (tds < 0) tds = 0;
  ph = 7.3 - ((tds - 120.0) / 1000.0) * 1.6;
  if (ph < 4.5) ph = 4.5;
  if (ph > 9.0) ph = 9.0;
  return tds;
}

// ==============================================================================
// 6. UPDATE LCD & ALARM
// ==============================================================================
void updateDisplay(float pga, float gal, String mmi, String dgr, float tds, float ph, float lvl, String rain) {
  if (!lcdAvailable) return;

  if (millis() - lastLcdSwitch > 2500) {
    lcdPage = (lcdPage + 1) % 2;
    lastLcdSwitch = millis();
    lcd.clear();
  }

  if (savedSSID.length() == 0) {
    lcd.setCursor(0, 0);
    lcd.print("Mode: BT Setup");
    lcd.setCursor(0, 1);
    lcd.print("Set WiFi via BT");
    return;
  }

  if (lcdPage == 0) {
    lcd.setCursor(0, 0);
    lcd.print("GEMPA: "); lcd.print(pga, 3); lcd.print("g");
    lcd.setCursor(0, 1);
    lcd.print("GAL:"); lcd.print(gal, 1);
    lcd.print(" ["); lcd.print(dgr); lcd.print("]");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("AIR:"); lcd.print((int)tds);
    lcd.print("ppm pH:"); lcd.print(ph, 1);
    lcd.setCursor(0, 1);
    lcd.print("LVL:"); lcd.print(lvl, 0);
    lcd.print("cm "); lcd.print(rain);
  }
}

void checkAlarm(float pga, float lvl) {
  bool danger = (pga >= THRESHOLD_PGA_DANGER || lvl >= THRESHOLD_WATER_DGR);
  digitalWrite(BUZZER_PIN, danger ? HIGH : LOW);
  digitalWrite(LED_STATUS_PIN, danger ? HIGH : LOW);
}

// ==============================================================================
// 7. TELEMETRI HTTP POST KE DJANGO
// ==============================================================================
void sendTelemetry(float pga, float gal, String mmi, float lvl, int rainRaw, float rainRate, float tds, float ph) {
  if (WiFi.status() != WL_CONNECTED || serverUrl.length() == 0) return;

  HTTPClient http;
  if (serverUrl.startsWith("https://")) {
    WiFiClientSecure sec;
    sec.setInsecure();
    http.begin(sec, serverUrl);
  } else {
    WiFiClient cli;
    http.begin(cli, serverUrl);
  }

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);

  char buf[384];
  snprintf(buf, sizeof(buf),
    "{\"station_id\":\"%s\",\"station_name\":\"%s\","
    "\"location\":{\"lat\":%.4f,\"lng\":%.4f,\"elevation\":%.1f},"
    "\"seismic\":{\"pga\":%.4f,\"gal\":%.2f,\"mmi\":\"%s\"},"
    "\"flood\":{\"waterLevelCm\":%.1f},"
    "\"rain\":{\"rawAnalog\":%d,\"rateMmh\":%.1f},"
    "\"water_quality\":{\"tds_ppm\":%.1f,\"ph\":%.1f}}",
    stationId.c_str(), stationName.c_str(),
    stationLat, stationLng, stationElev,
    pga, gal, mmi.c_str(), lvl, rainRaw, rainRate, tds, ph);

  int code = http.POST(buf);
  if (code == 200 || code == 201) {
    Serial.println("[HTTP] Telemetri terkirim (200 OK)");
  } else {
    Serial.print("[HTTP] Gagal kirim: "); Serial.println(code);
  }
  http.end();
}

// ==============================================================================
// 8. LOGIKA WIFI & PROVISIONING BLUETOOTH HC-06
// ==============================================================================
void saveWiFi(String ssid, String pass) {
  printlnBoth("\r\n[NVS] Menyimpan WiFi baru...");
  printBoth("SSID: "); printlnBoth(ssid);

  prefs.begin("geoshield-cfg", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();

  savedSSID = ssid;
  savedPass = pass;
  printlnBoth("[NVS] Tersimpan! Merestart ESP32...");
  delay(1500);
  ESP.restart();
}

void processCommand(String input) {
  input = cleanStr(input);
  if (input.length() == 0) return;

  String lower = input;
  lower.toLowerCase();

  // Format: set:NamaWiFi,Password
  if (lower.startsWith("set:") || lower.startsWith("wifi:")) {
    int colon = input.indexOf(':');
    int comma = input.indexOf(',');
    if (comma > colon) {
      String s = cleanStr(input.substring(colon + 1, comma));
      String p = cleanStr(input.substring(comma + 1));
      saveWiFi(s, p);
    } else {
      String s = cleanStr(input.substring(colon + 1));
      saveWiFi(s, "");
    }
  } else if (lower == "clear" || lower == "reset") {
    prefs.begin("geoshield-cfg", false);
    prefs.clear();
    prefs.end();
    savedSSID = "";
    savedPass = "";
    printlnBoth("\r\n[OK] WiFi berhasil di-reset. ESP32 stand-by mode Bluetooth.");
  } else if (lower == "status") {
    printlnBoth("\r\n--- STATUS SISTEM ---");
    printBoth("WiFi SSID : "); printlnBoth(savedSSID.length() > 0 ? savedSSID : "[Belum Diatur]");
    printBoth("Status IP : "); printlnBoth(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Offline");
    printBoth("Endpoint  : "); printlnBoth(serverUrl);
  } else if (lower == "restart" || lower == "reboot") {
    printlnBoth("\r\n[INFO] Merestart ESP32...");
    delay(1000);
    ESP.restart();
  } else {
    printlnBoth("\r\n[?] Perintah tidak dikenal.");
    printlnBoth("Format setting WiFi: set:NamaWiFi,Password");
    printlnBoth("Perintah lain: status | clear | restart");
  }
}

void updateWifi() {
  if (savedSSID.length() == 0) return; // Tunggu konfigurasi via Bluetooth

  switch (wifiState) {
    case WF_IDLE:
      if (millis() - wifiTimer > 3000) {
        Serial.print("[WIFI] Menghubungkan ke "); Serial.println(savedSSID);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.setTxPower(WIFI_POWER_11dBm); // Mencegah brownout ESP32-C3
        WiFi.begin(savedSSID.c_str(), savedPass.length() > 0 ? savedPass.c_str() : NULL);
        wifiTimer = millis();
        wifiState = WF_CONNECTING;
      }
      break;

    case WF_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        wifiState = WF_CONNECTED;
        Serial.print("[WIFI] Terhubung! IP: "); Serial.println(WiFi.localIP());
        HC06.print("\r\n[WIFI] Terhubung! IP: "); HC06.println(WiFi.localIP());
      } else if (millis() - wifiTimer > 15000) {
        Serial.println("[WIFI] Gagal konek, coba lagi nanti...");
        wifiState = WF_IDLE;
        wifiTimer = millis();
      }
      break;

    case WF_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WIFI] Terputus!");
        wifiState = WF_IDLE;
        wifiTimer = millis();
      }
      break;
  }
}

// ==============================================================================
// 9. SETUP & LOOP UTAMA
// ==============================================================================
void setup() {
  Serial.begin(115200);

  // Inisialisasi Pin
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_STATUS_PIN, OUTPUT);
  pinMode(HC_TRIG_PIN, OUTPUT);
  pinMode(HC_ECHO_PIN, INPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_STATUS_PIN, LOW);

  // Inisialisasi I2C & LCD
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.beginTransmission(0x27);
  if (Wire.endTransmission() == 0) {
    lcd.init();
    lcd.backlight();
    lcdAvailable = true;
    lcd.setCursor(0, 0);
    lcd.print("GeoShield EWS");
    lcd.setCursor(0, 1);
    lcd.print("Inisialisasi...");
  }

  // Inisialisasi MPU6050
  initMPU6050();

  // Inisialisasi Bluetooth HC-06
  pinMode(HC06_RX_PIN, INPUT_PULLUP);
  pinMode(HC06_TX_PIN, OUTPUT);
  HC06.begin(9600, SERIAL_8N1, HC06_RX_PIN, HC06_TX_PIN);

  // Baca Konfigurasi dari Flash NVS
  prefs.begin("geoshield-cfg", true);
  savedSSID   = prefs.getString("ssid", "");
  savedPass   = prefs.getString("pass", "");
  serverUrl   = prefs.getString("server", serverUrl);
  stationId   = prefs.getString("station_id", stationId);
  stationName = prefs.getString("station_name", stationName);
  stationLat  = prefs.getFloat("lat", stationLat);
  stationLng  = prefs.getFloat("lng", stationLng);
  stationElev = prefs.getFloat("elev", stationElev);
  prefs.end();

  printlnBoth("\r\n==========================================");
  printlnBoth("🚀 GEOSHIELD EWS - PROVISIONING READY");
  printlnBoth("==========================================");
  if (savedSSID.length() > 0) {
    printBoth("WiFi Tersimpan: "); printlnBoth(savedSSID);
    wifiState = WF_IDLE;
    wifiTimer = millis() - 2000;
  } else {
    printlnBoth("[INFO] Belum ada WiFi tersimpan.");
    printlnBoth("Kirim via Bluetooth: set:NamaWiFi,Password");
    wifiState = WF_IDLE;
  }
  printlnBoth("==========================================\r\n");
}

void loop() {
  // 1. Baca Sensor Gempa & Lingkungan
  float gal = 0;
  String mmi = "", danger = "";
  float pga = readSeismic(gal, mmi, danger);

  String waterStatus = "";
  float waterLevel = readWaterLevel(waterStatus);

  int rainRaw = 0;
  String rainStatus = "";
  float rainRate = readRain(rainRaw, rainStatus);

  float ph = 7.0;
  float tds = readWaterQuality(ph);

  // 2. Alarm & Tampilan LCD
  checkAlarm(pga, waterLevel);
  updateDisplay(pga, gal, mmi, danger, tds, ph, waterLevel, rainStatus);

  // 3. Manajemen Koneksi WiFi
  updateWifi();

  // 4. Kirim Telemetri ke Web Backend secara Berkala
  if (wifiState == WF_CONNECTED && millis() - lastTelemetry >= TELEMETRY_INTERVAL_MS) {
    sendTelemetry(pga, gal, mmi, waterLevel, rainRaw, rainRate, tds, ph);
    lastTelemetry = millis();
  }

  // 5. Pembacaan Serial & Bluetooth HC-06
  while (HC06.available()) {
    char c = (char)HC06.read();
    Serial.write(c);
    lastCharTime = millis();
    if (c == '\r' || c == '\n') {
      if (cmdBuffer.length() > 0) { processCommand(cmdBuffer); cmdBuffer = ""; }
    } else { cmdBuffer += c; }
  }

  while (Serial.available()) {
    char c = (char)Serial.read();
    HC06.write(c);
    lastCharTime = millis();
    if (c == '\r' || c == '\n') {
      if (cmdBuffer.length() > 0) { processCommand(cmdBuffer); cmdBuffer = ""; }
    } else { cmdBuffer += c; }
  }

  if (cmdBuffer.length() > 0 && (millis() - lastCharTime > 1500)) {
    processCommand(cmdBuffer);
    cmdBuffer = "";
  }

  delay(10);
}
