#include <WiFi.h>
#include <Preferences.h>
#include <WebServer.h>
#include <HTTPClient.h>

// =====================================================
// KONFIGURASI WEB BACKEND & CLOUD TELEMETRI
// =====================================================
// Alamat endpoint REST API backend Django / Web Server
// Ganti IP dengan IP laptop/server jaringan Anda (contoh: http://192.168.1.10:8000/api/telemetry/)
String webServerUrl = "http://192.168.1.100:8000/api/telemetry/";

// Timer pengiriman telemetri otomatis ke Web Backend
unsigned long lastTelemetrySend = 0;
const unsigned long telemetryInterval = 3000; // Kirim data setiap 3 detik

// =====================================================
// LOCAL WEBSERVER ESP32 (PORT 80)
// Untuk komunikasi langsung dengan browser (HP / Laptop)
// =====================================================
WebServer server(80);
String webLogs = "";

// =====================================================
// HC-06 UART PIN (ESP32-C3)
// =====================================================
#define HC06_RX_PIN 4
#define HC06_TX_PIN 5

HardwareSerial HC06(1);

// =====================================================
// WIFI STORAGE (PREFERENCES FLASH)
// =====================================================
Preferences preferences;

String ssid = "";
String password = "";

// =====================================================
// WIFI STATE
// =====================================================
enum WifiState {
  WIFI_IDLE,
  WIFI_CONNECTING,
  WIFI_CONNECTED
};

WifiState wifiState = WIFI_IDLE;

unsigned long wifiStart = 0;
unsigned long wifiRetry = 0;
unsigned long dotTimer = 0;

// =====================================================
// FUNGSI LOGGING TERPADU (SERIAL + HC-06 + WEB API)
// =====================================================
void logMessage(String msg)
{
  Serial.println(msg);
  HC06.println(msg);

  webLogs += msg + "\n";
  if (webLogs.length() > 4000)
  {
    webLogs = webLogs.substring(webLogs.length() - 2500);
  }
}

// =====================================================
// BACA WIFI DARI FLASH
// =====================================================
bool loadWiFi()
{
  preferences.begin("wifi_cfg", true);

  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");

  preferences.end();

  if (ssid.length() > 0)
  {
    Serial.println();
    logMessage("[WIFI] Konfigurasi ditemukan");
    logMessage("[WIFI] SSID: " + ssid);

    return true;
  }

  Serial.println();
  logMessage("[WIFI] Belum ada konfigurasi tersimpan.");

  return false;
}

// =====================================================
// SIMPAN WIFI KE FLASH
// =====================================================
void saveWiFi(String newSSID, String newPassword)
{
  preferences.begin("wifi_cfg", false);

  preferences.putString("ssid", newSSID);
  preferences.putString("password", newPassword);

  preferences.end();

  ssid = newSSID;
  password = newPassword;

  Serial.println();
  logMessage("[WIFI] Konfigurasi berhasil disimpan");
}

// =====================================================
// HAPUS WIFI DARI FLASH
// =====================================================
void clearWiFi()
{
  preferences.begin("wifi_cfg", false);
  preferences.clear();
  preferences.end();

  ssid = "";
  password = "";

  logMessage("[WIFI] Konfigurasi WiFi telah dihapus!");
  logMessage("[ESP32] Restart sistem...");
  delay(1500);
  ESP.restart();
}

// =====================================================
// MEMBANGUN JSON TELEMETRI SENSOR REALTIME
// =====================================================
String buildTelemetryJson()
{
  // Simulasi pembacaan parameter telemetri (dapat dihubungkan ke sensor MPU6050/Ultrasonic/Rain/TDS fisik)
  float pga = 0.002 + ((float)(esp_random() % 15) / 10000.0);
  float gal = pga * 980.665;
  float waterLevel = 42.5 + ((float)(esp_random() % 20) / 10.0);
  int rainRaw = 4095 - (esp_random() % 100);
  float rainRate = 0.0;
  float tdsPpm = 145.0 + (esp_random() % 10);

  String json = "{";
  json += "\"station_id\":\"ST-01-ESP32\",";
  json += "\"station_name\":\"Pendeteksi Gempa EWS ITERA - ESP32 C3\",";
  json += "\"location\":{\"lat\":-5.35824,\"lng\":105.31465},";
  json += "\"seismic\":{";
  json += "\"pga\":" + String(pga, 4) + ",";
  json += "\"gal\":" + String(gal, 2) + ",";
  json += "\"freq\":0.2,";
  json += "\"status\":\"AMAN\"";
  json += "},";
  json += "\"flood\":{";
  json += "\"waterLevelCm\":" + String(waterLevel, 1) + ",";
  json += "\"status\":\"NORMAL\"";
  json += "},";
  json += "\"rain\":{";
  json += "\"rawAnalog\":" + String(rainRaw) + ",";
  json += "\"rateMmh\":" + String(rainRate, 1) + ",";
  json += "\"status\":\"CERAH\"";
  json += "},";
  json += "\"tds\":{";
  json += "\"ppm\":" + String(tdsPpm, 1) + ",";
  json += "\"status\":\"BERSIH\"";
  json += "}";
  json += "}";

  return json;
}

