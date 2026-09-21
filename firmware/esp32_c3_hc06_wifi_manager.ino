/*
 * ==============================================================================
 * ESP32-C3 & Bluetooth HC-06 - Standalone WiFi Access Point & Provisioning
 * ==============================================================================
 * KHUSUS: Fokus murni pada Access Point & Bluetooth WiFi Manager (Tanpa Sensor).
 * 
 * ALUR KERJA:
 * 1. Sebelum tersambung ke WiFi, ESP32-C3 menjadi Access Point "GeoShield-EWS-AP"
 *    (IP: 192.168.4.1) dan modul Bluetooth HC-06 aktif standby.
 * 2. Pengguna setting WiFi via Web Portal (192.168.4.1) atau via Bluetooth HC-06
 *    (kirim format: set:NamaWiFi,Password).
 * 3. Setelah disetting, ESP32 menyimpan ke Flash NVS dan otomatis me-restart.
 * 4. Setelah restart, ESP32 langsung terhubung ke WiFi tujuan dan web server
 *    lokal langsung aktif tersambung secara REALTIME!
 * ==============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

// ==============================================================================
// 1. PINOUT HARDWARE ESP32-C3
// ==============================================================================
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #define HC06_RX_PIN    20   // RX ESP32-C3 <-- TX Bluetooth HC-06
  #define HC06_TX_PIN    21   // TX ESP32-C3 --> RX Bluetooth HC-06
  #define LED_STATUS_PIN 2    // LED Indikator Onboard
  HardwareSerial HC06(0);
#else
  #define HC06_RX_PIN    16
  #define HC06_TX_PIN    17
  #define LED_STATUS_PIN 2
  HardwareSerial HC06(1);
#endif

// ==============================================================================
// 2. VARIABEL & OBJEK SERVER
// ==============================================================================
Preferences prefs;
String savedSSID = "";
String savedPass = "";

WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;
bool isAPMode = false;

// Buffer Bluetooth HC-06
String cmdBuffer = "";
unsigned long lastCharTime = 0;
unsigned long lastBlink = 0;
bool ledState = false;

// ==============================================================================
// 3. FUNGSI LOGGING SERIAL & BLUETOOTH
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
// 4. TAMPILAN WEB SETUP ACCESS POINT (PORTAL PENDAFTARAN WIFI)
// ==============================================================================
const char SETUP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32-C3 WiFi Setup</title>
<style>
* { box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0B132B; color: #fff; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; }
.card { background: #1C2541; padding: 28px; border-radius: 18px; width: 100%; max-width: 400px; box-shadow: 0 10px 30px rgba(0,0,0,0.6); border: 1px solid #3A506B; }
h2 { margin: 0 0 6px 0; color: #38BDF8; font-size: 22px; text-align: center; }
p { font-size: 13px; color: #94A3B8; text-align: center; margin-bottom: 22px; }
.badge { display: block; width: fit-content; margin: 0 auto 15px auto; background: #10B981; color: #fff; font-size: 11px; font-weight: 700; padding: 3px 10px; border-radius: 12px; }
.form-group { margin-bottom: 16px; }
label { display: block; font-size: 13px; font-weight: 600; margin-bottom: 6px; color: #E2E8F0; }
input { width: 100%; padding: 13px; border-radius: 10px; border: 1px solid #3A506B; background: #0B132B; color: #fff; font-size: 14px; }
input:focus { outline: none; border-color: #38BDF8; }
button { width: 100%; padding: 14px; background: #2563EB; color: #fff; border: none; border-radius: 10px; font-size: 15px; font-weight: 700; cursor: pointer; transition: 0.2s; margin-top: 6px; }
button:hover { background: #1D4ED8; }
.bt-box { background: #0B132B; padding: 14px; border-radius: 10px; border-left: 4px solid #10B981; font-size: 12px; color: #CBD5E1; margin-top: 20px; line-height: 1.5; }
</style>
</head>
<body>
<div class="card">
  <span class="badge">MODE ACCESS POINT</span>
  <h2>⚙️ Setting WiFi ESP32</h2>
  <p>Pilih jaringan WiFi yang ingin dihubungkan</p>
  <form action="/save" method="POST">
    <div class="form-group">
      <label>Nama WiFi (SSID):</label>
      <input type="text" name="ssid" placeholder="Contoh: MyHotspot" required autofocus>
    </div>
    <div class="form-group">
      <label>Password WiFi:</label>
      <input type="password" name="pass" placeholder="Kosongkan jika tanpa sandi">
    </div>
    <button type="submit">💾 Simpan & Restart ESP32</button>
  </form>
  <div class="bt-box">
    📶 <strong>Bluetooth HC-06 Aktif:</strong><br>
    Bisa juga disetting via Bluetooth Terminal dengan perintah: <code>set:NamaWiFi,Password</code>
  </div>
</div>
</body>
</html>
)rawliteral";

// ==============================================================================
// 5. ROUTE WEB SERVER
// ==============================================================================
void handleRoot() {
  if (isAPMode) {
    server.send_P(200, "text/html", SETUP_HTML);
  } else {
    // Mode Station: Dashboard Realtime langsung di ESP32
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String html = "<!DOCTYPE html><html lang='id'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>ESP32 Realtime</title><meta http-equiv='refresh' content='2'><style>body{font-family:-apple-system,sans-serif;background:#0B132B;color:#fff;text-align:center;padding:25px;margin:0;}.box{background:#1C2541;padding:25px;border-radius:16px;display:inline-block;max-width:420px;width:100%;border:1px solid #3A506B;}h2{color:#10B981;margin-top:0;}.status-pill{background:#10B981;color:#fff;padding:4px 12px;border-radius:12px;font-size:12px;font-weight:700;}hr{border:0;border-top:1px solid #3A506B;margin:20px 0;}.item{display:flex;justify-content:space-between;margin:10px 0;font-size:14px;color:#CBD5E1;}.btn-reset{display:inline-block;margin-top:20px;padding:12px 20px;background:#EF4444;color:#fff;text-decoration:none;border-radius:8px;font-weight:bold;font-size:13px;}</style></head><body><div class='box'><span class='status-pill'>ONLINE REALTIME</span><h2>✅ ESP32-C3 Terhubung</h2><hr><div class='item'><span>WiFi SSID:</span><strong style='color:#38BDF8'>" + savedSSID + "</strong></div><div class='item'><span>IP Address:</span><strong style='color:#10B981'>" + WiFi.localIP().toString() + "</strong></div><div class='item'><span>Sinyal RSSI:</span><strong>" + String(WiFi.RSSI()) + " dBm</strong></div><div class='item'><span>Uptime:</span><strong>" + String(millis() / 1000) + " detik</strong></div><div class='item'><span>Bluetooth HC-06:</span><strong style='color:#10B981'>Standby (Aktif)</strong></div><hr><a href='/reset' class='btn-reset' onclick=\"return confirm('Reset WiFi dan kembali ke Access Point?')\">🔄 Ganti / Reset WiFi</a></div></body></html>";
    server.send(200, "text/html", html);
  }
}

void handleSave() {
  String s = cleanStr(server.arg("ssid"));
  String p = cleanStr(server.arg("pass"));

  if (s.length() == 0) {
    server.send(400, "text/plain", "Nama WiFi (SSID) tidak boleh kosong!");
    return;
  }

  printlnBoth("\r\n==========================================");
  printlnBoth("💾 [WEB AP] Menerima Pengaturan WiFi Baru:");
  printBoth("   • SSID     : ["); printBoth(s); printlnBoth("]");
  printBoth("   • Password : ["); printBoth(p.length() > 0 ? "********" : "Tanpa Sandi"); printlnBoth("]");
  printlnBoth("==========================================");

  // Simpan ke Flash NVS
  prefs.begin("geoshield-cfg", false);
  prefs.putString("ssid", s);
  prefs.putString("pass", p);
  prefs.end();

  savedSSID = s;
  savedPass = p;

  String resp = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:sans-serif;background:#0B132B;color:#fff;text-align:center;padding:40px;margin:0;}.card{background:#1C2541;padding:30px;border-radius:16px;display:inline-block;max-width:380px;border:1px solid #10B981;}</style></head><body><div class='card'><h2 style='color:#10B981'>✅ Kredensial Disimpan!</h2><p>ESP32 sedang merestart untuk terhubung ke: <br><b style='color:#38BDF8;font-size:16px;'>" + s + "</b></p><p style='color:#94A3B8;font-size:13px;'>Tampilan web akan langsung tersambung secara realtime.</p></div></body></html>";
  server.send(200, "text/html", resp);

  delay(2000);
  ESP.restart();
}

void handleReset() {
  prefs.begin("geoshield-cfg", false);
  prefs.clear();
  prefs.end();
  savedSSID = "";
  savedPass = "";

  String resp = "<html><body style='background:#0B132B;color:#fff;text-align:center;padding:50px;font-family:sans-serif;'><h2>🔄 WiFi Direset!</h2><p>ESP32 merestart dan kembali ke Mode Access Point...</p></body></html>";
  server.send(200, "text/html", resp);
  delay(1500);
  ESP.restart();
}

void handleApiStatus() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  char json[256];
  snprintf(json, sizeof(json),
    "{\"status\":\"ONLINE\",\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"uptime\":%lu}",
    savedSSID.c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI(), millis() / 1000);
  server.send(200, "application/json", json);
}

// ==============================================================================
// 6. PEMROSESAN PERINTAH BLUETOOTH HC-06
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
    printlnBoth("💾 [BLUETOOTH] Menerima Pengaturan WiFi:");
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
    printlnBoth("\r\n[OK] WiFi direset. Kembali ke Access Point 'GeoShield-EWS-AP'.");
    delay(1000);
    ESP.restart();
  } else if (lower == "status") {
    printlnBoth("\r\n--- STATUS ESP32-C3 ---");
    printBoth("Mode WiFi : "); printlnBoth(isAPMode ? "ACCESS POINT (Setup)" : "STATION (Terkoneksi)");
    printBoth("SSID      : "); printlnBoth(savedSSID.length() > 0 ? savedSSID : "[Belum Diatur]");
    printBoth("IP Address: "); printlnBoth(isAPMode ? "192.168.4.1" : WiFi.localIP().toString());
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
// 7. SETUP & LOOP UTAMA
// ==============================================================================
void setup() {
  Serial.begin(115200);

  // Inisialisasi LED
  pinMode(LED_STATUS_PIN, OUTPUT);
  digitalWrite(LED_STATUS_PIN, LOW);

  // Inisialisasi UART Bluetooth HC-06
  pinMode(HC06_RX_PIN, INPUT_PULLUP);
  pinMode(HC06_TX_PIN, OUTPUT);
  HC06.begin(9600, SERIAL_8N1, HC06_RX_PIN, HC06_TX_PIN);

  // Baca Kredensial WiFi dari Flash NVS
  prefs.begin("geoshield-cfg", true);
  savedSSID = prefs.getString("ssid", "");
  savedPass = prefs.getString("pass", "");
  prefs.end();

  printlnBoth("\r\n==========================================");
  printlnBoth("🚀 ESP32-C3 & BLUETOOTH HC-06 WIFI MANAGER");
  printlnBoth("==========================================");

  if (savedSSID.length() == 0) {
    // --------------------------------------------------------------------------
    // MODE 1: ACCESS POINT + BLUETOOTH (Belum Ada WiFi Tersimpan)
    // --------------------------------------------------------------------------
    isAPMode = true;
    printlnBoth("[MODE] Belum ada WiFi tersimpan.");
    printlnBoth("[MODE] Mengaktifkan Access Point: 'GeoShield-EWS-AP'");
    printlnBoth("[MODE] Buka Web Browser di: http://192.168.4.1");
    printlnBoth("[MODE] Bluetooth HC-06 Siap (Kirim: set:NamaWiFi,Password)");

    WiFi.mode(WIFI_AP);
    WiFi.softAP("GeoShield-EWS-AP");
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    // DNS Server untuk Captive Portal
    dnsServer.start(DNS_PORT, "*", apIP);
  } else {
    // --------------------------------------------------------------------------
    // MODE 2: STATION MODE (Sudah Ada WiFi Tersimpan)
    // --------------------------------------------------------------------------
    isAPMode = false;
    printBoth("[MODE] Menghubungkan ke WiFi: "); printlnBoth(savedSSID);

    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_11dBm); // Menjaga stabilitas arus ESP32-C3
    WiFi.begin(savedSSID.c_str(), savedPass.length() > 0 ? savedPass.c_str() : NULL);

    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 25) {
      delay(500);
      Serial.print(".");
      HC06.print(".");
      timeout++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      digitalWrite(LED_STATUS_PIN, HIGH);
      printlnBoth("\r\n==========================================");
      printlnBoth("✅ [WIFI TERHUBUNG REALTIME]");
      printBoth("   • SSID       : "); printlnBoth(savedSSID);
      printBoth("   • IP Address : "); printlnBoth(WiFi.localIP().toString());
      printBoth("   • Web Server : http://"); printlnBoth(WiFi.localIP().toString());
      printlnBoth("==========================================");
    } else {
      printlnBoth("\r\n⚠️ Gagal terhubung ke WiFi! Mengaktifkan mode Access Point...");
      prefs.begin("geoshield-cfg", false);
      prefs.clear();
      prefs.end();
      delay(1000);
      ESP.restart();
    }
  }

  // Daftarkan Route Web Server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reset", HTTP_GET, handleReset);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.onNotFound(handleRoot); // Captive portal fallback
  server.begin();

  printlnBoth("[SERVER] Web Server Port 80 Siap Beroperasi.");
  printlnBoth("==========================================\r\n");
}

void loop() {
  // 1. DNS Captive Portal (hanya jika di Access Point)
  if (isAPMode) {
    dnsServer.processNextRequest();
    // Kedipkan LED perlahan saat mode AP
    if (millis() - lastBlink > 500) {
      ledState = !ledState;
      digitalWrite(LED_STATUS_PIN, ledState ? HIGH : LOW);
      lastBlink = millis();
    }
  }

  // 2. Layani Request Web Server Realtime
  server.handleClient();

  // 3. Layani Perintah Bluetooth HC-06 & Serial USB
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
