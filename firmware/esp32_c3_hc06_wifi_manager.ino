/*
 * ==============================================================================
 * Project: ESP32 / ESP32-C3 + HC-06 Bluetooth Auto-Provisioning & Auto-Restart
 * Author: GeoShield EWS Open Source
 *
 * Alur Kerja Sistem:
 * 1. Saat pertama kali dinyalakan / gagal konek:
 *    - Bluetooth HC-06 AKTIF.
 *    - User kirim 'masukan wifi' atau 'menu' dari HP.
 *    - Masukkan SSID & Password.
 *    - ESP32 menyimpan ke Flash NVS dan otomatis MERESTART (ESP.restart()).
 *
 * 2. Setelah ESP32 Restart:
 *    - Mencoba koneksi ke WiFi dengan konfigurasi TX Power 11dBm (Super Stabil & Cepat).
 *    - JIKA BERHASIL: Bluetooth dimatikan (HC06.end()), ESP32 online normal.
 *    - JIKA GAGAL: Bluetooth dihidupkan kembali, kirim notifikasi gagal ke HP,
 *      dan siap menerima konfigurasi WiFi baru.
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

#define HC06_RX 4  // Pin RX ESP32 <-- TX HC-06
#define HC06_TX 5  // Pin TX ESP32 --> RX HC-06

// Inisialisasi UART HC-06
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  HardwareSerial HC06(0); // ESP32-C3 UART
#else
  HardwareSerial HC06(1); // ESP32 Standard UART1
#endif

Preferences preferences;

// State Machine Menu
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

String savedSSID = "";
String savedPass = "";
String tempSSID = "";
String tempPass = "";

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

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==========================================");
  Serial.println("🚀 ESP32-C3 AUTO-WIFI & BLUETOOTH MANAGER");
  Serial.println("==========================================");

  // 1. Baca kredensial WiFi dari Flash NVS
  preferences.begin("wifi-config", true);
  savedSSID = cleanString(preferences.getString("ssid", ""));
  savedPass = cleanString(preferences.getString("pass", ""));
  preferences.end();

  // 2. Jika ada WiFi tersimpan, coba hubungkan terlebih dahulu
  if (savedSSID.length() > 0) {
    Serial.println("[BOOT] Ditemukan WiFi tersimpan: \"" + savedSSID + "\"");
    Serial.println("[BOOT] Menghubungkan menggunakan driver RF stabil...");
    
    bool connected = tryConnectSavedWiFi();

    if (connected) {
      Serial.println("\n==========================================");
      Serial.println("✅ [SUKSES] ESP32 TERHUBUNG KE WIFI!");
      Serial.println("   • SSID       : " + WiFi.SSID());
      Serial.println("   • IP Address : " + WiFi.localIP().toString());
      Serial.println("   • Status BT  : BLUETOOTH DINONAKTIFKAN (HEMAT DAYA)");
      Serial.println("==========================================\n");
      // Sesuai permintaan: Bluetooth tetap mati saat WiFi sukses
      stopBluetooth();
      return;
    } else {
      Serial.println("\n❌ [GAGAL] Tidak dapat terhubung ke WiFi tersimpan.");
      Serial.println("[INFO] Mengaktifkan kembali Bluetooth HC-06 untuk konfigurasi ulang...\n");
    }
  } else {
    Serial.println("[BOOT] Belum ada konfigurasi WiFi tersimpan.");
    Serial.println("[INFO] Mengaktifkan Bluetooth HC-06...\n");
  }

  // 3. Jika gagal atau belum ada WiFi, hidupkan Bluetooth HC-06
  startBluetooth();
}

void loop() {
  // Jika WiFi sudah terkoneksi normal, loop utama menjalankan tugas
  if (WiFi.status() == WL_CONNECTED) {
    // Mode monitoring normal (Kirim telemetri ke server, dsb)
    delay(100);
    return;
  }

  // JIKA WIFI OFFLINE & BLUETOOTH AKTIF: Layani input user dari HP / Serial
  if (!isBluetoothActive) {
    startBluetooth();
  }

  // 1. Baca dari Bluetooth HC-06 (HP)
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

  // 2. Baca dari USB Serial Komputer
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

  // 3. Auto-process jika tanpa menekan tombol Enter
  if (inputBuffer.length() > 0 && (millis() - lastCharTime > BUFFER_TIMEOUT_MS)) {
    processCommand(cleanString(inputBuffer));
    inputBuffer = "";
  }

  delay(10);
}

// ============================================================
// TEMPLATE KONEKSI WIFI CEPAT & SUPER STABIL (TX POWER 11dBm)
// ============================================================
bool tryConnectSavedWiFi() {
  if (savedSSID.length() == 0) return false;

  Serial.println("\n[WIFI] Memulai koneksi ke: " + savedSSID);
  
  // Format template koneksi WiFi anti-error
  WiFi.disconnect(true);
  delay(40);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.setTxPower(WIFI_POWER_11dBm); // Mencegah drop tegangan pada ESP32-C3

  if (savedPass.length() > 0) {
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
  } else {
    WiFi.begin(savedSSID.c_str());
  }

  unsigned long startAttempt = millis();
  unsigned long dotTimer = millis();

  // Tunggu koneksi hingga 12 detik
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

// ============================================================
// KENDALI BLUETOOTH HC-06 (HIDUP / MATI)
// ============================================================
void startBluetooth() {
  if (isBluetoothActive) return;

  pinMode(HC06_RX, INPUT_PULLUP);
  pinMode(HC06_TX, OUTPUT);
  HC06.begin(9600, SERIAL_8N1, HC06_RX, HC06_TX);
  isBluetoothActive = true;

  delay(200);

  printlnBoth("\r\n==========================================");
  printlnBoth("📶 [BLUETOOTH AKTIF] SIAP MENERIMA WIFI");
  printlnBoth("==========================================");
  if (savedSSID.length() > 0) {
    printlnBoth("⚠️ Status: KONEKSI KE \"" + savedSSID + "\" GAGAL.");
    printlnBoth("👉 Ketik 'masukan wifi' untuk mengganti password/SSID.");
  } else {
    printlnBoth("👉 Ketik 'masukan wifi' atau 'menu' untuk login.");
  }
  printlnBoth("==========================================\r\n");
}

void stopBluetooth() {
  if (!isBluetoothActive) return;
  HC06.flush();
  HC06.end();
  isBluetoothActive = false;
  Serial.println(F("[BLUETOOTH] Modul HC-06 Dinonaktifkan (Hemat Daya & Bebas Interferensi)."));
}

// ============================================================
// PEMBERSIH STRING TERSEMBUNYI (CR/LF/SPASI)
// ============================================================
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

// ============================================================
// PENGOLAHAN COMMAND & LOGIN VIA BLUETOOTH
// ============================================================
void processCommand(String input) {
  input = cleanString(input);
  if (input.length() == 0) return;

  String lower = input;
  lower.toLowerCase();

  switch (currentState) {
    case STATE_NORMAL: {
      if (lower.indexOf("wifi") != -1 || lower.indexOf("menu") != -1 || 
          lower.indexOf("masuk") != -1 || lower.indexOf("login") != -1 || 
          lower.indexOf("set") != -1 || lower == "1" || lower == "help") {
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
      } else if (lower == "restart" || lower == "reboot") {
        printlnBoth("\r\n[INFO] Merestart ESP32...");
        delay(1000);
        ESP.restart();
      } else {
        printlnBoth("\r\n[?] Perintah diterima: \"" + input + "\"");
        printlnBoth("💡 Ketik 'masukan wifi' atau 'menu' untuk membuka form login.\r\n");
      }
      break;
    }

    case STATE_MENU: {
      if (input == "1" || lower.indexOf("masuk") != -1 || lower.indexOf("wifi") != -1 || lower.indexOf("login") != -1) {
        currentState = STATE_INPUT_SSID;
        printlnBoth("\r\n------------------------------------------");
        printlnBoth("🔑 [HALAMAN LOGIN WIFI ESP32]");
        printlnBoth("------------------------------------------");
        printlnBoth("Langkah 1/2: Masukkan Nama WiFi (SSID):");
        printBoth("SSID -> ");
      } else if (input == "2" || lower.indexOf("status") != -1) {
        printlnBoth("\r\n------------------------------------------");
        printlnBoth("📊 STATUS WIFI TERSIMPAN:");
        printlnBoth("   • SSID Tersimpan : " + (savedSSID.length() > 0 ? savedSSID : "[Belum Ada]"));
        printlnBoth("   • Status Koneksi : " + String(WiFi.status() == WL_CONNECTED ? "TERHUBUNG 🟢" : "TERPUTUS 🔴"));
        printlnBoth("------------------------------------------\r\n");
        showMainMenu();
      } else if (input == "3" || lower.indexOf("hapus") != -1 || lower.indexOf("reset") != -1) {
        preferences.begin("wifi-config", false);
        preferences.clear();
        preferences.end();
        savedSSID = "";
        savedPass = "";
        printlnBoth("\r\n[SUCCESS] Kredensial WiFi berhasil dihapus!");
        showMainMenu();
      } else if (input == "4" || lower.indexOf("restart") != -1) {
        printlnBoth("\r\n[INFO] Merestart ESP32...");
        delay(1000);
        ESP.restart();
      } else {
        printlnBoth("[!] Pilihan tidak valid. Silakan ketik angka 1 sampai 4.");
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
  }
}

// ============================================================
// SIMPAN KE FLASH NVS & RESTART OTOMATIS
// ============================================================
void saveAndRestart(String ssid, String pass) {
  printlnBoth("\r\n==========================================");
  printlnBoth("📋 KONFIRMASI PENYIMPANAN WIFI:");
  printlnBoth("   • SSID     : [" + ssid + "]");
  printlnBoth("   • Password : " + (pass.length() > 0 ? "******** (" + String(pass.length()) + " karakter)" : "[Tanpa Password]"));
  printlnBoth("==========================================");
  printlnBoth("💾 Menyimpan kredensial ke Flash NVS ESP32...");

  // Simpan ke Flash Preferences
  preferences.begin("wifi-config", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  preferences.end();

  printlnBoth("✅ [BERHASIL DISIMPAN]");
  printlnBoth("🔄 ESP32 akan merestart dalam 2 detik untuk langsung menghubungkan ke WiFi...");
  printlnBoth("📡 Bluetooth akan otomatis dinonaktifkan jika berhasil terkoneksi.");
  printlnBoth("==========================================\r\n");

  delay(2000);
  ESP.restart(); // Restart ESP32 otomatis
}

// ============================================================
// TAMPILAN MENU UTAMA
// ============================================================
void showMainMenu() {
  currentState = STATE_MENU;
  printlnBoth("\r\n==========================================");
  printlnBoth("       📶 MENU PENGATURAN WIFI ESP32     ");
  printlnBoth("==========================================");
  printlnBoth(" [1] 🔑 Masukkan Nama WiFi & Password Baru");
  printlnBoth(" [2] 📊 Cek Status & Profil WiFi Saat Ini");
  printlnBoth(" [3] 🗑️  Hapus Data WiFi Tersimpan");
  printlnBoth(" [4] 🔄 Restart ESP32");
  printlnBoth("==========================================");
  printBoth("Pilih opsi (1-4) -> ");
}

// ============================================================
// HELPER DUAL OUTPUT (SERIAL & BLUETOOTH)
// ============================================================
void printBoth(String msg) {
  Serial.print(msg);
  if (isBluetoothActive) HC06.print(msg);
}

void printlnBoth(String msg) {
  Serial.println(msg);
  if (isBluetoothActive) HC06.println(msg);
}