// =====================================================
// PENGIRIMAN TELEMETRI KE SERVER WEB (DJANGO REST API)
// =====================================================
void sendTelemetryToWeb()
{
  if (WiFi.status() != WL_CONNECTED) return;

  if (millis() - lastTelemetrySend < telemetryInterval) return;
  lastTelemetrySend = millis();

  HTTPClient http;
  http.begin(webServerUrl);
  http.addHeader("Content-Type", "application/json");

  String payload = buildTelemetryJson();
  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0)
  {
    Serial.print("[WEB HTTP POST] Response code: ");
    Serial.println(httpResponseCode);
  }
  else
  {
    Serial.print("[WEB HTTP POST] Gagal kirim ke backend: ");
    Serial.println(http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}

// =====================================================
// PEMROSES PERINTAH (DARI HC-06 MAUPUN WEB API /CMD)
// =====================================================
void processCommand(String cmd)
{
  cmd.trim();
  if (cmd.length() == 0) return;

  logMessage("[CMD] Diterima: " + cmd);

  // Perintah set kredensial WiFi: set:SSID,PASSWORD
  if (cmd.startsWith("set:"))
  {
    String params = cmd.substring(4);
    int commaIndex = params.indexOf(',');
    if (commaIndex > 0)
    {
      String newSSID = params.substring(0, commaIndex);
      String newPass = params.substring(commaIndex + 1);
      newSSID.trim();
      newPass.trim();

      saveWiFi(newSSID, newPass);
      logMessage("[WIFI] Konfigurasi diterima via perintah!");
      logMessage("[WIFI] Menyambungkan ulang...");
      delay(1000);
      ESP.restart();
    }
    else
    {
      logMessage("[CMD ERROR] Format perintah: set:SSID,PASSWORD");
    }
  }
  else if (cmd.equalsIgnoreCase("status"))
  {
    logMessage("=== STATUS ESP32 ===");
    logMessage("WiFi: " + String(WiFi.status() == WL_CONNECTED ? "TERHUBUNG" : "TERPUTUS"));
    logMessage("SSID: " + (WiFi.status() == WL_CONNECTED ? WiFi.SSID() : ssid));
    logMessage("IP  : " + WiFi.localIP().toString());
    logMessage("RSSI: " + String(WiFi.RSSI()) + " dBm");
  }
  else if (cmd.equalsIgnoreCase("hapus wifi") || cmd.equalsIgnoreCase("clear"))
  {
    clearWiFi();
  }
  else if (cmd.equalsIgnoreCase("restart"))
  {
    logMessage("[ESP32] Restarting...");
    delay(1000);
    ESP.restart();
  }
  else
  {
    logMessage("[CMD] Perintah tidak dikenali: " + cmd);
    logMessage("Gunakan: status | set:SSID,PASS | hapus wifi | restart");
  }
}

// =====================================================
// INISIALISASI REST API WEBSERVER (PORT 80)
// =====================================================
void setupWebServer()
{
  // 1. Endpoint Home / Informasi Sistem
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String html = "<h1>GeoShield EWS - ESP32-C3 Web Gateway</h1>";
    html += "<p>Status: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "</p>";
    html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
    html += "<p>API Endpoints: /api/status, /api/telemetry, /api/logs, /api/cmd</p>";
    server.send(200, "text/html", html);
  });

  // 2. Endpoint Status WiFi & Hardware untuk Dashboard Web
  server.on("/api/status", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String json = "{";
    json += "\"status\":\"" + String(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED") + "\",";
    json += "\"ssid\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.SSID() : ssid) + "\",";
    json += "\"ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"uptime_ms\":" + String(millis()) + ",";
    json += "\"backend_url\":\"" + webServerUrl + "\"";
    json += "}";
    server.send(200, "application/json", json);
  });

  // 3. Endpoint Telemetri Sensor Real-Time (Dapat di-polling langsung oleh Web)
  server.on("/api/telemetry", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", buildTelemetryJson());
  });

  server.on("/api/data", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", buildTelemetryJson());
  });

  // 4. Endpoint Logs untuk Terminal Web Provisioning
  server.on("/api/logs", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", webLogs);
  });

  // 5. Endpoint Eksekusi Perintah dari Web (/api/cmd?q=...)
  server.on("/api/cmd", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    if (server.hasArg("q"))
    {
      String cmd = server.arg("q");
      processCommand(cmd);
      server.send(200, "text/plain", "OK");
    }
    else
    {
      server.send(400, "text/plain", "Parameter 'q' tidak ditemukan");
    }
  });

  // 6. Endpoint Konfigurasi WiFi dari Web Browser
  server.on("/api/wifi", HTTP_POST, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    if (server.hasArg("ssid") && server.hasArg("password"))
    {
      String newSSID = server.arg("ssid");
      String newPass = server.arg("password");
      saveWiFi(newSSID, newPass);
      server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"WiFi tersimpan, ESP32 restart...\"}");
      delay(1500);
      ESP.restart();
    }
    else
    {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"ssid & password required\"}");
    }
  });

  // Handle CORS preflight request
  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS)
    {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
      server.sendHeader("Access-Control-Allow-Headers", "*");
      server.send(204);
    }
    else
    {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(404, "text/plain", "Not Found");
    }
  });

  server.begin();
  logMessage("[WEBSERVER] REST API Web aktif di port 80");
}

