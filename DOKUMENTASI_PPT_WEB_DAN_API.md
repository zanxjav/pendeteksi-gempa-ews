# 📊 Materi Presentasi (5 Slide): Arsitektur Web, REST API & Koneksi ESP32 GeoShield EWS

File presentasi PowerPoint telah di-generate secara otomatis dan dapat langsung dibuka di Microsoft PowerPoint:
📁 **File:** `presentation_geoshield_ews.pptx`

Berikut adalah rincian isi materi 5 slide presentasi lengkap dengan penjelasan detail tapi padat dan to the point:

---

## Slide 1: Gambaran Arsitektur Web GIS & IoT GeoShield EWS
**Kategori:** ARSITEKTUR SISTEM  
**Subjudul:** Integrasi Holistik antara Web GIS Dashboard, Backend Django, dan Edge Node ESP32-C3

### 1. 🌐 Frontend Web GIS (Leaflet.js)
- **Peta Interaktif GIS:** Menggunakan Leaflet.js dengan basemap OpenStreetMap & citra Google Satellite untuk visualisasi spasial gempa bumi real-time di wilayah Lampung & ITERA.
- **Multi-Layer Kebencanaan:** Menampilkan zona rawan gempa, jalur sesar aktif, ketinggian muka air banjir, radius bahaya, dan sebaran stasiun sensor.
- **Live Seismograph & Charts:** Render gelombang seismograf getaran tanah via HTML5 Canvas (60 FPS) serta grafik riwayat sensor interaktif.

### 2. ⚙️ Backend Django Framework
- **Core Framework:** Menangani arsitektur MVT/MVC, routing REST API, serta manajemen session dan autentikasi pengguna.
- **Database Relasional:** Menyimpan profil stasiun sensor (`SensorStation`), riwayat log telemetri (`TelemetryRecord`), dan riwayat log peringatan bencana (`DisasterAlertLog`).
- **Automated Processing:** Algoritma cerdas yang menghitung estimasi magnitudo gempa, estimasi skala MMI, dan status siaga bencana secara otomatis dari data percepatan tanah.

### 3. 📡 Perangkat Edge Node (ESP32-C3)
- **Multi-Sensor Station:** Terhubung ke sensor seismik PGA getaran tanah, ultrasonic kedalaman air, rain drop curah hujan, dan TDS kualitas air.
- **Komunikasi Nirkabel:** Dilengkapi modul WiFi 2.4 GHz untuk koneksi web dan Bluetooth HC-06 untuk konfigurasi offline.
- **Dual Connection:** Mendukung transmisi telemetri otomatis ke server web Django sekaligus menyediakan WebServer internal lokal (port 80).

---

## Slide 2: Dokumentasi Endpoint REST API Web Backend
**Kategori:** REST API SPECIFICATION  
**Subjudul:** Katalog Endpoint Django untuk Komunikasi Data Mesin (M2M) dan Antarmuka Pengguna

### 1. 📥 Ingestion Telemetri: `POST /api/telemetry/`
- **Menerima Telemetri Sensor:** Menerima payload JSON dari ESP32 mencakup PGA getaran, kedalaman air, analog hujan, dan nilai TDS air.
- **Auto-Calculate Metrik:** Server mengonversi PGA ke satuan Gal, menentukan magnitudo Richter, skala MMI (I - VI+), dan status kebencanaan (NORMAL, WASPADA, SIAGA, AWAS).
- **Auto-Update Koordinat:** Jika ESP32 mengirim data latitude/longitude baru, koordinat stasiun pada peta Leaflet otomatis diperbarui.

### 2. 📊 Data Feed: `GET /api/telemetry/` & `/api/telemetry/latest/`
- **Histori Telemetri (`GET /api/telemetry/`):** Mengembalikan 30 data rekaman sensor terbaru untuk plotting grafik riwayat tren di web.
- **Realtime Feed (`GET /api/telemetry/latest/`):** Polling kilat 1 data terbaru untuk memperbarui indikator gauge dashboard secara instan dan efisien.
- **Export Data (`GET /api/telemetry/export/`):** Menghasilkan unduhan file CSV berisi timestamp, tipe sensor, dan status untuk kebutuhan analisis riset.

### 3. 🌍 BMKG Live Proxy: `GET /api/bmkg/feed/`
- **Bypass Kendala CORS:** Backend bertindak sebagai proxy aman yang mengambil data feed gempa terkini dari BMKG dan USGS.
- **Normalisasi Data:** Menyelaraskan format gempa bumi regional Indonesia sebelum disuntikkan ke marker gempa pada peta Leaflet.
- **Early Warning Feeds:** Menampilkan notifikasi seketika jika terjadi gempa berkekuatan M > 5.0 di wilayah sekitar Sumatera & Lampung.

---

## Slide 3: Alur Pengiriman Data: Dari ESP32 ke Web Server
**Kategori:** HARDWARE TO WEB DATA FLOW  
**Subjudul:** Transmisi Telemetri Real-Time melalui Protokol HTTP REST Menggunakan Format JSON Standar

