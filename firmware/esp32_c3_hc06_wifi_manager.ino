#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// =========================================================
// 1. HC-06 UART (ESP32-C3)
// =========================================================
#define HC06_RX_PIN 4   // ESP32-C3 RX  <- HC-06 TXD
#define HC06_TX_PIN 5   // ESP32-C3 TX  -> HC-06 RXD

HardwareSerial HC06(1);

// WebServer untuk akses point dari HP / Laptop browser
WebServer apServer(80);
String recentLogs = "";

// =========================================================
// 2. PREFERENCES
// =========================================================
Preferences preferences;

String savedSSID = "";
String savedPASS = "";

// =========================================================
// 3. CONSOLE STATE
// =========================================================
enum ConsoleState {
    CONSOLE_NORMAL,
    CONSOLE_WAIT_SSID,
    CONSOLE_WAIT_PASSWORD
};

ConsoleState consoleState = CONSOLE_NORMAL;

String inputBuffer = "";
String serialInputBuffer = "";

// =========================================================
// 4. WIFI STATE
// Mengikuti template WiFi kamu
// =========================================================
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

// =========================================================
// 5. WIFI STATUS HELPER
// =========================================================
String wifiStatus(wl_status_t s) {

    switch (s) {

        case WL_CONNECTED:
            return "CONNECTED";

        case WL_NO_SSID_AVAIL:
            return "NO SSID";

        case WL_CONNECT_FAILED:
            return "FAILED";

        case WL_DISCONNECTED:
            return "DISCONNECTED";

        case WL_IDLE_STATUS:
            return "IDLE";

        default:
            return String((int)s);
    }
}

// =========================================================
// 6. KIRIM DATA KE HC-06 + SERIAL MONITOR + WEB AP
// =========================================================
void sendHC(String text) {

    HC06.println(text);

    Serial.println(text);

    recentLogs += text + "\n";
    if (recentLogs.length() > 4000) {
        recentLogs = recentLogs.substring(recentLogs.length() - 2500);
    }
}

// =========================================================
// 7. LOAD WIFI DARI MEMORY
// =========================================================
void loadWiFiConfig() {

    preferences.begin("wifi_cfg", true);

    savedSSID = preferences.getString("ssid", "");
    savedPASS = preferences.getString("pass", "");

    preferences.end();

    Serial.println();
    Serial.println("[CONFIG] Membaca konfigurasi WiFi...");

    if (savedSSID.length() > 0) {

        Serial.println("[CONFIG] SSID ditemukan: " + savedSSID);

    } else {

        Serial.println("[CONFIG] Belum ada WiFi tersimpan.");

    }
}

// =========================================================
// 8. SAVE WIFI KE MEMORY
// =========================================================
void saveWiFiConfig(String ssid, String pass) {

    preferences.begin("wifi_cfg", false);

    preferences.putString("ssid", ssid);
    preferences.putString("pass", pass);

    preferences.end();

    savedSSID = ssid;
    savedPASS = pass;

    Serial.println("[CONFIG] WiFi berhasil disimpan.");
}

// =========================================================
// 9. HAPUS WIFI DARI MEMORY
// =========================================================
void clearWiFiConfig() {

    preferences.begin("wifi_cfg", false);

    preferences.clear();

    preferences.end();

    savedSSID = "";
    savedPASS = "";

    sendHC("");
    sendHC("Konfigurasi WiFi telah dihapus.");
    sendHC("ESP32 akan restart...");

    delay(1500);

    ESP.restart();
}

// =========================================================
// 10. START WIFI
// Mengikuti TEMPLATE WIFI KAMU
// =========================================================
void startWifi() {

    if (savedSSID.length() == 0) {

        Serial.println("[WIFI] Tidak ada SSID tersimpan.");

        wifiState = WIFI_IDLE;

        return;
    }

    wifiAttempt++;

    Serial.println();
    Serial.println("[WIFI] Menghubungkan ke: " + savedSSID);

    // =====================================================
    // BAGIAN INI SENGAJA MENGIKUTI TEMPLATE KAMU
    // =====================================================

    WiFi.disconnect(true);

    delay(100);

    WiFi.mode(WIFI_STA);

    WiFi.setSleep(false);

    WiFi.setAutoReconnect(true);

    WiFi.persistent(true);

    // TX power 11 dBm
    WiFi.setTxPower(WIFI_POWER_11dBm);

    // Mulai koneksi
    WiFi.begin(
        savedSSID.c_str(),
        savedPASS.c_str()
    );

    wifiStart = millis();

    dotTimer = millis();

    wifiState = WIFI_CONNECTING;

    Serial.println("[WIFI] Proses koneksi dimulai.");
}

