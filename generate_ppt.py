"""
Script generator presentasi PowerPoint (PPTX) GeoShield EWS
Membahas arsitektur Web, REST API, koneksi ESP32, dan integrasi IoT
Dibuat menggunakan python-pptx dengan desain profesional, modern dark mode.
"""
from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN
from pptx.enum.shapes import MSO_SHAPE

def create_presentation():
    prs = Presentation()
    prs.slide_width = Inches(13.333)
    prs.slide_height = Inches(7.5) # Widescreen 16:9

    # Color Palette: Modern Navy / Tech Dark Mode
    COLOR_BG = RGBColor(11, 19, 43)        # #0B132B
    COLOR_CARD = RGBColor(19, 31, 66)      # #131F42
    COLOR_ACCENT = RGBColor(0, 229, 255)   # #00E5FF (Cyan/Teal)
    COLOR_EMERALD = RGBColor(16, 185, 129) # #10B981 (Green)
    COLOR_WARNING = RGBColor(245, 158, 11) # #F59E0B (Amber)
    COLOR_WHITE = RGBColor(255, 255, 255)
    COLOR_MUTED = RGBColor(160, 174, 192)  # #A0AEC0
    COLOR_CARD_BORDER = RGBColor(34, 52, 102)

    blank_layout = prs.slide_layouts[6]

    slides_data = [
        # SLIDE 1
        {
            "slide_num": "01",
            "category": "ARSITEKTUR SISTEM",
            "title": "Gambaran Arsitektur Web GIS & IoT GeoShield EWS",
            "subtitle": "Integrasi Holistik antara Web GIS Dashboard, Backend Django, dan Edge Node ESP32-C3",
            "cards": [
                {
                    "title": "🌐 Frontend Web GIS (Leaflet.js)",
                    "color": COLOR_ACCENT,
                    "points": [
                        "Peta Interaktif GIS: Menggunakan Leaflet.js dengan basemap OpenStreetMap & citra Google Satellite untuk visualisasi spasial real-time.",
                        "Multi-Layer Kebencanaan: Menampilkan zona rawan gempa, jalur sesar aktif, ketinggian muka air banjir, dan sebaran sensor.",
                        "Live Seismograph & Charts: Render gelombang seismograf getaran tanah via HTML5 Canvas (60 FPS) dan grafik sensor interaktif."
                    ]
                },
                {
                    "title": "⚙️ Backend Django Framework",
                    "color": COLOR_EMERALD,
                    "points": [
                        "Core Framework: Menangani arsitektur MVT/MVC, routing REST API, serta manajemen session dan autentikasi pengguna.",
                        "Database Relasional: Menyimpan profil stasiun sensor (SensorStation), riwayat log telemetri (TelemetryRecord), dan riwayat peringatan bencana.",
                        "Automated Processing: Algoritma cerdas yang menghitung magnitudo gempa, estimasi skala MMI, dan status siaga bencana secara otomatis."
                    ]
                },
                {
                    "title": "📡 Perangkat Edge Node (ESP32-C3)",
                    "color": COLOR_WARNING,
                    "points": [
                        "Multi-Sensor Station: Terhubung ke sensor MPU6050 (PGA), Ultrasonic (Muka Air), Rain Drop Sensor, dan TDS Meter (Kualitas Air).",
                        "Komunikasi Nirkabel: Dilengkapi modul WiFi berdaya 11 dBm untuk koneksi web dan Bluetooth HC-06 untuk konfigurasi offline.",
                        "Dual Connection: Mendukung pengiriman telemetri ke server web sekaligus menyediakan WebServer internal lokal (port 80)."
                    ]
                }
            ]
        },

        # SLIDE 2
        {
            "slide_num": "02",
            "category": "REST API SPECIFICATION",
            "title": "Dokumentasi Endpoint REST API Web Backend",
            "subtitle": "Katalog Endpoint Django untuk Komunikasi Data Mesin (M2M) dan Antarmuka Pengguna",
            "cards": [
                {
                    "title": "📥 Ingestion Telemetri: POST /api/telemetry/",
                    "color": COLOR_EMERALD,
                    "points": [
                        "Menerima Telemetri Sensor: Menerima payload JSON dari ESP32 mencakup PGA getaran, kedalaman air, analog hujan, dan nilai TDS air.",
                        "Auto-Calculate Metrik: Server langsung mengonversi PGA ke satuan Gal, menentukan magnitudo Richter, skala MMI (I - VI+), dan status kebencanaan.",
                        "Auto-Update Koordinat: Jika ESP32 mengirim data latitude/longitude baru, koordinat stasiun pada peta otomatis diperbarui."
                    ]
                },
                {
                    "title": "📊 Data Feed: GET /api/telemetry/ & /latest/",
                    "color": COLOR_ACCENT,
                    "points": [
                        "Histori Telemetri (GET /api/telemetry/): Mengembalikan 30 data rekaman sensor terbaru untuk plotting grafik riwayat tren.",
                        "Realtime Feed (GET /api/telemetry/latest/): Polling kilat 1 data terbaru untuk memperbarui indikator gauge dashboard secara efisien.",
                        "Export Data (GET /api/telemetry/export/): Menghasilkan file CSV berisi timestamp, tipe sensor, dan status untuk kebutuhan analisis."
                    ]
                },
                {
                    "title": "🌍 BMKG Live Proxy: GET /api/bmkg/feed/",
                    "color": COLOR_WARNING,
                    "points": [
                        "Bypass Kendala CORS: Backend bertindak sebagai proxy aman yang mengambil data feed gempa terkini dari BMKG dan USGS.",
                        "Normalisasi Data: Menyelaraskan format gempa bumi regional Indonesia sebelum disuntikkan ke marker gempa pada peta Leaflet.",
                        "Early Warning Feeds: Menampilkan notifikasi seketika jika terjadi gempa berkekuatan M > 5.0 di wilayah sekitar Sumatera & Lampung."
                    ]
                }
            ]
        },

        # SLIDE 3
        {
            "slide_num": "03",
            "category": "HARDWARE TO WEB DATA FLOW",
            "title": "Alur Pengiriman Data: Dari ESP32 ke Web Server",
            "subtitle": "Transmisi Telemetri Real-Time melalui Protokol HTTP REST Menggunakan Format JSON Standar",
            "cards": [
                {
                    "title": "📦 Struktur Payload JSON ESP32",
                    "color": COLOR_ACCENT,
                    "points": [
                        "Identitas: station_id ('ST-01-ESP32'), nama posko, dan koordinat GPS (lat: -5.3582, lng: 105.3146).",
                        "Data Seismik: Nilai percepatan tanah PGA (g), percepatan Gal (cm/s²), dan frekuensi getaran gelombang seismik (Hz).",
                        "Multi-Parameter: Ketinggian muka air (cm) deteksi banjir, intensitas curah hujan (mm/jam), serta TDS kualitas air (ppm)."
                    ]
                },
                {
                    "title": "🚀 Mekanisme Transmisi HTTPClient",
                    "color": COLOR_EMERALD,
                    "points": [
                        "Protokol HTTP POST: ESP32 memanfaatkan library HTTPClient.h dengan target URL web backend yang dapat dikonfigurasi.",
                        "Interval Terjadwal: Pengiriman data dilakukan periodik setiap 3000 ms (3 detik) saat status WiFi terverifikasi WL_CONNECTED.",
                        "Handshake & Response: Server merespons dengan HTTP 201 Created dan record_id; ESP32 mencatat log berhasil ke Serial & HC-06."
                    ]
                },
                {
                    "title": "🛡️ Keandalan & Error Handling Transmisi",
                    "color": COLOR_WARNING,
                    "points": [
                        "Non-Blocking WiFi Loop: Status WiFi dikelola menggunakan State Machine (IDLE -> CONNECTING -> CONNECTED).",
                        "Auto-Reconnection: Jika koneksi WiFi terputus, siklus loop otomatis mendeteksi status dan mencoba reconnect tanpa hang.",
                        "Timeout Protection: Timeout koneksi dibatasi 10 detik guna mencegah mikrokontroler membeku (freezing) saat sinyal lemah."
                    ]
                }
            ]
        },

        # SLIDE 4
        {
            "slide_num": "04",
            "category": "WEB TO HARDWARE INTERACTION",
            "title": "Koneksi Web ke ESP32: Kontrol & Provisioning",
            "subtitle": "Tiga Kanal Komunikasi Langsung dari Antarmuka Web Browser Menuju Mikrokontroler ESP32",
            "cards": [
                {
                    "title": "🌐 1. Local WebServer Port 80 (WiFi / AP)",
                    "color": COLOR_ACCENT,
                    "points": [
                        "Embedded Web Server: ESP32 menjalankan WebServer di port 80 dengan header CORS (Access-Control-Allow-Origin: *).",
                        "Endpoint Lokal: Menyediakan /api/status (info WiFi & RSSI), /api/telemetry (data sensor), dan /api/logs (terminal log).",
                        "Direct Command (/api/cmd?q=...): Browser dapat mengirim perintah jarak jauh seperti cek status atau restart perangkat."
                    ]
                },
                {
                    "title": "🔌 2. Web Serial API (Chrome/Edge Laptop)",
                    "color": COLOR_EMERALD,
                    "points": [
                        "Akses Port Serial Langsung: Browser di PC/Laptop dapat membuka port COM USB atau Bluetooth Serial langsung tanpa driver khusus.",
                        "Komunikasi Dua Arah: Baudrate 9600 bps, membaca log UART serial dan mengirimkan perintah langsung dari terminal UI web.",
                        "Bebas Instalasi Software: Tidak memerlukan Arduino Serial Monitor atau PuTTY, seluruh interaksi terjadi di tab browser."
                    ]
                },
                {
                    "title": "📶 3. Web Bluetooth API & Modul HC-06",
                    "color": COLOR_WARNING,
                    "points": [
                        "Koneksi Nirkabel Bluetooth: Web browser menghubungkan modul HC-06 melalui layanan Bluetooth SPP/UART secara wireless.",
                        "Format Perintah WiFi Provisioning: Web mengirim 'set:SSID,PASSWORD' untuk menyimpan kredensial ke flash memory ESP32.",
                        "Fallback SoftAP: Jika WiFi belum terkonfigurasi, ESP32 memancarkan WiFi AP 'ESP32-C3-EWS' agar HP dapat mengakses halaman setup."
                    ]
                }
            ]
        },

        # SLIDE 5
        {
            "slide_num": "05",
            "category": "SECURITY & RELIABILITY",
            "title": "Keamanan, Keandalan & Otomasi Peringatan Dini",
            "subtitle": "Fondasi Robustness Sistem untuk Menjamin Kesiapsiagaan Menghadapi Bencana Gempa & Banjir",
            "cards": [
                {
                    "title": "💾 Penyimpanan Flash Non-Volatile (NVS)",
                    "color": COLOR_ACCENT,
                    "points": [
                        "Library Preferences.h: SSID dan password WiFi tersimpan aman pada partisi flash NVS ('wifi_cfg').",
                        "Power-Loss Immune: Konfigurasi tidak hilang meskipun suplai listrik padam mendadak atau ESP32 di-restart.",
                        "Pembersihan Mudah: Perintah 'hapus wifi' atau endpoint POST /api/wifi memungkinkan reset konfigurasi secara instan."
                    ]
                },
                {
                    "title": "🚨 Audio-Visual EWS Alarm Automation",
                    "color": COLOR_WARNING,
                    "points": [
                        "Deteksi Ambang Batas Otomatis: Web memicu status SIAGA/AWAS jika getaran PGA melampaui 0.05g atau Muka Air > 100cm.",
                        "Peringatan Audio Multitonal: Sirine darurat berbasis Web Audio API langsung berbunyi di browser petugas saat bahaya terdeteksi.",
                        "Modal Peringatan Evakuasi: Muncul dialog darurat interaktif lengkap dengan panduan mitigasi dan titik evakuasi terdekat."
                    ]
                },
                {
                    "title": "🔒 Arsitektur Keamanan & Skalabilitas",
                    "color": COLOR_EMERALD,
                    "points": [
                        "Proteksi M2M IoT: Endpoint telemetri dilindungi dengan verifikasi skema JSON dan penanganan CSRF yang disesuaikan untuk IoT.",
                        "CORS Header Terkendali: Memastikan data sensor aman diakses oleh web dashboard resmi dan aplikasi mobile pendukung.",
                        "Multi-Station Architecture: Struktur database mendukung skalabilitas banyak stasiun sensor (multi-node) di berbagai penjuru wilayah."
                    ]
                }
            ]
        }
    ]

    for data in slides_data:
        slide = prs.slides.add_slide(blank_layout)

        # Background Fill
        bg = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, prs.slide_width, prs.slide_height)
        bg.fill.solid()
        bg.fill.fore_color.rgb = COLOR_BG
        bg.line.fill.background()

        # Top Category & Slide Number Badge
        cat_box = slide.shapes.add_textbox(Inches(0.8), Inches(0.4), Inches(10), Inches(0.4))
        tf_cat = cat_box.text_frame
        tf_cat.word_wrap = True
        p_cat = tf_cat.paragraphs[0]
        p_cat.text = f"SLIDE {data['slide_num']}  //  {data['category']}"
        p_cat.font.name = "Arial"
        p_cat.font.size = Pt(11)
        p_cat.font.bold = True
        p_cat.font.color.rgb = COLOR_ACCENT

        # Slide Main Title
        title_box = slide.shapes.add_textbox(Inches(0.8), Inches(0.75), Inches(11.7), Inches(0.8))
        tf_title = title_box.text_frame
        tf_title.word_wrap = True
        p_title = tf_title.paragraphs[0]
        p_title.text = data["title"]
        p_title.font.name = "Arial"
        p_title.font.size = Pt(22)
        p_title.font.bold = True
        p_title.font.color.rgb = COLOR_WHITE

        # Slide Subtitle
        sub_box = slide.shapes.add_textbox(Inches(0.8), Inches(1.4), Inches(11.7), Inches(0.5))
        tf_sub = sub_box.text_frame
        tf_sub.word_wrap = True
        p_sub = tf_sub.paragraphs[0]
        p_sub.text = data["subtitle"]
        p_sub.font.name = "Arial"
        p_sub.font.size = Pt(12)
        p_sub.font.color.rgb = COLOR_MUTED

        # Render 3 Columns / Cards
        card_width = Inches(3.64)
        card_height = Inches(4.8)
        card_gap = Inches(0.38)
        left_margin = Inches(0.8)
        top_margin = Inches(2.0)

        for i, card in enumerate(data["cards"]):
            c_left = left_margin + i * (card_width + card_gap)
            
            # Card Background Shape
            card_shape = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, c_left, top_margin, card_width, card_height)
            card_shape.fill.solid()
            card_shape.fill.fore_color.rgb = COLOR_CARD
            card_shape.line.color.rgb = COLOR_CARD_BORDER
            card_shape.line.width = Pt(1.5)

            # Card Header Color Accent Line
            line_shape = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, c_left + Inches(0.2), top_margin + Inches(0.2), card_width - Inches(0.4), Pt(3))
            line_shape.fill.solid()
            line_shape.fill.fore_color.rgb = card["color"]
            line_shape.line.fill.background()

            # Card Title Box
            ctitle_box = slide.shapes.add_textbox(c_left + Inches(0.2), top_margin + Inches(0.3), card_width - Inches(0.4), Inches(0.75))
            tf_ctitle = ctitle_box.text_frame
            tf_ctitle.word_wrap = True
            p_ctitle = tf_ctitle.paragraphs[0]
            p_ctitle.text = card["title"]
            p_ctitle.font.name = "Arial"
            p_ctitle.font.size = Pt(13)
            p_ctitle.font.bold = True
            p_ctitle.font.color.rgb = card["color"]

            # Card Content (Bullet Points with detailed explanations)
            content_box = slide.shapes.add_textbox(c_left + Inches(0.15), top_margin + Inches(1.05), card_width - Inches(0.3), Inches(3.5))
            tf_content = content_box.text_frame
            tf_content.word_wrap = True

            for idx, pt in enumerate(card["points"]):
                p = tf_content.add_paragraph() if idx > 0 else tf_content.paragraphs[0]
                p.text = "• " + pt
                p.font.name = "Calibri"
                p.font.size = Pt(11)
                p.font.color.rgb = RGBColor(226, 232, 240)
                p.space_after = Pt(10)
                p.line_spacing = 1.15

        # Footer
        footer_box = slide.shapes.add_textbox(Inches(0.8), Inches(6.95), Inches(11.7), Inches(0.3))
        tf_footer = footer_box.text_frame
        p_footer = tf_footer.paragraphs[0]
        p_footer.text = "GeoShield EWS — Sistem Informasi Geografis & Early Warning System Berbasis IoT ESP32"
        p_footer.font.name = "Arial"
        p_footer.font.size = Pt(9)
        p_footer.font.color.rgb = RGBColor(100, 116, 139)

    output_path = "presentation_geoshield_ews.pptx"
    prs.save(output_path)
    print(f"File presentasi PowerPoint berhasil dibuat: {output_path}")

if __name__ == "__main__":
    create_presentation()