// =====================================================
// SETTING WIFI MELALUI HC-06
// =====================================================
void wifiSetup()
{
  Serial.println();
  Serial.println("================================");
  Serial.println("      WIFI SETUP HC-06 / WEB");
  Serial.println("================================");

  HC06.println();
  HC06.println("================================");
  HC06.println("ESP32-C3 WIFI SETUP");
  HC06.println("================================");
  HC06.println("MASUKKAN SSID:");

  // Aktifkan juga SoftAP agar web browser HP / Laptop juga bisa mengonfigurasi
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32-C3-EWS");
  IPAddress apIP = WiFi.softAPIP();
  setupWebServer();

  logMessage("[AP] Hotspot Setup Aktif: ESP32-C3-EWS");
  logMessage("[AP] Buka browser di http://" + apIP.toString() + "/");

  // ===================================================
  // MENUNGGU SSID DARI HC-06 ATAU WEB
  // ===================================================
  String newSSID = "";

  while (newSSID.length() == 0)
  {
    server.handleClient();

    if (HC06.available())
    {
      newSSID = HC06.readStringUntil('\n');
      newSSID.trim();

      if (newSSID.length() > 0)
      {
        Serial.print("[HC-06] SSID: ");
        Serial.println(newSSID);
      }
    }

    delay(10);
  }

  HC06.println("SSID diterima.");
  HC06.println("MASUKKAN PASSWORD:");

  // ===================================================
  // MENUNGGU PASSWORD DARI HC-06 ATAU WEB
  // ===================================================
  String newPassword = "";

  while (newPassword.length() == 0)
  {
    server.handleClient();

    if (HC06.available())
    {
      newPassword = HC06.readStringUntil('\n');
      newPassword.trim();

      if (newPassword.length() > 0)
      {
        Serial.println("[HC-06] Password diterima");
      }
    }

    delay(10);
  }

  // ===================================================
  // SIMPAN
  // ===================================================
  saveWiFi(newSSID, newPassword);

  HC06.println();
  HC06.println("KONFIGURASI WIFI BERHASIL");
  HC06.println("ESP32 AKAN RESTART...");
  HC06.println("HC-06 TIDAK DIGUNAKAN LAGI");

  Serial.println();
  Serial.println("[WIFI] Setup selesai");
  Serial.println("[ESP32] Restart...");

  delay(2000);

  ESP.restart();
}

