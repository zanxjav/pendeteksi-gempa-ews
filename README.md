# 🌋 GeoShield EWS - Sistem Pemantauan Gempa & Multi-Bencana (Python Django Full-Stack)

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Framework: Django 5](https://img.shields.io/badge/Backend-Django%205.x%20%7C%20DRF-092e20.svg)](geoshield/)
[![Platform](https://img.shields.io/badge/GIS-Google%20Maps%20%7C%20Leaflet-blue.svg)](static/)
[![Hardware](https://img.shields.io/badge/Hardware-ESP32%20%7C%20ESP32--C3-emerald.svg)](firmware/)
[![Status](https://img.shields.io/badge/Status-Online%20%7C%20Production%20Ready-success.svg)](https://lopez-holes-permits-fancy.trycloudflare.com)

---

## 🌐 LINK AKSES WEB PUBLIK (SIAP PAKAI / LIVE RESMI)

| Layanan | Link Akses | Keterangan |
|---|---|---|
| **🌍 Web Publik Online (HTTPS)** | **[https://lopez-holes-permits-fancy.trycloudflare.com](https://lopez-holes-permits-fancy.trycloudflare.com)** | Langsung aktif & bisa dibuka dosen / publik |
| **🚀 1-Click Cloud Hosting (Render)** | **[Deploy to Render](https://render.com/deploy?repo=https://github.com/zanxjav/pendeteksi-gempa-ews)** | Otomatis deploy 24/7 cloud dari GitHub |
| **📱 1-Click Download APK Android** | **[Generate APK via PWABuilder](https://www.pwabuilder.com/reportcard?site=https://lopez-holes-permits-fancy.trycloudflare.com)** | Paket installer `.apk` siap kirim ke dosen |
| **📡 Endpoint REST API ESP32** | **`https://lopez-holes-permits-fancy.trycloudflare.com/api/telemetry/`** | Target pengiriman data sensor mikrokontroler |
| **🔑 Admin Panel Django** | **[https://lopez-holes-permits-fancy.trycloudflare.com/admin/](https://lopez-holes-permits-fancy.trycloudflare.com/admin/)** | Kelola stasiun & log data (user: `admin`, pass: `admin123`) |

---

## 🌟 Fitur Utama Sistem

1. **🐍 Backend Python Django 5.x & REST API**:
   - Endpoint `POST /api/telemetry/`: Menerima data payload JSON dari mikrokontroler ESP32-C3.
   - Endpoint `GET /api/telemetry/`: Menyediakan data telemetri seismik & kebencanaan real-time.
   - Middleware **WhiteNoise** & **Gunicorn** untuk performa hosting cloud produksi tinggi.
2. **🗺️ Google Maps Satellite Command Center**:
   - Citra satelit asli Google Maps Hybrid (`lyrs=y`), Google Roads (`lyrs=m`), dan Google Terrain (`lyrs=p`).
   - **Lingkaran Bahaya Gempa 25 KM**: Terkoneksi dinamis ke data akselerasi getaran **MPU-6050** ESP32 dengan badge puncak (*apex tag*) `25 KM`.
   - **Rute Jalan Evakuasi Riil (OSRM)**: Menghitung jarak jalan raya riil dan estimasi waktu berkendara menuju posko aman (GOR ITERA, RSUD Airan Raya, dll).
   - **Link Navigasi Google Maps**: Buka rute turn-by-turn langsung di aplikasi Google Maps HP.
3. **📱 Progressive Web App (PWA) & APK Ready**:
   - Ikon resolusi tinggi 192px dan 512px dengan manifest standalone.
   - Dapat diinstal langsung dari Google Chrome Android (*"Tambahkan ke Layar Utama"*) atau diekspor ke `.apk`.
4. **📡 Firmware ESP32-C3 / ESP32 Master**:
   - Provisioning WiFi via Bluetooth HC-06 interaktif (perintah `set:SSID,PASS` dan `server:URL`).
   - Multi-Sensor: MPU-6050 (Gempa), HC-SR04 (Banjir), FC-37 (Hujan), TDS Meter (Kualitas Air), Buzzer EWS.

---

## 🚀 Menjalankan Secara Lokal (Opsional)

```bash
# 1. Install Dependensi
pip install -r requirements.txt

# 2. Migrasi Database & Buat Admin
python manage.py migrate
python create_admin.py

# 3. Jalankan Server
python manage.py runserver 0.0.0.0:8000
```

---

## 📄 Lisensi

Proyek ini bersifat open-source di bawah lisensi [MIT License](LICENSE).
