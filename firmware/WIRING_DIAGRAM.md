# 🔌 Skema Sirkuit & Diagram Wiring Hardware GeoShield EWS

Dokumentasi ini menjelaskan konfigurasi pinout, rangkaian elektronika, dan kalibrasi sensor untuk stasiun **GeoShield EWS** berbasis mikrokontroler **ESP32 DevKit V1** dan **ESP32-C3**.

---

## 📦 Daftar Komponen & Sensor

| No | Komponen | Fungsi | Antarmuka | Tegangan Operasi |
|---|---|---|---|---|
| 1 | **ESP32 DevKit V1 / ESP32-C3** | Mikrokontroler Utama, WiFi & SoC | SoC | 3.3V / 5V (USB) |
| 2 | **HC-06 Bluetooth Module** | Provisioning WiFi & Menu CLI Nirkabel | UART Serial (9600 Baud) | 3.3V - 5V |
| 3 | **MPU-6050 Accelerometer / Gyro** | Pendeteksi Gempa & Getaran Seismik (PGA) | I2C (SDA/SCL) | 3.3V - 5V |
| 4 | **HC-SR04 / JSN-SR04T Waterproof** | Sensor Muka Air / Ketinggian Banjir | Digital Trigger & Echo | 5V |
| 5 | **FC-37 / YL-83 Raindrop Sensor** | Sensor Intensitas Curah Hujan | Analog ADC | 3.3V - 5V |
| 6 | **Analog TDS Meter Sensor V1.0** | Sensor Kualitas & Partikel Terlarut Air | Analog ADC | 3.3V - 5.5V |
| 7 | **Active Buzzer 5V** | Sirine / Alarm Peringatan di Lokasi | Digital GPIO | 3.3V - 5V |
| 8 | **Power Supply / Adaptor 5V 2A** | Catu daya stabil outdoor / Solar Panel | DC | 5V DC |

---

## ⚡ Tabel Pinout Hardware

Firmware [esp32_c3_hc06_wifi_manager.ino](file:///d:/MY%20FILE/dangerous%20file/code%20and%20sirkuit/pendeteksi%20gempa/firmware/esp32_c3_hc06_wifi_manager.ino) mendukung kedua jenis board secara otomatis via preprocessor macro:

### 1. Board Standar: ESP32 DevKit V1 (30 Pin / 38 Pin)

```
                       +-------------------+
                       |    ESP32 DEVKIT   |
                       +-------------------+
       HC-06 TX (Data) -> | GPIO 16 (RX2)     |
       HC-06 RX (Data) <- | GPIO 17 (TX2)     |
      MPU6050 (SDA)   -> | GPIO 21   GPIO 22 | <- MPU6050 (SCL)
      HC-SR04 (TRIG)  -> | GPIO 5    GPIO 18 | <- HC-SR04 (ECHO)
      Raindrop (Analog)-> | GPIO 34   GPIO 35 | <- TDS Meter (Analog)
      Active Buzzer   <- | GPIO 4    GPIO 2  | -> LED Indikator Onboard
                         | 3.3V          5V  |
                         | GND          GND  |
                         +-------------------+
```

### 2. Board Khusus: ESP32-C3 SuperMini / DevKit (RISC-V)

```
                       +-------------------+
                       |    ESP32-C3 BOARD |
                       +-------------------+
       HC-06 TX (Data) -> | GPIO 20 (RX)      |
       HC-06 RX (Data) <- | GPIO 21 (TX)      |
      MPU6050 (SDA)   -> | GPIO 8    GPIO 9  | <- MPU6050 (SCL)
      HC-SR04 (TRIG)  -> | GPIO 6    GPIO 7  | <- HC-SR04 (ECHO)
      Raindrop (Analog)-> | GPIO 0    GPIO 1  | <- TDS Meter (Analog)
      Active Buzzer   <- | GPIO 3    GPIO 2  | -> LED Indikator Onboard
                         | 3.3V          5V  |
                         | GND          GND  |
                         +-------------------+
```

---

## 🛠️ Panduan Rangkaian Sensor & Modul

1. **Modul Bluetooth HC-06**:
   - `VCC` -> ESP32 `5V` (atau `3.3V`)
   - `GND` -> ESP32 `GND`
   - `TXD HC-06` -> ESP32 `RX Pin` (GPIO 16 pada ESP32 / GPIO 20 pada ESP32-C3)
   - `RXD HC-06` -> ESP32 `TX Pin` (GPIO 17 pada ESP32 / GPIO 21 pada ESP32-C3)

2. **Sensor Getaran / Gempa (MPU-6050)**:
   - `VCC` -> ESP32 `3.3V`
   - `GND` -> ESP32 `GND`
   - `SDA` -> ESP32 `GPIO 21` (atau `GPIO 8` pada C3)
   - `SCL` -> ESP32 `GPIO 22` (atau `GPIO 9` pada C3)

3. **Sensor Ketinggian Air / Banjir (HC-SR04)**:
   - `VCC` -> ESP32 `5V`
   - `GND` -> ESP32 `GND`
   - `TRIG` -> ESP32 `GPIO 5` (atau `GPIO 6` pada C3)
   - `ECHO` -> ESP32 `GPIO 18` (atau `GPIO 7` pada C3) *(Disarankan resistor voltage divider 1k/2k jika sensor 5V murni)*

4. **Sensor Curah Hujan (FC-37 / YL-83)**:
   - `VCC` -> ESP32 `3.3V`
   - `GND` -> ESP32 `GND`
   - `A0` -> ESP32 `GPIO 34` (atau `GPIO 0` pada C3)

5. **Sensor TDS Air (Kekeruhan / Kualitas Air)**:
   - `VCC` -> ESP32 `3.3V`
   - `GND` -> ESP32 `GND`
   - `A0` -> ESP32 `GPIO 35` (atau `GPIO 1` pada C3)

6. **Sirine Fisik (Active Buzzer)**:
   - `Positif (+)` -> ESP32 `GPIO 4` (atau `GPIO 3` pada C3)
   - `Negatif (-)` -> ESP32 `GND`

---

## 📱 Cara Menggunakan Fitur HC-06 Bluetooth Provisioning

1. Sambungkan modul HC-06 ke ESP32 dan nyalakan ESP32.
2. Buka aplikasi **Bluetooth Terminal** (di Android / iOS) atau buka Web Dashboard GeoShield EWS pada Google Chrome.
3. Hubungkan ke perangkat Bluetooth **HC-06** (PIN default: `1234` atau `0000`).
4. Perintah yang tersedia:
   - `menu` : Menampilkan menu utama interaktif
   - `set:NamaSSID,SandiWiFi` : Format instan simpan SSID & sandi WiFi lalu restart otomatis
   - `server:http://IP_SERVER:8000/api/telemetry/` : Mengganti URL backend server tanpa re-flash
   - `sensor` : Menampilkan bacaan live semua sensor (Gempa, Air, Hujan, TDS)
   - `status` : Mengecek status IP address, RSSI, dan koneksi
   - `restart` : Melakukan reboot ESP32