// =========================================================
// 11. UPDATE WIFI
// Pola sama seperti template kamu
// =========================================================
void updateWifi() {

    switch (wifiState) {

        // =================================================
        // IDLE
        // =================================================
        case WIFI_IDLE:

            if (millis() - wifiRetry > 2000) {

                startWifi();
            }

            break;


        // =================================================
        // CONNECTING
        // =================================================
        case WIFI_CONNECTING: {

            wl_status_t status = WiFi.status();

            // ---------------------------------------------
            // BERHASIL
            // ---------------------------------------------
            if (status == WL_CONNECTED) {

                Serial.println();
                Serial.println("[WIFI] TERHUBUNG!");

                Serial.print("[WIFI] SSID: ");
                Serial.println(WiFi.SSID());

                Serial.print("[WIFI] IP: ");
                Serial.println(WiFi.localIP());

                Serial.print("[WIFI] Gateway: ");
                Serial.println(WiFi.gatewayIP());

                Serial.print("[WIFI] RSSI: ");
                Serial.print(WiFi.RSSI());
                Serial.println(" dBm");

                wifiState = WIFI_CONNECTED;

                wifiAttempt = 0;

                sendHC("");
                sendHC("================================");
                sendHC("        WIFI TERHUBUNG");
                sendHC("================================");
                sendHC("[WIFI TERHUBUNG]");
                sendHC("SSID : " + WiFi.SSID());
                sendHC("IP   : " + WiFi.localIP().toString());
                sendHC("RSSI : " + String(WiFi.RSSI()) + " dBm");
                sendHC("================================");

                break;
            }

            // ---------------------------------------------
            // DOT PROGRESS
            // ---------------------------------------------
            if (millis() - dotTimer > 500) {

                Serial.print(".");

                dotTimer = millis();
            }

            // ---------------------------------------------
            // TIMEOUT 10 DETIK
            // Sama seperti template kamu
            // ---------------------------------------------
            if (millis() - wifiStart > 10000) {

                Serial.println();

                Serial.println(
                    "[WIFI] Gagal/Timeout: " +
                    wifiStatus(status)
                );

                WiFi.disconnect(true);

                wifiState = WIFI_IDLE;

                wifiRetry = millis();

                sendHC("");
                sendHC("Koneksi WiFi gagal.");
                sendHC("Silakan cek SSID/password.");
                sendHC("");
                sendHC("Ketik: masukan wifi");

            }

            break;
        }


        // =================================================
        // CONNECTED
        // =================================================
        case WIFI_CONNECTED:

            if (WiFi.status() != WL_CONNECTED) {

                Serial.println();
                Serial.println("[WIFI] TERPUTUS!");

                Serial.println(
                    "[WIFI] Mencoba reconnect..."
                );

                wifiState = WIFI_IDLE;

                wifiRetry = millis();

                sendHC("");
                sendHC("WiFi terputus.");
                sendHC("Mencoba reconnect...");
            }

            break;
    }
}

