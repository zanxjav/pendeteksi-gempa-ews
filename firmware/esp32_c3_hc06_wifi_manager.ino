/*
 * ==============================================================================
 * GeoShield EWS - ESP32-C3 & Bluetooth HC-06 Access Point WiFi Manager
 * ==============================================================================
 * ALUR SISTEM SESUAI PERMINTAAN:
 * 1. Sebelum tersambung ke WiFi tertentu, ESP32-C3 menjadi Access Point (AP)
 *    bernama "GeoShield-EWS-AP" (IP 192.168.4.1) & mengaktifkan Bluetooth HC-06.
 * 2. Pengguna dapat menghubungkan HP/Laptop via Bluetooth HC-06 ATAU via WiFi AP
 *    untuk mengatur WiFi mana yang harus dituju melalui tampilan Web.
 * 3. Setelah WiFi disetting dari WEB / Bluetooth, ESP32 menyimpan kredensial ke
 *    Flash NVS dan otomatis mereset (reboot).
 * 4. Setelah reboot, ESP32 langsung terhubung ke WiFi tujuan dan tampilan web
 *    akan langsung tersambung secara REALTIME streaming data sensor!
 * ==============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Preferences.h>
#include <LiquidCrystal_I2C.h>

// ==============================================================================
// 1. PINOUT HARDWARE (ESP32-C3 RISC-V & ESP32 DEV BOARD)
// ==============================================================================
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #define HC06_RX_PIN        20   // RX ESP32-C3 <-- TX Bluetooth HC-06
  #define HC06_TX_PIN        21   // TX ESP32-C3 --> RX Bluetooth HC-06
  #define I2C_SDA_PIN        8    // SDA MPU6050 & LCD 16x2
  #define I2C_SCL_PIN        9    // SCL MPU6050 & LCD 16x2
  #define HC_TRIG_PIN        6    // Ultrasonic Trig
  #define HC_ECHO_PIN        7    // Ultrasonic Echo
  #define RAIN_ANALOG_PIN    0    // FC-37 Raindrop
  #define TDS_ANALOG_PIN     1    // TDS Meter
  #define BUZZER_PIN         3    // Buzzer Alarm
  #define LED_STATUS_PIN     2    // Status LED
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
// 2. VARIABEL GLOBAL SENSOR & PARAMETER
// ==============================================================================
#define MPU6050_ADDR          0x68
#define MPU6050_PWR_MGMT_1    0x6B
#define MPU6050_ACCEL_XOUT_H  0x3B

const float SEISMIC_DEADBAND_G   = 0.018; // Getaran di bawah ini dianggap noise (baca 0.000g)
const float SEISMIC_ALPHA        = 0.35;
const float THRESHOLD_PGA_DANGER = 0.060; // Ambang bahaya gempa (MMI VI+)
const float THRESHOLD_WATER_DGR  = 150.0; // Ambang bahaya banjir (cm)
const float DISTANCE_TO_BED_CM   = 200.0; // Jarak sensor ke dasar sungai

float baseAccelX = 0, baseAccelY = 0, baseAccelZ = 1.0;
float currentPga = 0.0, currentGal = 0.0;
String currentMmi = "I (Aman)", currentDanger = "AMAN";
float currentWaterLevel = 0.0;
float currentRainRate = 0.0;
int currentRainRaw = 4095;
String currentRainStatus = "Cerah";
float currentTds = 0.0, currentPh = 7.0;
bool mpuAvailable = false;

// Display LCD I2C 16x2
LiquidCrystal_I2C lcd(0x27, 16, 2);
bool lcdAvailable = false;
unsigned long lastLcdSwitch = 0;
int lcdPage = 0;

// Flash NVS Preferences
Preferences prefs;
String savedSSID = "";
String savedPass = "";
String serverUrl = "https://geoshield-ews.web.app/api/telemetry/";
String stationId = "STA-BDL-01";
String stationName = "Posko Utama 01";

// Server & Captive Portal Access Point
WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;
bool isAPMode = false;

// State WiFi & Timer Telemetri
enum WifiState { WF_IDLE, WF_CONNECTING, WF_CONNECTED };
WifiState wifiState = WF_IDLE;
unsigned long wifiTimer = 0;
unsigned long lastTelemetry = 0;
const unsigned long TELEMETRY_INTERVAL_MS = 2500;

// Buffer Komunikasi Bluetooth HC-06 & Serial
String cmdBuffer = "";
unsigned long lastCharTime = 0;

// ==============================================================================
// 3. FUNGSI UTILITAS LOGGING
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
// 4. HALAMAN WEB PORTAL ACCESS POINT (RESPONSIVE DARK THEME)
// ==============================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>GeoShield EWS - WiFi Setup</title>
<style>
* { box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0B132B; color: #fff; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; }
.card { background: #1C2541; padding: 28px; border-radius: 18px; width: 100%; max-width: 420px; box-shadow: 0 12px 30px rgba(0,0,0,0.6); border: 1px solid #3A506B; }
.logo-title { text-align: center; margin-bottom: 20px; }
.logo-title h2 { margin: 0; color: #38BDF8; font-size: 22px; font-weight: 700; }
.logo-title p { margin: 6px 0 0 0; font-size: 13px; color: #94A3B8; }
.form-group { margin-bottom: 18px; }
label { display: block; font-size: 13px; font-weight: 600; margin-bottom: 6px; color: #E2E8F0; }
input { width: 100%; padding: 13px; border-radius: 10px; border: 1px solid #3A506B; background: #0B132B; color: #fff; font-size: 14px; transition: border-color 0.2s; }
input:focus { outline: none; border-color: #38BDF8; }
button { width: 100%; padding: 14px; background: #2563EB; color: #fff; border: none; border-radius: 10px; font-size: 15px; font-weight: bold; cursor: pointer; transition: background 0.2s, transform 0.1s; }
button:hover { background: #1D4ED8; }
button:active { transform: scale(0.98); }
.box-bt { background: #0B132B; padding: 14px; border-radius: 10px; border-left: 4px solid #10B981; font-size: 12px; color: #CBD5E1; margin-top: 20px; line-height: 1.5; }
.live-pill { display: inline-block; background: #10B981; color: #fff; padding: 2px 8px; border-radius: 12px; font-size: 11px; font-weight: 700; margin-bottom: 8px; }
</style>
</head>
<body>
<div class="card">
  <div class="logo-title">
    <span class="live-pill">AP & BLUETOOTH AKTIF</span>
    <h2>🌋 GeoShield EWS</h2>
    <p>Pengaturan Jaringan WiFi ESP32-C3</p>
  </div>
  <form action="/save" method="POST">
    <div class="form-group">
      <label>Nama Jaringan WiFi (SSID):</label>
      <input type="text" name="ssid" placeholder="Contoh: WiFi-Kampus / Hotspot HP" required autofocus>
    </div>
    <div class="form-group">
      <label>Password WiFi:</label>
      <input type="password" name="pass" placeholder="Kosongkan jika tanpa password">
    </div>
    <button type="submit">💾 Simpan & Restart ESP32</button>
  </form>
  <div class="box-bt">
    📶 <strong>Bluetooth HC-06 Tersedia:</strong><br>
    Anda juga bisa mengirim konfigurasi via terminal Bluetooth dengan format: <code>set:NamaWiFi,Password</code>
  </div>
</div>
</body>
</html>
)rawliteral";

// ==============================================================================
// 5. HANDLER ROUTE WEB SERVER ESP32
// ==============================================================================
void handleRoot() {
  if (isAPMode) {
    server.send_P(200, "text/html", INDEX_HTML);
  } else {
    // Mode Station: Berikan info status realtime
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String html = "<html><head><meta http-equiv='refresh' content='2'><style>body{font-family:sans-serif;background:#0B132B;color:#fff;padding:20px;text-align:center;}.box{background:#1C2541;padding:20px;border-radius:12px;display:inline-block;max-width:400px;}</style></head><body><div class='box'><h2 style='color:#38BDF8'>GeoShield EWS Realtime</h2><p>WiFi Terhubung: <b>" + savedSSID + "</b></p><p>IP: <b>" + WiFi.localIP().toString() + "</b></p><hr><p>PGA Gempa: <b>" + String(currentPga, 3) + " g (" + currentMmi + ")</b></p><p>Air: <b>" + String(currentWaterLevel, 1) + " cm</b> | Hujan: <b>" + currentRainStatus + "</b></p><p>TDS: <b>" + String((int)currentTds) + " ppm</b> | pH: <b>" + String(currentPh, 1) + "</b></p></div></body></html>";
    server.send(200, "text/html", html);
  }
}

void handleSave() {
  String s = cleanStr(server.arg("ssid"));
  String p = cleanStr(server.arg("pass"));

  if (s.length() == 0) {
    server.send(400, "text/plain", "SSID tidak boleh kosong!");
    return;
  }

  printlnBoth("\r\n==========================================");
  printlnBoth("💾 [WEB AP] Menerima Pengaturan WiFi Baru:");
  printBoth("   • SSID : "); printlnBoth(s);
  printlnBoth("==========================================");

  // Simpan ke NVS Flash
  prefs.begin("geoshield-cfg", false);
  prefs.putString("ssid", s);
  prefs.putString("pass", p);
  prefs.end();

  savedSSID = s;
  savedPass = p;

  String resp = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:sans-serif;background:#0B132B;color:#fff;text-align:center;padding:40px;}.card{background:#1C2541;padding:30px;border-radius:16px;display:inline-block;max-width:380px;}</style></head><body><div class='card'><h2 style='color:#10B981'>✅ Kredensial Disimpan!</h2><p>ESP32-C3 sedang merestart untuk terhubung ke <b>" + s + "</b>...</p><p style='color:#94A3B8;font-size:13px;'>Tampilan web dashboard akan tersambung secara realtime.</p></div></body></html>";
  server.send(200, "text/html", resp);

  if (lcdAvailable) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("WiFi Disimpan!");
    lcd.setCursor(0, 1); lcd.print("Rebooting ESP...");
  }

  delay(2000);
  ESP.restart();
}

void handleApiData() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  char json[384];
  snprintf(json, sizeof(json),
    "{\"online\":true,\"station_id\":\"%s\",\"pga\":%.4f,\"gal\":%.2f,\"mmi\":\"%s\",\"waterLevel\":%.1f,\"rainRate\":%.1f,\"rainStatus\":\"%s\",\"tds\":%.1f,\"ph\":%.1f,\"danger\":\"%s\"}",
    stationId.c_str(), currentPga, currentGal, currentMmi.c_str(), currentWaterLevel, currentRainRate, currentRainStatus.c_str(), currentTds, currentPh, currentDanger.c_str());
  server.send(200, "application/json", json);
}

// ==============================================================================
// 6. SENSOR & PERIPHERAL
// ==============================================================================
void initMPU6050() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(MPU6050_PWR_MGMT_1);
  Wire.write(0x00);
  mpuAvailable = (Wire.endTransmission() == 0);

  if (mpuAvailable) {
    Serial.println("[MPU] MPU-6050 Terhubung. Mengkalibrasi baseline...");
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
  }
}

void readAllSensors() {
  // 1. MPU6050
  if (mpuAvailable) {
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
      float instPga = sqrt(ax * ax + ay * ay + az * az);
      if (instPga < SEISMIC_DEADBAND_G) instPga = 0.0;
      currentPga = (SEISMIC_ALPHA * instPga) + ((1.0 - SEISMIC_ALPHA) * currentPga);
      if (currentPga < 0.003) currentPga = 0.0;
      currentGal = currentPga * 980.665;
      if (currentGal < 1.4)       { currentMmi = "I";      currentDanger = "AMAN"; }
      else if (currentGal < 9.0)  { currentMmi = "II-III"; currentDanger = "WASPADA"; }
      else if (currentGal < 30.0) { currentMmi = "IV-V";   currentDanger = "SIAGA"; }
      else                        { currentMmi = "VI+";    currentDanger = "BAHAYA"; }
    }
  }

  // 2. Ultrasonic Water Level
  digitalWrite(HC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(HC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(HC_TRIG_PIN, LOW);
  long dur = pulseIn(HC_ECHO_PIN, HIGH, 25000);
  if (dur > 0) {
    float dist = (dur * 0.0343) / 2.0;
    currentWaterLevel = DISTANCE_TO_BED_CM - dist;
    if (currentWaterLevel < 0) currentWaterLevel = 0.0;
  }

  // 3. Rain Sensor FC-37
  currentRainRaw = analogRead(RAIN_ANALOG_PIN);
  if (currentRainRaw >= 4000) {
    currentRainStatus = "Cerah";
    currentRainRate = 0.0;
  } else {
    currentRainRate = ((4095.0 - (float)currentRainRaw) / 4095.0) * 80.0;
    if (currentRainRate < 2.5)       currentRainStatus = "Rintik";
    else if (currentRainRate < 15.0) currentRainStatus = "Sedang";
    else                             currentRainStatus = "Lebat";
  }

  // 4. TDS & pH
  int rawTds = analogRead(TDS_ANALOG_PIN);
  float v = (rawTds / 4095.0) * 3.3;
  currentTds = (133.42 * v * v * v - 255.86 * v * v + 857.39 * v) * 0.5;
  if (currentTds < 0) currentTds = 0;
  currentPh = 7.3 - ((currentTds - 120.0) / 1000.0) * 1.6;
  if (currentPh < 4.5) currentPh = 4.5;
  if (currentPh > 9.0) currentPh = 9.0;

  // 5. Alarm Darurat
  bool danger = (currentPga >= THRESHOLD_PGA_DANGER || currentWaterLevel >= THRESHOLD_WATER_DGR);
  digitalWrite(BUZZER_PIN, danger ? HIGH : LOW);
  digitalWrite(LED_STATUS_PIN, danger ? HIGH : LOW);
}

void updateLCD() {
  if (!lcdAvailable) return;

  if (isAPMode) {
    lcd.setCursor(0, 0); lcd.print("AP: GeoShield-AP");
    lcd.setCursor(0, 1); lcd.print("IP: 192.168.4.1 ");
    return;
  }

  if (millis() - lastLcdSwitch > 2500) {
    lcdPage = (lcdPage + 1) % 2;
    lastLcdSwitch = millis();
    lcd.clear();
  }

  if (lcdPage == 0) {
    lcd.setCursor(0, 0); lcd.print("GEMPA: "); lcd.print(currentPga, 3); lcd.print("g");
    lcd.setCursor(0, 1); lcd.print("GAL:"); lcd.print(currentGal, 1);
    lcd.print(" ["); lcd.print(currentDanger); lcd.print("]");
  } else {
    lcd.setCursor(0, 0); lcd.print("AIR:"); lcd.print((int)currentTds);
    lcd.print("ppm pH:"); lcd.print(currentPh, 1);
    lcd.setCursor(0, 1); lcd.print("LVL:"); lcd.print(currentWaterLevel, 0);
    lcd.print("cm "); lcd.print(currentRainStatus);
  }
}

// ==============================================================================
// 7. PENGIRIMAN TELEMETRI HTTP POST KE CLOUD WEB
// ==============================================================================
void sendTelemetryHttp() {
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
  http.setTimeout(2500);

  char buf[384];
  snprintf(buf, sizeof(buf),
    "{\"station_id\":\"%s\",\"station_name\":\"%s\","
    "\"seismic\":{\"pga\":%.4f,\"gal\":%.2f,\"mmi\":\"%s\"},"
    "\"flood\":{\"waterLevelCm\":%.1f},"
    "\"rain\":{\"rawAnalog\":%d,\"rateMmh\":%.1f},"
    "\"water_quality\":{\"tds_ppm\":%.1f,\"ph\":%.1f}}",
    stationId.c_str(), stationName.c_str(),
    currentPga, currentGal, currentMmi.c_str(),
    currentWaterLevel, currentRainRaw, currentRainRate,
    currentTds, currentPh);

  int code = http.POST(buf);
  if (code == 200 || code == 201) {
    Serial.println("[TELEMETRI] Terkirim Realtime ke Cloud Web (200 OK)");
  }
  http.end();
}

// ==============================================================================
// 8. PEMROSESAN PERINTAH BLUETOOTH HC-06
// ==============================================================================
void processBluetoothCommand(String input) {
  input = cleanStr(input);
  if (input.length() == 0) return;

  String lower = input;
  lower.toLowerCase();

  if (lower.startsWith("set:") || lower.startsWith("wifi:")) {
    int colon = input.indexOf(':');
    int comma = input.indexOf(',');
    String s = "", p = "";
    if (comma > colon) {
      s = cleanStr(input.substring(colon + 1, comma));
      p = cleanStr(input.substring(comma + 1));
    } else {
      s = cleanStr(input.substring(colon + 1));
      p = "";
    }

    printlnBoth("\r\n==========================================");
    printlnBoth("💾 [BLUETOOTH] Menyimpan Pengaturan WiFi:");
    printBoth("   • SSID     : ["); printBoth(s); printlnBoth("]");
    printBoth("   • Password : ["); printBoth(p.length() > 0 ? "********" : "Tanpa Sandi"); printlnBoth("]");
    printlnBoth("==========================================");

    prefs.begin("geoshield-cfg", false);
    prefs.putString("ssid", s);
    prefs.putString("pass", p);
    prefs.end();

    savedSSID = s;
    savedPass = p;
    printlnBoth("✅ Tersimpan! ESP32 akan merestart dalam 2 detik...");
    delay(2000);
    ESP.restart();
  } else if (lower == "clear" || lower == "reset") {
    prefs.begin("geoshield-cfg", false);
    prefs.clear();
    prefs.end();
    savedSSID = "";
    savedPass = "";
    printlnBoth("\r\n[OK] Kredensial WiFi dihapus. Kembali ke Mode Access Point & Bluetooth.");
    delay(1000);
    ESP.restart();
  } else if (lower == "status") {
    printlnBoth("\r\n--- STATUS ESP32-C3 ---");
    printBoth("Mode WiFi : "); printlnBoth(isAPMode ? "ACCESS POINT (Setup)" : "STATION (Terkoneksi)");
    printBoth("SSID      : "); printlnBoth(savedSSID.length() > 0 ? savedSSID : "[Belum Diatur]");
    printBoth("IP Lokal  : "); printlnBoth(isAPMode ? "192.168.4.1" : WiFi.localIP().toString());
    printBoth("PGA Gempa : "); printBoth(String(currentPga, 3)); printlnBoth(" g");
  } else if (lower == "restart" || lower == "reboot") {
    printlnBoth("[INFO] Merestart ESP32...");
    delay(1000);
    ESP.restart();
  } else {
    printlnBoth("\r\n[?] Perintah diterima: \"" + input + "\"");
    printlnBoth("Format: set:NamaWiFi,Password");
    printlnBoth("Ketik: status | clear | restart");
  }
}

// ==============================================================================
// 9. SETUP & LOOP
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
    lcd.setCursor(0, 0); lcd.print("GeoShield EWS");
    lcd.setCursor(0, 1); lcd.print("Booting Sistem..");
  }

  // Inisialisasi Sensor MPU6050
  initMPU6050();

  // Inisialisasi Bluetooth HC-06 UART
  pinMode(HC06_RX_PIN, INPUT_PULLUP);
  pinMode(HC06_TX_PIN, OUTPUT);
  HC06.begin(9600, SERIAL_8N1, HC06_RX_PIN, HC06_TX_PIN);

  // Baca Kredensial dari Flash NVS
  prefs.begin("geoshield-cfg", true);
  savedSSID = prefs.getString("ssid", "");
  savedPass = prefs.getString("pass", "");
  prefs.end();

  printlnBoth("\r\n==========================================");
  printlnBoth("🚀 GEOSHIELD EWS - ESP32-C3 & BLUETOOTH HC-06");
  printlnBoth("==========================================");

  // Cek apakah sudah ada WiFi tersimpan
  if (savedSSID.length() == 0) {
    // --------------------------------------------------------------------------
    // MODE 1: ACCESS POINT + BLUETOOTH PROVISIONING (Belum Ada WiFi)
    // --------------------------------------------------------------------------
    isAPMode = true;
    printlnBoth("[MODE] Belum ada WiFi tersimpan.");
    printlnBoth("[MODE] Mengaktifkan Access Point: 'GeoShield-EWS-AP'");
    printlnBoth("[MODE] IP Konfigurasi Web: http://192.168.4.1");
    printlnBoth("[MODE] Bluetooth HC-06 Standby (Kirim: set:NamaWiFi,Password)");

    WiFi.mode(WIFI_AP);
    WiFi.softAP("GeoShield-EWS-AP");
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    // Captive Portal DNS
    dnsServer.start(DNS_PORT, "*", apIP);

    if (lcdAvailable) {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("AP: GeoShield-AP");
      lcd.setCursor(0, 1); lcd.print("IP: 192.168.4.1 ");
    }
  } else {
    // --------------------------------------------------------------------------
    // MODE 2: STATION MODE (Sudah Ada WiFi Tersimpan)
    // --------------------------------------------------------------------------
    isAPMode = false;
    printBoth("[MODE] Menghubungkan ke WiFi: "); printlnBoth(savedSSID);

    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_11dBm); // Menjaga stabilitas daya ESP32-C3
    WiFi.begin(savedSSID.c_str(), savedPass.length() > 0 ? savedPass.c_str() : NULL);
    wifiState = WF_CONNECTING;
    wifiTimer = millis();

    if (lcdAvailable) {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Connecting WiFi:");
      lcd.setCursor(0, 1); lcd.print(savedSSID.substring(0, 16));
    }
  }

  // Daftarkan Route Web Server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/api/data", HTTP_GET, handleApiData);
  server.onNotFound(handleRoot); // Captive portal redirect
  server.begin();

  printlnBoth("[SERVER] Web Server Port 80 Berhasil Dimulai.");
  printlnBoth("==========================================\r\n");
}

void loop() {
  // 1. Tangani DNS Captive Portal (jika di mode Access Point)
  if (isAPMode) {
    dnsServer.processNextRequest();
  }

  // 2. Layani Request Web Server (Web Setup / Realtime API)
  server.handleClient();

  // 3. Baca Sensor & Alarm Darurat
  readAllSensors();

  // 4. Update Tampilan LCD 16x2
  updateLCD();

  // 5. Kelola Koneksi WiFi & Telemetri jika di Mode Station
  if (!isAPMode) {
    switch (wifiState) {
      case WF_CONNECTING:
        if (WiFi.status() == WL_CONNECTED) {
          wifiState = WF_CONNECTED;
          printlnBoth("\r\n==========================================");
          printlnBoth("✅ [WIFI TERHUBUNG REALTIME]");
          printBoth("   • SSID       : "); printlnBoth(savedSSID);
          printBoth("   • IP Address : "); printlnBoth(WiFi.localIP().toString());
          printBoth("   • Dashboard  : http://"); printlnBoth(WiFi.localIP().toString());
          printlnBoth("==========================================");
          if (lcdAvailable) {
            lcd.clear();
            lcd.setCursor(0, 0); lcd.print("WiFi Connected!");
            lcd.setCursor(0, 1); lcd.print(WiFi.localIP().toString().substring(0, 16));
            delay(1500);
          }
        } else if (millis() - wifiTimer > 20000) {
          // Jika gagal connect setelah 20 detik, kembali ke Mode AP & Bluetooth
          printlnBoth("\r\n⚠️ [WIFI GAGAL] Tidak dapat terhubung ke: " + savedSSID);
          printlnBoth("Mengaktifkan kembali Access Point untuk setting ulang...");
          prefs.begin("geoshield-cfg", false);
          prefs.clear();
          prefs.end();
          delay(1000);
          ESP.restart();
        }
        break;

      case WF_CONNECTED:
        if (WiFi.status() != WL_CONNECTED) {
          printlnBoth("[WIFI] Koneksi terputus! Mencoba menyambung kembali...");
          wifiState = WF_CONNECTING;
          wifiTimer = millis();
        } else if (millis() - lastTelemetry >= TELEMETRY_INTERVAL_MS) {
          sendTelemetryHttp();
          lastTelemetry = millis();
        }
        break;

      case WF_IDLE:
        break;
    }
  }

  // 6. Layani Perintah Bluetooth HC-06 & Serial USB
  while (HC06.available()) {
    char c = (char)HC06.read();
    Serial.write(c);
    lastCharTime = millis();
    if (c == '\r' || c == '\n') {
      if (cmdBuffer.length() > 0) { processBluetoothCommand(cmdBuffer); cmdBuffer = ""; }
    } else { cmdBuffer += c; }
  }

  while (Serial.available()) {
    char c = (char)Serial.read();
    HC06.write(c);
    lastCharTime = millis();
    if (c == '\r' || c == '\n') {
      if (cmdBuffer.length() > 0) { processBluetoothCommand(cmdBuffer); cmdBuffer = ""; }
    } else { cmdBuffer += c; }
  }

  if (cmdBuffer.length() > 0 && (millis() - lastCharTime > 1500)) {
    processBluetoothCommand(cmdBuffer);
    cmdBuffer = "";
  }

  delay(5);
}
