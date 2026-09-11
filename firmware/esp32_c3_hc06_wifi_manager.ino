/*
 * ==============================================================================
 * Project: ESP32 / ESP32-C3 + HC-06 Bluetooth Interactive WiFi Manager (V2.0)
 * Author: GeoShield EWS Open Source
 * Description:
 *   Sistem menu login interaktif via Bluetooth HC-06 yang dioptimalkan untuk HP.
 *   Mendukung deteksi otomatis tanpa harus tekan enter (Timeout buffer & Auto-keyword),
 *   dukungan aplikasi Serial Bluetooth Terminal di Android/iOS, serta penyimpanan
 *   permanen SSID & Password di memori internal ESP32 (NVS Flash Preferences).
 *
 * Pinout HC-06 ke ESP32 / ESP32-C3:
 *   HC-06 TX  -->  ESP32 RX (GPIO 4)
 *   HC-06 RX  -->  ESP32 TX (GPIO 5)
 *   HC-06 VCC -->  5V / 3.3V
 *   HC-06 GND -->  GND
 * ==============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

#define HC06_RX 4  // Pin RX ESP32 (dihubungkan ke TX HC-06)
#define HC06_TX 5  // Pin TX ESP32 (dihubungkan ke RX HC-06)

// Gunakan Serial1 bawaan ESP32 untuk komunikasi HC-06
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  HardwareSerial HC06(0); // ESP32-C3 Secondary UART
#else
  HardwareSerial HC06(1); // ESP32 Standard UART1
#endif

Preferences preferences;

// State Machine Alur Login
enum SetupState {
  STATE_NORMAL,
  STATE_MENU,
  STATE_INPUT_SSID,
  STATE_INPUT_PASS
};

SetupState currentState = STATE_NORMAL;

String inputBuffer = "";
unsigned long lastCharTime = 0;
const unsigned long BUFFER_TIMEOUT_MS = 250; // Auto-process jika HP tidak mengirim newline (Enter)

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
void printWiFiStatus();

void setup() {
  // 1. Inisialisasi USB Serial Komputer
  Serial.begin(115200);
  delay(1000);

  // 2. Inisialisasi UART HC-06 Bluetooth
  pinMode(HC06_RX, INPUT_PULLUP);
  pinMode(HC06_TX, OUTPUT);
  HC06.begin(9600, SERIAL_8N1, HC06_RX, HC06_TX);

  delay(500);

  printlnBoth("\r\n==========================================");
  printlnBoth("🚀 ESP32 + HC-06 BLUETOOTH AKTIF!");
  printlnBoth("==========================================");
  printlnBoth("[INFO] RX ESP32 = GPIO 4 | TX ESP32 = GPIO 5");
  printlnBoth("[INFO] Baudrate = 9600 bps");
  printlnBoth("==========================================");
  printlnBoth("👉 Ketik 'masukan wifi' atau 'menu' di HP Anda");
  printlnBoth("==========================================\r\n");

  // Baca kredensial WiFi tersimpan
  loadSavedWiFi();
}

void loop() {
  // 1. Baca data dari Bluetooth HC-06 (HP)
  while (HC06.available()) {
    char c = (char)HC06.read();
    Serial.write(c); // Echo ke Serial Monitor Komputer
    lastCharTime = millis();

    if (c == '\r' || c == '\n') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }

  // 2. Baca data dari USB Serial Komputer
  while (Serial.available()) {
    char c = (char)Serial.read();
    HC06.write(c); // Echo ke HP via Bluetooth
    lastCharTime = millis();

    if (c == '\r' || c == '\n') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }

  // 3. Auto-process jika pengguna di HP mengirim teks tanpa menekan tombol Enter (Timeout)
  if (inputBuffer.length() > 0 && (millis() - lastCharTime > BUFFER_TIMEOUT_MS)) {
    processCommand(inputBuffer);
    inputBuffer = "";
  }

  delay(10);
}

// ==========================================
// PENGOLAHAN COMMAND & FORM LOGIN
// ==========================================
void processCommand(String input) {
  input.trim();
  if (input.length() == 0) return;

  String lower = input;
  lower.toLowerCase();

  switch (currentState) {
    case STATE_NORMAL: {
      // Deteksi fleksibel kata kunci perintah
      if (lower.indexOf("wifi") != -1 || lower.indexOf("menu") != -1 || 
          lower.indexOf("masukan") != -1 || lower.indexOf("login") != -1 || 
          lower.indexOf("set") != -1 || lower == "1" || lower == "help") {
        showMainMenu();
      } else if (lower.startsWith("set:")) {
        // Format Pintas Cepat: set:NamaWiFi,PasswordWiFi
        int commaIndex = input.indexOf(',');
        if (commaIndex != -1) {
          String s = input.substring(4, commaIndex);
          String p = input.substring(commaIndex + 1);
          s.trim();
          p.trim();
          connectWiFi(s, p);
        } else {
          printlnBoth("\r\n[ERROR] Format salah! Gunakan: set:NamaWiFi,PasswordWiFi\r\n");
        }
      } else if (lower == "status") {
        printWiFiStatus();
      } else if (lower == "scan") {
        scanNearbyWiFi();
      } else {
        printlnBoth("\r\n[?] Perintah diterima: \"" + input + "\"");
        printlnBoth("💡 Ketik 'masukan wifi' atau 'menu' untuk membuka menu pengaturan.\r\n");
      }
      break;
    }

    case STATE_MENU: {
      if (input == "1" || lower.indexOf("scan") != -1) {
        scanNearbyWiFi();
        printlnBoth("\r\n👉 Ketik [2] untuk memasukkan WiFi atau [5] untuk keluar.");
      } else if (input == "2" || lower.indexOf("masuk") != -1 || lower.indexOf("wifi") != -1 || lower.indexOf("login") != -1) {
        currentState = STATE_INPUT_SSID;
        printlnBoth("\r\n------------------------------------------");
        printlnBoth("🔑 [HALAMAN LOGIN WIFI]");
        printlnBoth("------------------------------------------");
        printlnBoth("Langkah 1/2: Masukkan Nama WiFi (SSID):");
        printBoth("SSID -> ");
      } else if (input == "3" || lower.indexOf("status") != -1) {
        printWiFiStatus();
        showMainMenu();
      } else if (input == "4" || lower.indexOf("hapus") != -1 || lower.indexOf("reset") != -1) {
        preferences.begin("wifi-config", false);
        preferences.clear();
        preferences.end();
        printlnBoth("\r\n[SUCCESS] Kredensial WiFi berhasil dihapus dari memori ESP32!");
        showMainMenu();
      } else if (input == "5" || lower.indexOf("keluar") != -1 || lower.indexOf("exit") != -1) {
        currentState = STATE_NORMAL;
        printlnBoth("\r\n[INFO] Keluar dari menu setup WiFi.");
      } else {
        printlnBoth("[!] Pilihan tidak valid. Silakan ketik angka 1 sampai 5.");
      }
      break;
    }

    case STATE_INPUT_SSID: {
      tempSSID = input;
      currentState = STATE_INPUT_PASS;
      printlnBoth("\r\n✅ [OK] Nama WiFi tersimpan: \"" + tempSSID + "\"");
      printlnBoth("Langkah 2/2: Masukkan Password WiFi (Ketik 'none' jika tanpa password):");
      printBoth("Password -> ");
      break;
    }

    case STATE_INPUT_PASS: {
      tempPass = input;
      if (tempPass.equalsIgnoreCase("none") || tempPass.equalsIgnoreCase("kosong") || tempPass == "-") {
        tempPass = "";
      }

      printlnBoth("\r\n==========================================");
      printlnBoth("📋 KONFIRMASI DATA LOGIN:");
      printlnBoth("   • SSID     : " + tempSSID);
      printlnBoth("   • Password : " + (tempPass.length() > 0 ? "******** (" + String(tempPass.length()) + " karakter)" : "[Tanpa Password]"));
      printlnBoth("==========================================");
      printlnBoth("⏳ Menyimpan ke memori ESP32 & Menghubungkan...");

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
  printlnBoth("\r\n==========================================");
  printlnBoth("       📶 MENU PENGATURAN WIFI ESP32     ");
  printlnBoth("==========================================");
  printlnBoth(" [1] 🔍 Scan Daftar WiFi Sekitar");
  printlnBoth(" [2] 🔑 Masukkan Nama WiFi & Password");
  printlnBoth(" [3] 📊 Cek Status Koneksi Saat Ini");
  printlnBoth(" [4] 🗑️  Hapus WiFi Tersimpan");
  printlnBoth(" [5] 🚪 Keluar dari Menu");
  printlnBoth("==========================================");
  printBoth("Ketik angka pilihan (1-5) -> ");
}

// ==========================================
// FUNGSI SCANNING WIFI SEKITAR
// ==========================================
void scanNearbyWiFi() {
  printlnBoth("\r\n🔍 Sedang memindai jaringan WiFi sekitar...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  if (n == 0) {
    printlnBoth("[!] Tidak ada jaringan WiFi yang terdeteksi.");
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
// FUNGSI HUBUNGKAN KE WIFI & SIMPAN KE FLASH
// ==========================================
void connectWiFi(String ssid, String pass) {
  printlnBoth("\r\n[WiFi] Menghubungkan ke: " + ssid + " ...");
  
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
    printlnBoth("\r\n\r\n==========================================");
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
    printlnBoth("💾 [MEMORI] Kredensial WiFi tersimpan di ESP32!\r\n");
  } else {
    printlnBoth("\r\n\r\n❌ [LOGIN GAGAL] Tidak dapat terhubung ke WiFi.");
    printlnBoth("   Periksa apakah SSID dan Password sudah benar.");
    printlnBoth("   Ketik 'menu' untuk mencoba kembali.\r\n");
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
    printlnBoth("[MEMORI] Ditemukan WiFi tersimpan: " + savedSSID);
    printlnBoth("[MEMORI] Menghubungkan otomatis...");
    connectWiFi(savedSSID, savedPass);
  } else {
    printlnBoth("[MEMORI] Belum ada konfigurasi WiFi tersimpan.");
    printlnBoth("💡 Ketik 'masukan wifi' via Bluetooth untuk login.\r\n");
  }
}

// ==========================================
// CEK STATUS KONEKSI
// ==========================================
void printWiFiStatus() {
  printlnBoth("\r\n------------------------------------------");
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
  printlnBoth("------------------------------------------\r\n");
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