// =========================================================
// 12. SCAN WIFI
// =========================================================
void scanWiFi() {

    sendHC("");
    sendHC("================================");
    sendHC("          SCAN WIFI");
    sendHC("================================");

    sendHC("Scanning WiFi sekitar ESP32...");
    sendHC("Mohon tunggu...");
    sendHC("");

    Serial.println();
    Serial.println("[SCAN] Memulai scan WiFi...");

    // Pastikan mode STA
    WiFi.mode(WIFI_STA);

    delay(100);

    int numberOfNetworks = WiFi.scanNetworks();

    if (numberOfNetworks < 0) {

        sendHC("Scan WiFi gagal.");

        Serial.println("[SCAN] Gagal.");

        WiFi.scanDelete();

        return;
    }

    if (numberOfNetworks == 0) {

        sendHC("Tidak ada WiFi ditemukan.");

        Serial.println("[SCAN] Tidak ada jaringan.");

        WiFi.scanDelete();

        return;
    }

    sendHC(
        "WiFi ditemukan: " +
        String(numberOfNetworks)
    );

    sendHC("");

    Serial.println(
        "[SCAN] Ditemukan " +
        String(numberOfNetworks) +
        " WiFi."
    );

    for (int i = 0; i < numberOfNetworks; i++) {

        String ssid = WiFi.SSID(i);

        int rssi = WiFi.RSSI(i);

        wifi_auth_mode_t encryption =
            WiFi.encryptionType(i);

        String security;

        if (encryption == WIFI_AUTH_OPEN) {

            security = "OPEN";

        } else {

            security = "LOCKED";
        }

        // ---------------------------------------------
        // Kalau SSID kosong
        // ---------------------------------------------
        if (ssid.length() == 0) {

            ssid = "<HIDDEN>";
        }

        String result =
            String(i + 1) +
            ". " +
            ssid +
            " | " +
            String(rssi) +
            " dBm | " +
            security;

        sendHC(result);
    }

    sendHC("");

    sendHC("================================");
    sendHC("Scan selesai.");
    sendHC("================================");

    WiFi.scanDelete();
}

// =========================================================
// 13. SHOW STATUS
// =========================================================
void showStatus() {

    sendHC("");
    sendHC("================================");
    sendHC("          WIFI STATUS");
    sendHC("================================");

    if (WiFi.status() == WL_CONNECTED) {

        sendHC("Status : CONNECTED");
        sendHC("SSID   : " + WiFi.SSID());
        sendHC("IP     : " + WiFi.localIP().toString());
        sendHC(
            "RSSI   : " +
            String(WiFi.RSSI()) +
            " dBm"
        );

        sendHC(
            "Gateway: " +
            WiFi.gatewayIP().toString()
        );

    } else {

        sendHC("Status : DISCONNECTED");

        if (savedSSID.length() > 0) {

            sendHC(
                "Saved SSID: " +
                savedSSID
            );

        } else {

            sendHC("Belum ada WiFi tersimpan.");
        }
    }

    sendHC("================================");
}

// =========================================================
// 14. SHOW MENU
// =========================================================
void showMenu() {

    sendHC("");
    sendHC("================================");
    sendHC("      ESP32-C3 WIFI MANAGER");
    sendHC("================================");

    sendHC("");

    sendHC("Perintah yang tersedia:");

    sendHC("");

    sendHC("scan wifi");
    sendHC("  Scan WiFi sekitar ESP32");

    sendHC("");

    sendHC("masukan wifi");
    sendHC("  Setting SSID dan password");

    sendHC("");

    sendHC("status");
    sendHC("  Melihat status WiFi");

    sendHC("");

    sendHC("hapus wifi");
    sendHC("  Hapus konfigurasi WiFi");

    sendHC("");

    sendHC("help");
    sendHC("  Menampilkan menu");

    sendHC("");

    sendHC("================================");
}

// =========================================================
// 15. MULAI INPUT WIFI
// =========================================================
void startWiFiInput() {

    consoleState = CONSOLE_WAIT_SSID;

    inputBuffer = "";

    sendHC("");
    sendHC("================================");
    sendHC("          SETTING WIFI");
    sendHC("================================");

    sendHC("");

    sendHC("Masukan SSID WiFi:");

    sendHC("");

    Serial.println("[CONFIG] Menunggu SSID...");
}

// =========================================================
// 16. PROCESS SSID
// =========================================================
void processSSID(String ssid) {

    ssid.trim();

    if (ssid.length() == 0) {

        sendHC("SSID tidak boleh kosong.");
        sendHC("");
        sendHC("Masukan SSID lagi:");

        return;
    }

    savedSSID = ssid;

    consoleState = CONSOLE_WAIT_PASSWORD;

    sendHC("");

    sendHC(
        "SSID diterima: " +
        savedSSID
    );

    sendHC("");

    sendHC("Masukan password WiFi:");

    sendHC("");

    Serial.println(
        "[CONFIG] SSID: " +
        savedSSID
    );

    Serial.println(
        "[CONFIG] Menunggu password..."
    );
}

