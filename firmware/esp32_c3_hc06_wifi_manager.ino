/*
 * ==============================================================================
 * Project: ESP32 / ESP32-C3 + HC-06 Bluetooth Interactive WiFi Manager
 * Author: GeoShield EWS Open Source
 * Description:
 *   Sistem menu interaktif melalui Bluetooth HC-06 (atau Serial Monitor)
 *   untuk memasukkan Nama WiFi (SSID) dan Password seperti halaman login,
 *   serta menyimpan kredensial ke memori Flash NVS ESP32 (Preferences).
 *
 * Pinout HC-06 ke ESP32 / ESP32-C3:
 *   HC-06 TX  -->  ESP32 RX (GPIO 4)
 *   HC-06 RX  -->  ESP32 TX (GPIO 5) (Gunakan resistor divider jika perlu)
 *   HC-06 VCC -->  5V / 3.3V
 *   HC-06 GND -->  GND
 * ==============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

#define HC06_RX 4  // Pin RX ESP32 menerima data dari TX HC-06
#define HC06_TX 5  // Pin TX ESP32 mengirim data ke RX HC-06

HardwareSerial HC06(1);
Preferences preferences;

// State Machine Menu Interaktif
enum SetupState {
  STATE_NORMAL,
  STATE_MENU,
  STATE_INPUT_SSID,
  STATE_INPUT_PASS
};

SetupState currentState = STATE_NORMAL;

String inputBuffer = "";
String tempSSID = "";
String tempPass = "";

// Prototipe Fungsi
void printBoth(String msg);
void printlnBoth(String msg);
void showMainMenu();
void scanNearbyWiFi();
void connectWiFi(String ssid, String pass);
void loadSavedWiFi();
void processCommand(String input);

void setup() {
  Serial.begin(115200);
  delay(1500);

  printlnBoth("\n==========================================");
  printlnBoth("🚀 ESP32 + HC-06 BLUETOOTH WIFI MANAGER");
  printlnBoth("==========================================");

  // Inisialisasi UART HC-06 Bluetooth
  HC06.begin(9600, SERIAL_8N1, HC06_RX, HC06_TX);
  printlnBoth("[INFO] HC-06 Bluetooth Aktif (Baudrate: 9600)");
  printlnBoth("[INFO] Ketik 'menu' atau 'masukan wifi' via Bluetooth");
  printlnBoth("==========================================\n");

  // Baca kredensial WiFi yang tersimpan di NVS Flash
  loadSavedWiFi();
}

void loop() {
  // 1. Baca Input dari Bluetooth HC-06
  while (HC06.available()) {
    char c = (char)HC06.read();
    Serial.write(c); // Echo ke USB Serial

    if (c == '\r' || c == '\n') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }

  // 2. Baca Input dari USB Serial Monitor
  while (Serial.available()) {
    char c = (char)Serial.read();
    HC06.write(c); // Echo ke Bluetooth

    if (c == '\r' || c == '\n') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }

  delay(10);
}

// ==========================================
// PENGOLAHAN COMMAND & STATE LOGIN
// ==========================================
void processCommand(String input) {
  input.trim();
  if (input.length() == 0) return;

  switch (currentState) {
    case STATE_NORMAL: {
      String lower = input;
      lower.toLowerCase();

      // Trigger masuk ke menu login / setup
      if (lower == "menu" || lower == "wifi" || lower == "masukan wifi" || 
          lower == "set wifi" || lower == "login" || lower == "help" || lower == "setup" || lower == "1") {
        showMainMenu();
      } else if (lower.startsWith("set:")) {
        // Format pintas langsung: set:NamaWiFi,PasswordWiFi
        int commaIndex = input.indexOf(',');
        if (commaIndex != -1) {
          String s = input.substring(4, commaIndex);
          String p = input.substring(commaIndex + 1);
          s.trim();
          p.trim();
          connectWiFi(s, p);
        } else {
          printlnBoth("\n[ERROR] Format salah! Gunakan: set:NamaWiFi,PasswordWiFi\n");
        }
      } else if (lower == "status") {
        printWiFiStatus();
      } else if (lower == "scan") {
        scanNearbyWiFi();
      } else {
        printlnBoth("\n[?] Perintah tidak dikenal: '" + input + "'");
        printlnBoth("💡 Ketik 'menu' atau 'masukan wifi' untuk membuka menu pengaturan WiFi.\n");
      }
      break;
    }

    case STATE_MENU: {
      if (input == "1") {
        scanNearbyWiFi();
        printlnBoth("\n👉 Masukkan [2] untuk memasukkan WiFi atau [5] untuk keluar.");
      } else if (input == "2" || input.equalsIgnoreCase("masukan wifi") || input.equalsIgnoreCase("set wifi")) {
        currentState = STATE_INPUT_SSID;
        printlnBoth("\n------------------------------------------");
        printlnBoth("🔑 [HALAMAN LOGIN WIFI]");
        printlnBoth("------------------------------------------");
        printlnBoth("Silakan masukkan Nama WiFi (SSID):");
        printBoth("SSID -> ");
      } else if (input == "3") {
        printWiFiStatus();
        showMainMenu();
      } else if (input == "4") {
        // Hapus kredensial
        preferences.begin("wifi-config", false);
        preferences.clear();
        preferences.end();
        printlnBoth("\n[SUCCESS] Kredensial WiFi berhasil dihapus dari memori!");
        showMainMenu();
      } else if (input == "5" || input.equalsIgnoreCase("exit") || input.equalsIgnoreCase("keluar")) {
        currentState = STATE_NORMAL;
        printlnBoth("\n[INFO] Keluar dari menu setup WiFi. Mode monitoring normal aktif.");
      } else {
        printlnBoth("[!] Pilihan tidak valid. Silakan pilih 1 - 5.");
      }
      break;
    }

    case STATE_INPUT_SSID: {
      tempSSID = input;
      currentState = STATE_INPUT_PASS;
      printlnBoth("\n[OK] SSID tersimpan: \"" + tempSSID + "\"");
      printlnBoth("Silakan masukkan Password WiFi (Kosongkan/ketik 'none' jika tanpa password):");
      printBoth("Password -> ");
      break;
    }

    case STATE_INPUT_PASS: {
      tempPass = input;
      if (tempPass.equalsIgnoreCase("none") || tempPass.equalsIgnoreCase("kosong")) {
        tempPass = "";
      }

      printlnBoth("\n------------------------------------------");
      printlnBoth("📋 KONFIRMASI DATA LOGIN:");
      printlnBoth("   • SSID     : " + tempSSID);
      printlnBoth("   • Password : " + (tempPass.length() > 0 ? "******** (" + String(tempPass.length()) + " karakter)" : "[Tanpa Password]"));
      printlnBoth("------------------------------------------");
      printlnBoth("⏳ Menyimpan ke memori & mencoba menghubungkan...");

      // Coba hubungkan dan simpan jika berhasil
      connectWiFi(tempSSID, tempPass);
      currentState = STATE_NORMAL;
      break;
    }
  }
}

// ==========================================
// TAMPILAN MENU UTAMA (BLUETOOTH CLI)
// ==========================================
void showMainMenu() {
  currentState = STATE_MENU;
  printlnBoth("\n==========================================");
  printlnBoth("       📶 MENU PENGATURAN WIFI ESP32     ");
  printlnBoth("==========================================");
  printlnBoth(" [1] 🔍 Scan Daftar Jaringan WiFi Sekitar");
  printlnBoth(" [2] 🔑 Masukkan Nama WiFi & Password");
  printlnBoth(" [3] 📊 Cek Status Koneksi Saat Ini");
  printlnBoth(" [4] 🗑️  Hapus Riwayat WiFi Tersimpan");
  printlnBoth(" [5] 🚪 Keluar dari Menu");
  printlnBoth("==========================================");
  printBoth("Pilih opsi (1-5) -> ");
}

// ==========================================
// FUNGSI SCANNING WIFI SEKITAR
// ==========================================
void scanNearbyWiFi() {
  printlnBoth("\n🔍 Sedang memindai jaringan WiFi sekitar...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  if (n == 0) {
    printlnBoth("[!] Tidak ada jaringan WiFi yang ditemukan.");
  } else {
    printlnBoth("📡 Ditemukan " + String(n) + " jaringan WiFi:");
    for (int i = 0; i < n; ++i) {
      String lock = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "🔓 (Open)" : "🔒 (Terkunci)";
      printlnBoth("   " + String(i + 1) + ". " + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm) " + lock);
      delay(10);
    }
  }
}

// ==========================================
// FUNGSI HUBUNGKAN & SIMPAN KE FLASH (NVS)
// ==========================================
void connectWiFi(String ssid, String pass) {
  printlnBoth("\n[WiFi] Menghubungkan ke: " + ssid + " ...");
  
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  
  if (pass.length() > 0) {
    WiFi.begin(ssid.c_str(), pass.c_str());
  } else {
    WiFi.begin(ssid.c_str());
  }

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 25) {
    delay(500);
    printBoth(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    printlnBoth("\n\n==========================================");
    printlnBoth("✅ [LOGIN BERHASIL] WiFi Terkoneksi!");
    printlnBoth("==========================================");
    printlnBoth("   • SSID       : " + WiFi.SSID());
    printlnBoth("   • IP Address : " + WiFi.localIP().toString());
    printlnBoth("   • Sinyal RSSI: " + String(WiFi.RSSI()) + " dBm");
    printlnBoth("==========================================");

    // Simpan permanen ke NVS Preferences ESP32
    preferences.begin("wifi-config", false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", pass);
    preferences.end();
    printlnBoth("💾 [MEMORI] Kredensial WiFi berhasil disimpan secara permanen!\n");
  } else {
    printlnBoth("\n\n❌ [LOGIN GAGAL] Tidak dapat terhubung ke WiFi.");
    printlnBoth("   Pastikan SSID dan Password yang dimasukkan sudah benar.");
    printlnBoth("   Ketik 'menu' untuk mencoba kembali.\n");
  }
}

// ==========================================
// BACA KREDENSIAL DARI MEMORI SAAT BOOT
// ==========================================
void loadSavedWiFi() {
  preferences.begin("wifi-config", true);
  String savedSSID = preferences.getString("ssid", "");
  String savedPass = preferences.getString("pass", "");
  preferences.end();

  if (savedSSID.length() > 0) {
    printlnBoth("[MEMORI] Ditemukan profil WiFi tersimpan: " + savedSSID);
    printlnBoth("[MEMORI] Mencoba auto-connect...");
    connectWiFi(savedSSID, savedPass);
  } else {
    printlnBoth("[MEMORI] Belum ada WiFi yang tersimpan.");
    printlnBoth("💡 Kirim pesan 'masukan wifi' via Bluetooth untuk login.\n");
  }
}

// ==========================================
// CEK STATUS KONEKSI
// ==========================================
void printWiFiStatus() {
  printlnBoth("\n------------------------------------------");
  printlnBoth("📊 STATUS KONEKSI WIFI SAAT INI:");
  if (WiFi.status() == WL_CONNECTED) {
    printlnBoth("   • Status     : TERKONEKSI (ONLINE) 🟢");
    printlnBoth("   • SSID       : " + WiFi.SSID());
    printlnBoth("   • IP Address : " + WiFi.localIP().toString());
    printlnBoth("   • Sinyal RSSI: " + String(WiFi.RSSI()) + " dBm");
    printlnBoth("   • Gateway    : " + WiFi.gatewayIP().toString());
  } else {
    printlnBoth("   • Status     : TERPUTUS (OFFLINE) 🔴");
    printlnBoth("   • Info       : Belum terhubung ke WiFi manapun.");
  }
  printlnBoth("------------------------------------------\n");
}

// ==========================================
// HELPER DUAL OUTPUT (SERIAL + BLUETOOTH)
// ==========================================
void printBoth(String msg) {
  Serial.print(msg);
  HC06.print(msg);
}

void printlnBoth(String msg) {
  Serial.println(msg);
  HC06.println(msg);
}