// =====================================================
// MULAI KONEKSI WIFI
// =====================================================
void startWifi()
{
  Serial.println();
  Serial.print("[WIFI] Menghubungkan ke: ");
  Serial.println(ssid);

  WiFi.disconnect(true);

  delay(100);

  WiFi.mode(WIFI_STA);

  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // Sama seperti sistem WiFi kode kamu
  WiFi.setTxPower(WIFI_POWER_11dBm);

  WiFi.begin(
    ssid.c_str(),
    password.c_str()
  );

  wifiStart = millis();
  dotTimer = millis();

  wifiState = WIFI_CONNECTING;
}

// =====================================================
// UPDATE WIFI
// =====================================================
void updateWifi()
{
  switch (wifiState)
  {
    // =================================================
    // IDLE
    // =================================================
    case WIFI_IDLE:

      if (millis() - wifiRetry > 2000)
      {
        startWifi();
      }

      break;

    // =================================================
    // CONNECTING
    // =================================================
    case WIFI_CONNECTING:
    {
      wl_status_t status = WiFi.status();

      if (status == WL_CONNECTED)
      {
        Serial.println();
        Serial.println("================================");
        Serial.println("[WIFI] TERHUBUNG!");
        Serial.println("================================");

        Serial.print("[WIFI] SSID : ");
        Serial.println(WiFi.SSID());

        Serial.print("[WIFI] IP   : ");
        Serial.println(WiFi.localIP());

        Serial.print("[WIFI] RSSI : ");
        Serial.println(WiFi.RSSI());

        logMessage("[WIFI] TERHUBUNG REALTIME!");
        logMessage("[WIFI] IP: " + WiFi.localIP().toString());

        wifiState = WIFI_CONNECTED;
      }
      else
      {
        if (millis() - dotTimer > 500)
        {
          Serial.print(".");
          dotTimer = millis();
        }

        // Timeout 10 detik
        if (millis() - wifiStart > 10000)
        {
          Serial.println();
          Serial.println("[WIFI] Gagal terhubung");

          WiFi.disconnect(true);

          wifiState = WIFI_IDLE;

          wifiRetry = millis();
        }
      }

      break;
    }

    // =================================================
    // CONNECTED
    // =================================================
    case WIFI_CONNECTED:

      if (WiFi.status() != WL_CONNECTED)
      {
        Serial.println();
        Serial.println("[WIFI] Terputus");
        Serial.println("[WIFI] Mencoba reconnect...");

        wifiState = WIFI_IDLE;

        wifiRetry = millis();
      }
      else
      {
        // Jalankan pengiriman data sensor periodik ke Backend Web
        sendTelemetryToWeb();
      }

      break;
  }
}

// =====================================================
// BACA HC-06 DAN SERIAL MONITOR DI DALAM LOOP
// =====================================================
void checkSerialAndHC06()
{
  // Cek pesan/perintah masuk dari HC-06
  if (HC06.available())
  {
    String incoming = HC06.readStringUntil('\n');
    processCommand(incoming);
  }

  // Cek pesan/perintah masuk dari Serial Monitor USB
  if (Serial.available())
  {
    String incoming = Serial.readStringUntil('\n');
    processCommand(incoming);
  }
}

// =====================================================
// SETUP
// =====================================================
void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("========================================");
  Serial.println("      ESP32-C3 WIFI MANAGER");
  Serial.println("     HC-06 & WEB INTEGRATION");
  Serial.println("========================================");

  // ===================================================
  // HC-06 UART
  // ===================================================
  HC06.begin(
    9600,
    SERIAL_8N1,
    HC06_RX_PIN,
    HC06_TX_PIN
  );

  Serial.println("[HC-06] UART aktif");

  // ===================================================
  // BACA KONFIGURASI WIFI
  // ===================================================
  bool wifiExists = loadWiFi();

  // ===================================================
  // BELUM ADA WIFI
  // ===================================================
  if (!wifiExists)
  {
    Serial.println();
    Serial.println("[WIFI] Masuk mode konfigurasi HC-06 & Web AP");

    wifiSetup();
  }

  // ===================================================
  // SUDAH ADA WIFI
  // ===================================================
  wifiRetry = millis() - 2000;

  wifiState = WIFI_IDLE;

  // Jalankan Web Server lokal ESP32 untuk browser
  setupWebServer();
}

// =====================================================
// LOOP
// =====================================================
void loop()
{
  // Melayani permintaan HTTP dari Web browser (HP / Laptop)
  server.handleClient();

  // Membaca perintah dari Bluetooth HC-06 dan Serial
  checkSerialAndHC06();

  // Mengelola koneksi WiFi dan pengiriman telemetri ke Web
  updateWifi();

  delay(1);
}