// =========================================================
// 17. PROCESS PASSWORD
// =========================================================
void processPassword(String password) {

    password.trim();

    savedPASS = password;

    sendHC("");

    sendHC("Password diterima.");

    sendHC("");

    sendHC("================================");
    sendHC("       TEST KONEKSI WIFI");
    sendHC("================================");

    sendHC(
        "SSID: " +
        savedSSID
    );

    sendHC("");

    sendHC("Menghubungkan...");

    Serial.println();
    Serial.println("[CONFIG] Test koneksi WiFi.");
    Serial.println(
        "[CONFIG] SSID: " +
        savedSSID
    );

    // =====================================================
    // TEST KONEKSI
    // Menggunakan mekanisme WiFi template
    // =====================================================

    WiFi.disconnect(true);

    delay(100);

    WiFi.mode(WIFI_STA);

    WiFi.setSleep(false);

    WiFi.setAutoReconnect(true);

    WiFi.persistent(true);

    WiFi.setTxPower(WIFI_POWER_11dBm);

    WiFi.begin(
        savedSSID.c_str(),
        savedPASS.c_str()
    );

    unsigned long testStart = millis();

    unsigned long testDot = millis();

    bool connected = false;

    while (millis() - testStart < 15000) {

        if (WiFi.status() == WL_CONNECTED) {

            connected = true;

            break;
        }

        if (millis() - testDot > 500) {

            HC06.print(".");

            Serial.print(".");

            testDot = millis();
        }

        delay(50);
    }

    HC06.println("");

    Serial.println("");

    // =====================================================
    // BERHASIL
    // =====================================================
    if (connected) {

        Serial.println("[CONFIG] WiFi berhasil.");

        sendHC("");
        sendHC("================================");
        sendHC("       WIFI BERHASIL");
        sendHC("================================");
        sendHC("[WIFI TERHUBUNG]");

        sendHC(
            "SSID : " +
            WiFi.SSID()
        );

        sendHC(
            "IP   : " +
            WiFi.localIP().toString()
        );

        sendHC(
            "RSSI : " +
            String(WiFi.RSSI()) +
            " dBm"
        );

        sendHC("");

        sendHC("Menyimpan konfigurasi...");

        saveWiFiConfig(
            savedSSID,
            savedPASS
        );

        sendHC("Konfigurasi tersimpan.");

        sendHC("");

        sendHC(
            "ESP32-C3 akan restart"
        );

        sendHC(
            "dalam 3 detik..."
        );

        Serial.println(
            "[CONFIG] Restart dalam 3 detik."
        );

        delay(3000);

        ESP.restart();

    }

    // =====================================================
    // GAGAL
    // =====================================================
    else {

        Serial.println(
            "[CONFIG] WiFi gagal terhubung."
        );

        sendHC("");
        sendHC("================================");
        sendHC("        WIFI GAGAL");
        sendHC("================================");

        sendHC(
            "Status: " +
            wifiStatus(WiFi.status())
        );

        sendHC("");

        sendHC(
            "SSID atau password salah."
        );

        sendHC("");

        sendHC(
            "Ketik 'masukan wifi'"
        );

        sendHC(
            "untuk mencoba kembali."
        );

        // Kembalikan state console
        consoleState = CONSOLE_NORMAL;

        inputBuffer = "";

        // Kembalikan WiFi ke state normal
        WiFi.disconnect(true);

        delay(100);

        wifiState = WIFI_IDLE;

        wifiRetry = millis();
    }
}

