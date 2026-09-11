# 🔌 Skema Sirkuit & Diagram Wiring Hardware GeoShield EWS

Dokumentasi ini menjelaskan konfigurasi pinout, rangkaian elektronika, dan kalibrasi sensor untuk stasiun **GeoShield EWS** berbasis mikrokontroler **ESP32 DevKit V1**.

---

## 📦 Daftar Komponen & Sensor

| No | Komponen | Fungsi | Antarmuka | Tegangan Operasi |
|---|---|---|---|---|
| 1 | **ESP32 DevKit V1 (30 Pin / 38 Pin)** | Mikrokontroler Utama & Modul WiFi | SoC | 3.3V / 5V (Micro USB) |
| 2 | **MPU-6050 Accelerometer / Gyro** | Pendeteksi Gempa & Getaran Seismik | I2C (SDA/SCL) | 3.3V - 5V |
| 3 | **HC-SR04 / JSN-SR04T Waterproof** | Sensor Muka Air / Ketinggian Banjir | Digital Trigger & Echo | 5V |
| 4 | **FC-37 / YL-83 Raindrop Sensor** | Sensor Intensitas Curah Hujan | Analog (A0) | 3.3V - 5V |
| 5 | **Analog TDS Meter Sensor V1.0** | Sensor Kualitas & Partikel Terlarut Air | Analog (A0) | 3.3V - 5.5V |
| 6 | **Active Buzzer 5V** | Sirine / Alarm Peringatan di Lokasi | Digital GPIO | 3.3V - 5V |
| 7 | **Step-down / Power Supply 5V 2A** | Catu daya stabil outdoor / Solar Panel | DC | 5V DC |

---

## ⚡ Tabel Koneksi Pinout ESP32

```
                       +-------------------+
                       |    ESP32 DEVKIT   |
                       +-------------------+
      MPU6050 (SDA) -> | GPIO 21   GPIO 22 | <- MPU6050 (SCL)
    HC-SR04 (TRIG)  -> | GPIO 5    GPIO 18 | <- HC-SR04 (ECHO)
      Active Buzzer -> | GPIO 4    GPIO 2  | -> LED Indikator Onboard
  Raindrop Analog   -> | GPIO 34   GPIO 35 | <- TDS Meter Analog
                       | 3.3V          5V  |
                       | GND          GND  |
                       +-------------------+
```

### Rincian Pin:

1. **Sensor Getaran / Gempa (MPU-6050)**:
   - `VCC` -> ESP32 `3.3V` (atau 5V)
   - `GND` -> ESP32 `GND`
   - `SCL` -> ESP32 `GPIO 22`
   - `SDA` -> ESP32 `GPIO 21`

2. **Sensor Ketinggian Muka Air (HC-SR04 Ultrasonic)**:
   - `VCC` -> ESP32 `5V` (VIN)
   - `GND` -> ESP32 `GND`
   - `TRIG` -> ESP32 `GPIO 5`
   - `ECHO` -> ESP32 `GPIO 18` *(Disarankan pembagi tegangan 1kΩ & 2kΩ jika sensor 5V untuk keamanan pin 3.3V ESP32)*

3. **Sensor Hujan (FC-37 Rain Module)**:
   - `VCC` -> ESP32 `3.3V`
   - `GND` -> ESP32 `GND`
   - `A0` (Analog) -> ESP32 `GPIO 34` (ADC1 Channel 6)

4. **Sensor TDS Air (Analog TDS Sensor)**:
   - `VCC` -> ESP32 `3.3V`
   - `GND` -> ESP32 `GND`
   - `Signal / A0` -> ESP32 `GPIO 35` (ADC1 Channel 7)

5. **Sirine Fisik (Active Buzzer)**:
   - `Positif (+)` -> ESP32 `GPIO 4`
   - `Negatif (-)` -> ESP32 `GND`

---

## 🛠️ Langkah Kalibrasi & Pemasangan

1. **Sensor MPU-6050 (Gempa)**:
   - Pasang MPU-6050 pada bidang datar yang kokoh (pondasi bangunan atau ground stake).
   - Pastikan chip terisolasi dari getaran mesin kipas/AC ruangan.
2. **Sensor HC-SR04 (Banjir)**:
   - Arahkan probe sensor tegak lurus mengarah ke permukaan air sungai / saluran drainase.
   - Atur nilai konstanta `DISTANCE_TO_RIVER_BED_CM` di file `.ino` sesuai dengan tinggi pemasangan sensor dari dasar sungai saat surut.
3. **Sensor TDS Air**:
   - Celupkan elektroda probe TDS ke dalam aliran air tanpa menyentuh modul PCB amplifiernya.
4. **Sensor Hujan**:
   - Posisikan pelat raindrop miring sekitar 15° - 30° di area terbuka agar tetesan air mengalir secara alami dan tidak menggenang permanen setelah hujan reda.

---

## 🚀 Panduan Upload Firmware

1. Install software **Arduino IDE** (v2.x disarankan).
2. Buka menu **Tools -> Board -> ESP32 Arduino -> ESP32 Dev Module**.
3. Install library berikut melalui **Library Manager** (Ctrl + Shift + I):
   - `Wire` (Built-in)
   - `HTTPClient` & `WiFi` (Built-in ESP32)
4. Buka file [firmware/esp32_disaster_station.ino](file:///d:/MY%20FILE/dangerous%20file/code%20and%20sirkuit/pendeteksi%20gempa/firmware/esp32_disaster_station.ino).
5. Sesuaikan nama WiFi (`WIFI_SSID`), `WIFI_PASSWORD`, dan `SERVER_API_URL`.
6. Hubungkan kabel USB dan klik tombol **Upload**.
