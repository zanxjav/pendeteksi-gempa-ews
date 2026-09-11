/*
 * ==============================================================================
 * Project: ESP32 / ESP32-C3 + HC-06 Bluetooth Interactive WiFi Manager (V3.0 Ultra-Reliable)
 * Author: GeoShield EWS Open Source
 * Description:
 *   Sistem login WiFi via Bluetooth HC-06 dengan pembersih karakter tersembunyi
 *   (CR/LF/Null byte cleaner), fitur pemilihan nomor WiFi dari hasil scan otomatis,
 *   diagnostik status error koneksi realtime, serta penyimpanan Flash NVS.
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

// Inisialisasi UART HC-06
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  HardwareSerial HC06(0); // ESP32-C3
#else
  HardwareSerial HC06(1); // ESP32 Standard
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
const unsigned long BUFFER_TIMEOUT_MS = 250; // Timeout auto-submit jika tanpa Enter

String tempSSID = "";
String tempPass = "";

// Array untuk menyimpan daftar SSID hasil scan agar bisa dipilih via angka
String scannedSSIDs[20];
int scannedCount = 0;

// Prototipe Fungsi
void printBoth(String msg);
void printlnBoth(String msg);
String cleanString(String raw);
void showMainMenu();
void scanNearbyWiFi();
void connectWiFi(String ssid, String pass);
void loadSavedWiFi();
void processCommand(String input);
void printWiFiStatus();

void setup() {
  // Inisialisasi Serial Komputer
  Serial.begin(115200);
  delay(1000);

  // Inisialisasi UART HC-06 Bluetooth
  pinMode(HC06_RX, INPUT_PULLUP);
  pinMode(HC06_TX, OUTPUT);
  HC06.begin(9600, SERIAL_8N1, HC06_RX, HC06_TX);

  delay(500);

  printlnBoth("\r\n==========================================");
  printlnBoth("🚀 ESP32 + HC-06 BLUETOOTH SIAP!");
  printlnBoth("==========================================");
  printlnBoth("[INFO] Pin RX = GPIO 4 | Pin TX = GPIO 5");
  printlnBoth("[INFO] Baudrate = 9600 bps");
  printlnBoth("==========================================");
  printlnBoth("👉 Kirim 'masukan wifi' atau 'menu' di HP Anda");
  printlnBoth("==========================================\r\n");

  // Baca kredensial tersimpan
  loadSavedWiFi();
}

void loop() {
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
    HC06.write(c);
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

  // 3. Auto-process jika di HP mengirim teks tanpa tekan Enter
  if (inputBuffer.length() > 0 && (millis() - lastCharTime > BUFFER_TIMEOUT_MS)) {
    processCommand(cleanString(inputBuffer));
    inputBuffer = "";
  }

  delay(10);
}

// =======================================================
// PEMBERSIH KARAKTER TERSEMBUNYI (CR, LF, NULL, CONTROL)
// Menghilangkan bug penyebab password gagal terautentikasi
// =======================================================
String cleanString(String raw) {
  String cleaned = "";
  for (unsigned int i = 0; i < raw.length(); i++) {
    char c = raw.charAt(i);
    // Hanya simpan karakter ASCII printable standar (kode 32 - 126)
    if (c >= 32 && c <= 126) {
      cleaned += c;
    }
  }
  cleaned.trim();
  return cleaned;
}

// ==========================================
// PENGOLAHAN COMMAND & STATE LOGIN
// ==========================================
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
        // Format Pintas Cepat: set:NamaWiFi,PasswordWiFi
        int commaIndex = input.indexOf(',');
        if (commaIndex != -1) {
          String s = cleanString(input.substring(4, commaIndex));
          String p = cleanString(input.substring(commaIndex + 1));
          connectWiFi(s, p);
        } else {
          printlnBoth("\r\n[ERROR] Format salah! Gunakan format: set:NamaWiFi,PasswordWiFi\r\n");
        }
      } else if (lower == "status") {
        printWiFiStatus();
      } else if (lower == "scan") {
        scanNearbyWiFi();
      } else {
        printlnBoth("\r\n[?] Perintah diterima: \"" + input + "\"");
        printlnBoth("💡 Ketik 'masukan wifi' atau 'menu' untuk membuka menu.\r\n");
      }
      break;
    }

    case STATE_MENU: {
      if (input == "1" || lower.indexOf("scan") != -1) {
        scanNearbyWiFi();
      } else if (input == "2" || lower.indexOf("masuk") != -1 || lower.indexOf("wifi") != -1 || lower.indexOf("login") != -1) {
        currentState = STATE_INPUT_SSID;
        printlnBoth("\r\n------------------------------------------");
        printlnBoth("🔑 [HALAMAN LOGIN WIFI]");
        printlnBoth("------------------------------------------");
        if (scannedCount > 0) {
          printlnBoth("💡 Tips: Anda bisa ketik nomor WiFi hasil scan (cth: 1, 2) atau ketik nama SSID manual:");
        } else {
          printlnBoth("Langkah 1/2: Masukkan Nama WiFi (SSID):");
        }
        printBoth("SSID / Nomor -> ");
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
      // Cek apakah user menginput nomor dari hasil scan
      int choiceNum = input.toInt();
      if (choiceNum >= 1 && choiceNum <= scannedCount) {
        tempSSID = scannedSSIDs[choiceNum - 1];
        printlnBoth("\r\n👉 Anda memilih nomor [" + String(choiceNum) + "]: \"" + tempSSID + "\"");
      } else {
        tempSSID = input;
      }

      currentState = STATE_INPUT_PASS;
      printlnBoth("\r\n✅ [SSID TERCATAT]: \"" + tempSSID + "\" (" + String(tempSSID.length()) + " karakter)");
      printlnBoth("Langkah 2/2: Masukkan Password WiFi:");
      printlnBoth("(Ketik 'none' jika WiFi tanpa password / open hotspot)");
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
      printlnBoth("   • SSID     : [" + tempSSID + "]");
      printlnBoth("   • Password : " + (tempPass.length() > 0 ? "[" + tempPass + "]" : "[Tanpa Password]"));
      printlnBoth("==========================================");
      printlnBoth("⏳ Menghubungkan ke jaringan WiFi...");

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
  printlnBoth(" [4] 🗑️  Hapus Riwayat WiFi Tersimpan");
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

  scannedCount = WiFi.scanNetworks();
  if (scannedCount == 0) {
    printlnBoth("[!] Tidak ada jaringan WiFi yang terdeteksi.");
  } else {
    printlnBoth("\r\n📡 Ditemukan " + String(scannedCount) + " jaringan WiFi (2.4 GHz):");
    if (scannedCount > 20) scannedCount = 20;
    
    for (int i = 0; i < scannedCount; ++i) {
      scannedSSIDs[i] = WiFi.SSID(i);
      String lock = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "🔓 (Open)" : "🔒 (Terkunci)";
      printlnBoth("   [" + String(i + 1) + "] " + scannedSSIDs[i] + " (" + String(WiFi.RSSI(i)) + " dBm) " + lock);
      delay(10);
    }
    printlnBoth("\r\n👉 Ketik [2] untuk memilih nama WiFi atau memasukkan password.");
  }
}

// =======================================================
// FUNGSI HUBUNGKAN KE WIFI DENGAN PENANGANAN LENGKAP
// =======================================================
void connectWiFi(String ssid, String pass) {
  ssid = cleanString(ssid);
  pass = cleanString(pass);

  if (ssid.length() == 0) {
    printlnBoth("[ERROR] Nama WiFi (SSID) tidak boleh kosong!\r\n");
    return;
  }

  printlnBoth("\r\n[WiFi] Reset radio WiFi ESP32...");
  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  delay(300);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Nonaktifkan power saving untuk koneksi maksimal

  printlnBoth("[WiFi] Mengirim permintaan koneksi ke: \"" + ssid + "\"");
  
  if (pass.length() > 0) {
    WiFi.begin(ssid.c_str(), pass.c_str());
  } else {
    WiFi.begin(ssid.c_str());
  }

  // Coba hubungkan hingga 40 siklus (sekitar 20 detik)
  int retries = 0;
  bool isConnected = false;

  while (retries < 40) {
    delay(500);
    retries++;

    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
      isConnected = true;
      break;
    } else if (st == WL_CONNECT_FAILED) {
      printBoth("[X]");
    } else if (st == WL_NO_SSID_AVAIL) {
      printBoth("[?]");
    } else {
      printBoth(".");
    }
  }

  if (isConnected || WiFi.status() == WL_CONNECTED) {
    printlnBoth("\r\n\r\n==========================================");
    printlnBoth("✅ [LOGIN BERHASIL] ESP32 TERKONEKSI!");
    printlnBoth("==========================================");
    printlnBoth("   • Nama WiFi  : " + WiFi.SSID());
    printlnBoth("   • IP Address : " + WiFi.localIP().toString());
    printlnBoth("   • Sinyal RSSI: " + String(WiFi.RSSI()) + " dBm");
    printlnBoth("   • Gateway    : " + WiFi.gatewayIP().toString());
    printlnBoth("==========================================");

    // Simpan permanen ke NVS Preferences ESP32
    preferences.begin("wifi-config", false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", pass);
    preferences.end();
    printlnBoth("💾 [MEMORI] Kredensial WiFi berhasil disimpan secara permanen di ESP32!\r\n");
  } else {
    wl_status_t finalStatus = WiFi.status();
    printlnBoth("\r\n\r\n==========================================");
    printlnBoth("❌ [GAGAL TERHUBUNG]");
    printlnBoth("==========================================");
    
    if (finalStatus == WL_NO_SSID_AVAIL) {
      printlnBoth("   Penyebab: Nama WiFi \"" + ssid + "\" TIDAK DITEMUKAN!");
      printlnBoth("   • Pastikan WiFi router/Hotspot berada pada frekuensi 2.4 GHz (ESP32 tidak mendukung 5 GHz murni).");
      printlnBoth("   • Pastikan jarak ESP32 tidak terlalu jauh dari router.");
    } else if (finalStatus == WL_CONNECT_FAILED) {
      printlnBoth("   Penyebab: PASSWORD SALAH untuk WiFi \"" + ssid + "\".");
      printlnBoth("   • Periksa huruf besar/kecil pada password.");
    } else {
      printlnBoth("   Penyebab: Timeout saat meminta IP Address (Status Code: " + String(finalStatus) + ").");
      printlnBoth("   • Coba hidupkan ulang Hotspot HP Anda atau dekatan ESP32 ke router.");
    }
    printlnBoth("==========================================");
    printlnBoth("💡 Ketik 'masukan wifi' atau 'menu' untuk mencoba login kembali.\r\n");
  }
}

// ==========================================
// BACA KREDENSIAL DARI MEMORI SAAT BOOT
// ==========================================
void loadSavedWiFi() {
  preferences.begin("wifi-config", true);
  String savedSSID = cleanString(preferences.getString("ssid", ""));
  String savedPass = cleanString(preferences.getString("pass", ""));
  preferences.end();

  if (savedSSID.length() > 0) {
    printlnBoth("[MEMORI] Ditemukan profil WiFi tersimpan: \"" + savedSSID + "\"");
    printlnBoth("[MEMORI] Mencoba auto-connect...");
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