// =========================================================
// 18. PROCESS COMMAND
// =========================================================
void processCommand(String command) {

    command.trim();

    String originalCommand = command;

    command.toLowerCase();

    Serial.println();
    Serial.println(
        "[COMMAND] " +
        originalCommand
    );

    // =====================================================
    // MENUNGGU SSID
    // =====================================================
    if (consoleState == CONSOLE_WAIT_SSID) {

        processSSID(originalCommand);

        return;
    }

    // =====================================================
    // MENUNGGU PASSWORD
    // =====================================================
    if (consoleState == CONSOLE_WAIT_PASSWORD) {

        processPassword(originalCommand);

        return;
    }

    // =====================================================
    // DIRECT SET FORMAT: set:SSID,PASSWORD (dari Web / Serial)
    // =====================================================
    if (command.startsWith("set:")) {
        int commaIdx = originalCommand.indexOf(',');
        if (commaIdx > 4) {
            String newSSID = originalCommand.substring(4, commaIdx);
            String newPass = originalCommand.substring(commaIdx + 1);
            newSSID.trim();
            newPass.trim();
            savedSSID = newSSID;
            processPassword(newPass);
            return;
        }
    }

    // =====================================================
    // COMMAND NORMAL
    // =====================================================

    if (command == "help" || command == "menu") {

        showMenu();
    }

    else if (command == "scan wifi" || command == "scan") {

        scanWiFi();
    }

    else if (command == "masukan wifi") {

        startWiFiInput();
    }

    else if (command == "status") {

        showStatus();
    }

    else if (command == "hapus wifi" || command == "clear" || command == "reset") {

        clearWiFiConfig();
    }

    else {

        sendHC("");

        sendHC(
            "Perintah tidak dikenal:"
        );

        sendHC(
            originalCommand
        );

        sendHC("");

        sendHC(
            "Ketik 'help' untuk melihat"
        );

        sendHC(
            "daftar perintah."
        );
    }
}

// =========================================================
// 19. READ HC-06 & USB SERIAL
// =========================================================
void readHC06() {

    // Baca dari Bluetooth HC-06
    while (HC06.available()) {

        char c = HC06.read();

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            if (inputBuffer.length() > 0) {
                processCommand(inputBuffer);
                inputBuffer = "";
            }
        }
        else {
            inputBuffer += c;
            if (inputBuffer.length() > 100) {
                inputBuffer = "";
                sendHC("Input terlalu panjang.");
            }
        }
    }

    // Baca dari Serial USB (opsional saat debugging di Arduino Serial Monitor)
    while (Serial.available()) {
        char c = Serial.read();

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            if (serialInputBuffer.length() > 0) {
                processCommand(serialInputBuffer);
                serialInputBuffer = "";
            }
        }
        else {
            serialInputBuffer += c;
            if (serialInputBuffer.length() > 100) {
                serialInputBuffer = "";
                Serial.println("Input terlalu panjang.");
            }
        }
    }
}

// =========================================================
// 20. AUTO RECONNECT
// =========================================================
void checkWiFiConnection() {

    if (wifiState != WIFI_CONNECTED) {

        return;
    }

    if (WiFi.status() != WL_CONNECTED) {

        Serial.println();
        Serial.println(
            "[WIFI] Koneksi terputus!"
        );

        Serial.println(
            "[WIFI] Reconnecting..."
        );

        sendHC("");
        sendHC("WiFi terputus.");
        sendHC("Reconnecting...");

        wifiState = WIFI_IDLE;

        wifiRetry = millis();
    }
}

