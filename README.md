# 🌋 GeoShield EWS - Sistem Pemantauan Gempa & Multi-Bencana (Python Django Full-Stack)

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Framework: Django 5](https://img.shields.io/badge/Backend-Django%205.x%20%7C%20DRF-092e20.svg)](geoshield/)
[![Platform](https://img.shields.io/badge/Platform-OpenStreetMap%20%7C%20ESP32-emerald.svg)](firmware/)
[![Status](https://img.shields.io/badge/Early%20Warning-System%20Ready-red.svg)](index.html)

**GeoShield EWS** adalah platform open-source modern berbasis **Python Django Full-Stack** dan **OpenStreetMap / Google Maps GIS** untuk sistem peringatan dini (*Early Warning System*) dan monitoring real-time bencana alam: **Gempa Bumi / Seismik**, **Banjir (Level Air)**, **Curah Hujan**, dan **Kualitas Air (TDS)**.

Dilengkapi dengan backend database relasional SQLite/PostgreSQL, REST API endpoint untuk mikrokontroler **ESP32 / ESP32-C3**, antarmuka **Django Admin Panel**, **Seismograf Canvas Digital**, **Grafik Telemetri Multi-Sensor Chart.js**, serta firmware **Bluetooth HC-06 Interactive WiFi Provisioning**.

---

## 🌟 Fitur Utama

- 🐍 **Full-Stack Django Backend & REST API**:
  - Endpoint `POST /api/telemetry/`: Menerima data sensor dari ESP32 dan otomatis menyimpan ke database.
  - Endpoint `GET /api/telemetry/latest/`: Menyediakan data telemetri realtime untuk dashboard.
  - Endpoint `GET /api/telemetry/export/`: Download riwayat data sensor format `.csv`.
  - **Django Admin (`/admin`)**: Manajemen stasiun posko sensor, log bencana, dan hak akses user.
- 📱 **ESP32-C3 / ESP32 Bluetooth HC-06 WiFi Manager**:
  - Menu interaktif via Bluetooth Serial (Aplikasi HP) untuk input SSID dan Password WiFi seperti form login.
  - Kredensial WiFi otomatis tersimpan permanen di memori Flash NVS ESP32.
- 🗺️ **Peta OpenStreetMap & GIS Layer**:
  - OpenStreetMap Standard resmi, Dark GIS, Citra Satelit Esri, dan Google Maps API.
  - Pencarian lokasi (*Geocoding*) dan *Reverse-Geocoding* otomatis saat marker stasiun digeser.
  - Sinkronisasi koordinat otomatis saat ESP32 pertama kali terhubung.
- 📈 **Grafik Telemetri Multi-Sensor (*Chart.js*)**:
  - Grafik 1: Tren getaran seismik (PGA $g$ dan percepatan Gal $cm/s^2$).
  - Grafik 2: Kurva kenaikan muka air banjir ($cm$) dan curah hujan ($mm/jam$).
  - Grafik 3: Fluktuasi kekeruhan & partikel terlarut air TDS ($PPM$).
  - Mode tampilan: Peta GIS, Grafik Lengkap, atau Tampilan Split.
- 🚨 **Early Warning System (EWS)**:
  - Sirine Audio sintesis Web Audio API (tanpa file mp3 eksternal).
  - Pengumuman Suara Peringatan Bahasa Indonesia (Web Speech API).

---

## 🚀 Panduan Menjalankan Server Django

### 1. Install Dependensi Python
```bash
pip install -r requirements.txt
```

### 2. Jalankan Migrasi Database
```bash
python manage.py migrate
```

### 3. Buat Akun Admin & Stasiun Default
```bash
python create_admin.py
```
*Username default*: `admin`  
*Password default*: `admin123`

### 4. Jalankan Server Django
```bash
python manage.py runserver
```
Buka browser pada alamat:
- **Web Dashboard**: `http://127.0.0.1:8000/`
- **Django Admin Panel**: `http://127.0.0.1:8000/admin/`

---

## ⚡ Firmware Mikrokontroler ESP32

Tersedia 2 program firmware Arduino siap pakai di folder `firmware/`:

1. **[firmware/esp32_c3_hc06_wifi_manager.ino](firmware/esp32_c3_hc06_wifi_manager.ino)**:
   - Program setup Bluetooth HC-06 untuk menghubungkan ESP32 ke WiFi melalui menu login di HP.
   - Pin: **RX ESP32 = GPIO 4**, **TX ESP32 = GPIO 5**, Baudrate = 9600.
   - Ketik `masukan wifi` atau `menu` di Bluetooth Terminal HP untuk membuka form login.

2. **[firmware/esp32_disaster_station.ino](firmware/esp32_disaster_station.ino)**:
   - Program utama membaca sensor MPU-6050, Ultrasonic Banjir HC-SR04, Sensor Hujan, dan TDS Meter lalu mengirim data JSON via HTTP POST ke endpoint Django `http://IP_SERVER:8000/api/telemetry/`.

---

## 📄 Lisensi

Proyek ini bersifat open-source di bawah lisensi [MIT License](LICENSE).