### 1. 📦 Struktur Payload JSON ESP32
- **Identitas Stasiun:** `station_id` ('ST-01-ESP32'), nama posko, dan koordinat GPS (`lat: -5.3582`, `lng: 105.3146`).
- **Data Seismik:** Nilai percepatan tanah PGA ($g$), percepatan Gal ($cm/s^2$), dan frekuensi getaran gelombang seismik ($Hz$).
- **Multi-Parameter:** Ketinggian muka air ($cm$) deteksi banjir, intensitas curah hujan ($mm/jam$), serta TDS kualitas air ($ppm$).

### 2. 🚀 Mekanisme Transmisi HTTPClient
- **Protokol HTTP POST:** ESP32 memanfaatkan library `HTTPClient.h` dengan target URL web backend yang dapat dikonfigurasi.
- **Interval Terjadwal:** Pengiriman data dilakukan periodik setiap 3000 ms (3 detik) saat status WiFi terverifikasi `WL_CONNECTED`.
- **Handshake & Response:** Server merespons dengan `HTTP 201 Created` dan `record_id`; ESP32 mencatat log berhasil ke Serial & HC-06.

### 3. 🛡️ Keandalan & Error Handling Transmisi
- **Non-Blocking WiFi Loop:** Status WiFi dikelola menggunakan State Machine (`WIFI_IDLE` -> `WIFI_CONNECTING` -> `WIFI_CONNECTED`).
- **Auto-Reconnection:** Jika koneksi WiFi terputus, siklus loop otomatis mendeteksi status dan mencoba reconnect tanpa hang.
- **Timeout Protection:** Timeout koneksi dibatasi 10 detik guna mencegah mikrokontroler membeku (freezing) saat sinyal lemah.

---

## Slide 4: Koneksi Web ke ESP32: Kontrol & Provisioning
**Kategori:** WEB TO HARDWARE INTERACTION  
**Subjudul:** Tiga Kanal Komunikasi Langsung dari Antarmuka Web Browser Menuju Mikrokontroler ESP32

### 1. 🌐 1. Local WebServer Port 80 (WiFi / AP)
- **Embedded Web Server:** ESP32 menjalankan WebServer di port 80 dengan header CORS (`Access-Control-Allow-Origin: *`).
- **Endpoint Lokal:** Menyediakan `/api/status` (info WiFi & RSSI), `/api/telemetry` (data sensor), dan `/api/logs` (terminal log).
- **Direct Command (`/api/cmd?q=...`):** Browser dapat mengirim perintah jarak jauh seperti cek status, scan WiFi, atau restart perangkat.

### 2. 🔌 2. Web Serial API (Chrome/Edge Laptop)
- **Akses Port Serial Langsung:** Browser di PC/Laptop dapat membuka port COM USB atau Bluetooth Serial langsung tanpa driver khusus.
- **Komunikasi Dua Arah:** Baudrate 9600 bps, membaca log UART serial dan mengirimkan perintah langsung dari terminal UI web.
- **Bebas Instalasi Software:** Tidak memerlukan Arduino Serial Monitor atau PuTTY, seluruh interaksi terjadi di tab browser.

### 3. 📶 3. Web Bluetooth API & Modul HC-06
- **Koneksi Nirkabel Bluetooth:** Web browser menghubungkan modul HC-06 melalui layanan Bluetooth SPP/UART secara wireless.
- **Format Perintah WiFi Provisioning:** Web mengirim `set:SSID,PASSWORD` untuk menyimpan kredensial ke flash memory ESP32.
- **Fallback SoftAP:** Jika WiFi belum terkonfigurasi, ESP32 memancarkan WiFi AP `ESP32-C3-EWS` agar HP dapat mengakses halaman setup.

---

## Slide 5: Keamanan, Keandalan & Otomasi Peringatan Dini
**Kategori:** SECURITY & RELIABILITY  
**Subjudul:** Fondasi Robustness Sistem untuk Menjamin Kesiapsiagaan Menghadapi Bencana Gempa & Banjir

### 1. 💾 Penyimpanan Flash Non-Volatile (NVS)
- **Library Preferences.h:** SSID dan password WiFi tersimpan aman pada partisi flash NVS (`wifi_cfg`).
- **Power-Loss Immune:** Konfigurasi tidak hilang meskipun suplai listrik padam mendadak atau ESP32 di-restart.
- **Pembersihan Mudah:** Perintah `hapus wifi` atau endpoint `POST /api/wifi` memungkinkan reset konfigurasi secara instan.

### 2. 🚨 Audio-Visual EWS Alarm Automation
- **Deteksi Ambang Batas Otomatis:** Web memicu status SIAGA/AWAS jika getaran PGA melampaui 0.05g atau Muka Air > 100cm.
- **Peringatan Audio Multitonal:** Sirine darurat berbasis Web Audio API langsung berbunyi di browser petugas saat bahaya terdeteksi.
- **Modal Peringatan Evakuasi:** Muncul dialog darurat interaktif lengkap dengan panduan mitigasi dan titik evakuasi terdekat.

### 3. 🔒 Arsitektur Keamanan & Skalabilitas
- **Proteksi M2M IoT:** Endpoint telemetri dilindungi dengan verifikasi skema JSON dan penanganan CSRF yang disesuaikan untuk IoT.
- **CORS Header Terkendali:** Memastikan data sensor aman diakses oleh web dashboard resmi dan aplikasi mobile pendukung.
- **Multi-Station Architecture:** Struktur database mendukung skalabilitas banyak stasiun sensor (multi-node) di berbagai penjuru wilayah.