// =========================================================
// 21. SETUP
// =========================================================
void setup() {

    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("==========================================");
    Serial.println("     ESP32-C3 WIFI PROVISIONING");
    Serial.println("            HC-06 MANAGER");
    Serial.println("==========================================");

    Serial.println();

    // =====================================================
    // HC-06 UART
    // =====================================================

    HC06.begin(
        9600,
        SERIAL_8N1,
        HC06_RX_PIN,
        HC06_TX_PIN
    );

    delay(1000);

    Serial.println(
        "[HC06] UART aktif."
    );

    Serial.println(
        "[HC06] RX GPIO: " +
        String(HC06_RX_PIN)
    );

    Serial.println(
        "[HC06] TX GPIO: " +
        String(HC06_TX_PIN)
    );

    // =====================================================
    // LOAD CONFIG
    // =====================================================

    loadWiFiConfig();

    // =====================================================
    // WIFI MODE
    // =====================================================

    WiFi.mode(WIFI_STA);

    WiFi.setSleep(false);

    WiFi.setAutoReconnect(true);

    WiFi.persistent(true);

    WiFi.setTxPower(WIFI_POWER_11dBm);

    // =====================================================
    // JIKA ADA WIFI TERSIMPAN
    // =====================================================

    if (savedSSID.length() > 0) {

        Serial.println();
        Serial.println(
            "[BOOT] WiFi tersimpan ditemukan."
        );

        Serial.println(
            "[BOOT] SSID: " +
            savedSSID
        );

        sendHC("");
        sendHC(
            "WiFi tersimpan ditemukan."
        );

        sendHC(
            "SSID: " +
            savedSSID
        );

        sendHC(
            "Menghubungkan..."
        );

        wifiRetry = millis() - 2000;

        wifiState = WIFI_IDLE;
    }

    // =====================================================
    // BELUM ADA WIFI
    // =====================================================

    else {

        Serial.println();
        Serial.println(
            "[BOOT] Belum ada WiFi."
        );

        Serial.println(
            "[BOOT] Menunggu konfigurasi HC-06."
        );

        sendHC("");
        sendHC("================================");
        sendHC(" ESP32-C3 WIFI PROVISIONING");
        sendHC("================================");

        sendHC("");
        sendHC("Belum ada WiFi tersimpan.");
        sendHC("");

        // =====================================================
        // AKTIFKAN ACCESS POINT SOFTAP (UNTUK HP & LAPTOP BROWSER)
        // =====================================================
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP("ESP32-C3-EWS");
        IPAddress apIP = WiFi.softAPIP();

        sendHC("[AP] Access Point Hotspot Aktif!");
        sendHC("[AP] Nama WiFi : ESP32-C3-EWS");
        sendHC("[AP] IP Gateway: " + apIP.toString());
        sendHC("[AP] (HP / Laptop dapat konek ke WiFi ini)");
        sendHC("");

        sendHC("Ketik 'scan wifi' untuk scan.");
        sendHC("Ketik 'masukan wifi' untuk setting.");
        sendHC("Ketik 'help' untuk menu.");
        sendHC("");

        wifiState = WIFI_IDLE;

        // Jangan konek WiFi sebelum ada konfigurasi
        wifiRetry = millis();
    }

    // =====================================================
    // ROUTE WEBSERVER API (UNTUK WEB DI HP & LAPTOP)
    // =====================================================
    apServer.on("/api/logs", HTTP_GET, []() {
        apServer.sendHeader("Access-Control-Allow-Origin", "*");
        apServer.send(200, "text/plain", recentLogs);
    });

    apServer.on("/api/cmd", HTTP_GET, []() {
        apServer.sendHeader("Access-Control-Allow-Origin", "*");
        if (apServer.hasArg("q")) {
            String cmd = apServer.arg("q");
            processCommand(cmd);
            apServer.send(200, "text/plain", "OK");
        } else {
            apServer.send(400, "text/plain", "Missing cmd param");
        }
    });

    apServer.on("/api/status", HTTP_GET, []() {
        apServer.sendHeader("Access-Control-Allow-Origin", "*");
        String json = "{\"status\":\"" + wifiStatus(WiFi.status()) + "\",";
        json += "\"ssid\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.SSID() : savedSSID) + "\",";
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI()) + "}";
        apServer.send(200, "application/json", json);
    });

    apServer.begin();
    Serial.println("[WEBSERVER] Web API aktif di port 80.");

    Serial.println();
    Serial.println(
        "[SYSTEM] ESP32-C3 READY."
    );
}

// =========================================================
// 22. LOOP
// =========================================================
void loop() {

    // =====================================================
    // Handle WebServer Client (HP / Browser)
    // =====================================================
    apServer.handleClient();

    // =====================================================
    // Baca command dari HC-06 & USB
    // =====================================================

    readHC06();

    // =====================================================
    // Update koneksi WiFi
    // =====================================================

    if (savedSSID.length() > 0) {

        updateWifi();

        checkWiFiConnection();
    }

    // =====================================================
    // Beri waktu sistem
    // =====================================================

    yield();

    delay(1);
}
