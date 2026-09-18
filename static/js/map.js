/**
 * GeoShield EWS - Google Maps Connected Real-Time GIS Mapping Engine
 * 
 * Fitur Utama:
 * 1. Citra Satelit & Jalan Asli Google Maps (Google Hybrid lyrs=y & Google Road lyrs=m)
 * 2. Navigasi Real Turn-by-Turn Jalan Raya menggunakan OSRM Driving Engine
 * 3. Link Langsung ke Google Maps Navigation untuk setiap rute evakuasi & shelter
 * 4. Deteksi GPS Presisi Tinggi (HTML5 Geolocation High Accuracy)
 * 5. Zona Bahaya Gempa 25 KM dengan Apex Tag Dinamis dari Sensor ESP32 (MPU-6050)
 */

class GeoMappingEngine {
    constructor() {
        // Koordinat Utama Stasiun EWS ITERA (Institut Teknologi Sumatera, Lampung)
        this.station = {
            id: 'ST-01-ESP32',
            name: 'Stasiun EWS ITERA (ESP32-C3)',
            lat: -5.3582,
            lng: 105.3146
        };

        // Daftar Posko / Shelter Evakuasi Riil di sekitar ITERA & Bandar Lampung
        this.shelters = [
            {
                id: 1,
                name: 'Posko 1 (GOR ITERA / Embung A)',
                lat: -5.3615,
                lng: 105.3128,
                type: 'green',
                num: 1,
                dist: '1.2 km',
                eta: '4 mins',
                color: '#06B6D4'
            },
            {
                id: 2,
                name: 'Posko 2 (RSUD Airan Raya)',
                lat: -5.3725,
                lng: 105.3056,
                type: 'red',
                num: 2,
                dist: '3.8 km',
                eta: '9 mins',
                color: '#38BDF8'
            },
            {
                id: 3,
                name: 'Posko 3 (Balai Desa Way Huwi)',
                lat: -5.3850,
                lng: 105.2950,
                type: 'green',
                num: 1,
                dist: '6.4 km',
                eta: '14 mins',
                color: '#F59E0B'
            },
            {
                id: 4,
                name: 'Posko 4 (Stadion PKOR Way Halim)',
                lat: -5.3900,
                lng: 105.2750,
                type: 'red',
                num: 2,
                dist: '9.8 km',
                eta: '22 mins',
                color: '#10B981'
            }
        ];

        this.leafletMap = null;
        this.stationMarker = null;
        this.quakeRadiusCircle = null;
        this.apexBadgeMarker = null;
        this.routePolylines = [];
        this.shelterMarkers = [];

        this.currentRadiusKm = 25; // Default 25 KM matching reference photo '25MI'
        this.currentLayerMode = 'google-hybrid'; // 'google-hybrid', 'google-road', 'google-terrain', 'osm'
        this.activeRouteColor = '#06B6D4';

        // Layer Instances
        this.layerGoogleHybrid = null;
        this.layerGoogleRoad = null;
        this.layerGoogleTerrain = null;
        this.layerOsm = null;

        this.init();
    }

    init() {
        this.initMapWithGoogleTiles();
        this.renderStationEpicenter();
        this.renderQuakeRadiusZone(this.currentRadiusKm);
        this.calculateRealRoadRoutes();
        this.renderShelterMarkers();
        this.bindEvents();
    }

    // Inisialisasi Peta menggunakan Google Maps Hybrid Tiles
    initMapWithGoogleTiles() {
        const container = document.getElementById('leaflet-map');
        if (!container) return;

        // Centered at ITERA Lampung (-5.3582, 105.3146)
        this.leafletMap = L.map('leaflet-map', {
            center: [this.station.lat, this.station.lng],
            zoom: 13,
            zoomControl: false,
            attributionControl: false
        });

        // 1. Google Maps Hybrid (Satelit Asli + Jalan Raya + Label Nama Tempat)
        this.layerGoogleHybrid = L.tileLayer('https://mt1.google.com/vt/lyrs=y&x={x}&y={y}&z={z}', {
            maxZoom: 20,
            subdomains: ['mt0', 'mt1', 'mt2', 'mt3']
        }).addTo(this.leafletMap);

        // 2. Google Maps Standard Roads
        this.layerGoogleRoad = L.tileLayer('https://mt1.google.com/vt/lyrs=m&x={x}&y={y}&z={z}', {
            maxZoom: 20,
            subdomains: ['mt0', 'mt1', 'mt2', 'mt3']
        });

        // 3. Google Maps Terrain
        this.layerGoogleTerrain = L.tileLayer('https://mt1.google.com/vt/lyrs=p&x={x}&y={y}&z={z}', {
            maxZoom: 20,
            subdomains: ['mt0', 'mt1', 'mt2', 'mt3']
        });

        // 4. OpenStreetMap Standard
        this.layerOsm = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
            maxZoom: 19
        });
    }

    // Render Epicenter / Node Sensor ESP32
    renderStationEpicenter() {
        if (!this.leafletMap) return;

        const pulseIcon = L.divIcon({
            className: 'epicenter-radar-marker',
            html: `
                <div class="epicenter-pulse"></div>
                <div class="epicenter-core" title="${this.station.name}"></div>
            `,
            iconSize: [32, 32],
            iconAnchor: [16, 16]
        });

        this.stationMarker = L.marker([this.station.lat, this.station.lng], { icon: pulseIcon }).addTo(this.leafletMap);
        this.updateStationPopup();
    }

    updateStationPopup() {
        if (!this.stationMarker) return;
        const gmapsLink = `https://www.google.com/maps/search/?api=1&query=${this.station.lat},${this.station.lng}`;
        this.stationMarker.bindPopup(`
            <div style="color: #0F172A; font-family: sans-serif; font-size: 12px; line-height: 1.4;">
                <strong style="color: #1D4ED8; font-size: 13px;">📍 ${this.station.name}</strong><br>
                <span>ID Node: <code>${this.station.id}</code></span><br>
                <span>Koordinat Real Google Maps: <strong>${this.station.lat.toFixed(5)}, ${this.station.lng.toFixed(5)}</strong></span><br>
                <div style="margin-top: 6px; padding: 4px 8px; background: #FEE2E2; color: #DC2626; border-radius: 4px; font-weight: bold;">
                    ⚠️ Titik Pusat Sensor Gempa ESP32 (Aktif)
                </div>
                <div style="margin-top: 6px;">
                    <a href="${gmapsLink}" target="_blank" style="color: #0284C7; font-weight: bold; text-decoration: none; display: inline-flex; align-items: center; gap: 4px;">
                        <i class="fa-brands fa-google"></i> Buka Titik di Google Maps &rarr;
                    </a>
                </div>
            </div>
        `);
    }

    // Render Lingkaran Bahaya Gempa Merah dengan Apex Tag '25 KM'
    renderQuakeRadiusZone(radiusKm) {
        this.currentRadiusKm = radiusKm;
        const radiusMeters = radiusKm * 1000;

        if (this.quakeRadiusCircle && this.leafletMap) {
            this.leafletMap.removeLayer(this.quakeRadiusCircle);
        }
        if (this.apexBadgeMarker && this.leafletMap) {
            this.leafletMap.removeLayer(this.apexBadgeMarker);
        }

        // Lingkaran Merah Bahaya Gempa
        this.quakeRadiusCircle = L.circle([this.station.lat, this.station.lng], {
            radius: radiusMeters,
            color: '#DC2626',
            weight: 2.2,
            opacity: 0.95,
            fillColor: '#EF4444',
            fillOpacity: 0.32
        }).addTo(this.leafletMap);

        this.quakeRadiusCircle.bindPopup(`
            <div style="color: #0F172A; font-family: sans-serif; font-size: 12px;">
                <strong style="color: #DC2626; font-size: 13px;">🔴 ZONA BAHAYA GUNCANGAN GEMPA</strong><br>
                <span>Estimasi Radius Kerusakan: <strong>${radiusKm} KM</strong></span><br>
                <span>Data Sensor ESP32 (MPU-6050): PGA &ge; 0.040g (MMI VI+)</span><br>
                <small style="color: #64748B;">Jalur evakuasi diarahkan keluar dari lingkaran ini menuju Google Maps safe shelter.</small>
            </div>
        `);

        // Tag Merah 25 KM di Puncak Lingkaran (Matches Screenshot '25MI' / '25 KM')
        const latOffset = radiusKm / 111.32;
        const apexLat = this.station.lat + latOffset;
        const apexLng = this.station.lng;

        const apexBadgeIcon = L.divIcon({
            className: 'quake-apex-badge-wrap',
            html: `<div class="quake-apex-badge">${radiusKm} KM</div>`,
            iconSize: [80, 26],
            iconAnchor: [40, 13]
        });

        this.apexBadgeMarker = L.marker([apexLat, apexLng], {
            icon: apexBadgeIcon,
            interactive: true
        }).addTo(this.leafletMap);

        // Update Text Pill di Bottom Dock
        const pillText = document.querySelector('#pill-radius-quake span');
        if (pillText) {
            pillText.innerHTML = `Radius Gempa ESP32 (PGA): <strong>${radiusKm} KM</strong>`;
        }
    }

    // Kalkulasi Rute Jalan Nyata menggunakan OSRM Driving Engine (Turn-by-Turn Mengikuti Jalan Riil)
    async calculateRealRoadRoutes() {
        if (!this.leafletMap) return;

        // Bersihkan polylines sebelumnya
        this.routePolylines.forEach(p => this.leafletMap.removeLayer(p));
        this.routePolylines = [];

        // Rute untuk masing-masing shelter
        for (let i = 0; i < this.shelters.length; i++) {
            const shelter = this.shelters[i];
            const originLat = this.station.lat;
            const originLng = this.station.lng;
            const destLat = shelter.lat;
            const destLng = shelter.lng;

            try {
                // Request OSRM Real Road Driving Geometry
                const url = `https://router.project-osrm.org/route/v1/driving/${originLng},${originLat};${destLng},${destLat}?overview=full&geometries=geojson`;
                const response = await fetch(url);
                const data = await response.json();

                if (data.routes && data.routes.length > 0) {
                    const route = data.routes[0];
                    const coords = route.geometry.coordinates.map(pt => [pt[1], pt[0]]);
                    const distanceKm = (route.distance / 1000).toFixed(1);
                    const durationMins = Math.round(route.duration / 60);

                    // Gambar Polyline di Peta mengikuti jalan raya Google Maps
                    const polyline = L.polyline(coords, {
                        color: shelter.color,
                        weight: 5,
                        opacity: 0.9,
                        lineCap: 'round',
                        lineJoin: 'round'
                    }).addTo(this.leafletMap);

                    const gmapsUrl = `https://www.google.com/maps/dir/?api=1&origin=${originLat},${originLng}&destination=${destLat},${destLng}&travelmode=driving`;

                    polyline.bindPopup(`
                        <div style="font-family: sans-serif; font-size: 12px; color: #0F172A;">
                            <strong style="color: ${shelter.color}; font-size: 13px;">${shelter.name}</strong><br>
                            <span>Jarak Riil Jalan: <strong>${distanceKm} km</strong></span><br>
                            <span>Waktu Tempuh Kendaraan: <strong>${durationMins} menit</strong></span><br>
                            <div style="margin-top: 8px;">
                                <a href="${gmapsUrl}" target="_blank" style="display: inline-block; background: #0284C7; color: #FFF; padding: 4px 8px; border-radius: 4px; text-decoration: none; font-weight: bold;">
                                    <i class="fa-brands fa-google"></i> Navigasi Google Maps &rarr;
                                </a>
                            </div>
                        </div>
                    `);

                    this.routePolylines.push(polyline);

                    // Update UI teks jarak di Floating Panel
                    const distElement = document.getElementById(`r${i+1}-dist-text`);
                    if (distElement) {
                        distElement.textContent = `${distanceKm} km — ${durationMins} mins`;
                    }
                }
            } catch (err) {
                console.warn(`[OSRM] Fallback to direct path for shelter ${shelter.id}:`, err.message);
                // Fallback geometry jika koneksi OSRM lambat
                const fallbackCoords = [
                    [originLat, originLng],
                    [(originLat + destLat) / 2, originLng],
                    [destLat, destLng]
                ];
                const fallbackLine = L.polyline(fallbackCoords, {
                    color: shelter.color,
                    weight: 4.5,
                    opacity: 0.85
                }).addTo(this.leafletMap);
                this.routePolylines.push(fallbackLine);
            }
        }
    }

    // Render Marker Shelter Evakuasi (Kotak Hijau 1 & Merah 2 persis screenshot)
    renderShelterMarkers() {
        if (!this.leafletMap) return;

        this.shelterMarkers.forEach(m => this.leafletMap.removeLayer(m));
        this.shelterMarkers = [];

        this.shelters.forEach(s => {
            const badgeClass = (s.type === 'green') ? 'shelter-green' : 'shelter-red';
            const icon = L.divIcon({
                className: 'shelter-badge-wrap',
                html: `<div class="shelter-badge-marker ${badgeClass}">${s.num}</div>`,
                iconSize: [26, 26],
                iconAnchor: [13, 13]
            });

            const marker = L.marker([s.lat, s.lng], { icon: icon }).addTo(this.leafletMap);
            const gmapsUrl = `https://www.google.com/maps/dir/?api=1&origin=${this.station.lat},${this.station.lng}&destination=${s.lat},${s.lng}&travelmode=driving`;

            marker.bindPopup(`
                <div style="color: #0F172A; font-family: sans-serif; font-size: 12px; line-height: 1.4;">
                    <strong style="color: ${s.type === 'green' ? '#10B981' : '#EF4444'}; font-size: 13px;">${s.name}</strong><br>
                    <span>Status: <strong>${s.type === 'green' ? '🟢 Zona Aman Luar Radius Gempa' : '🔴 Posko Siaga Transit'}</strong></span><br>
                    <span>Koordinat Google Maps: <code>${s.lat.toFixed(5)}, ${s.lng.toFixed(5)}</code></span><br>
                    <div style="margin-top: 8px;">
                        <a href="${gmapsUrl}" target="_blank" style="display: inline-block; background: #0284C7; color: #FFF; padding: 4px 10px; border-radius: 4px; text-decoration: none; font-weight: bold;">
                            <i class="fa-brands fa-google"></i> Buka Navigasi di Google Maps &rarr;
                        </a>
                    </div>
                </div>
            `);
            this.shelterMarkers.push(marker);
        });
    }

    // Deteksi GPS Realtime Pengguna (Geolocation API Presisi Tinggi)
    detectUserGPS() {
        if (!navigator.geolocation) {
            alert('Browser Anda tidak mendukung deteksi lokasi GPS.');
            return;
        }

        if (window.showToast) window.showToast('Mencari sinyal GPS presisi tinggi...', 'info');

        navigator.geolocation.getCurrentPosition(
            (pos) => {
                const userLat = pos.coords.latitude;
                const userLng = pos.coords.longitude;
                const accuracyMeters = Math.round(pos.coords.accuracy);

                this.station.lat = userLat;
                this.station.lng = userLng;
                this.station.name = `Lokasi GPS Nyata Anda (Akurasi: ±${accuracyMeters}m)`;

                // Update Posisi Epicenter & Peta
                if (this.stationMarker) {
                    this.stationMarker.setLatLng([userLat, userLng]);
                    this.updateStationPopup();
                }

                // Update Form Input
                const in1 = document.getElementById('input-loc-1');
                if (in1) in1.value = `Lokasi GPS Saya (${userLat.toFixed(4)}, ${userLng.toFixed(4)})`;

                // Recenter & Recalculate
                this.leafletMap.setView([userLat, userLng], 14, { animate: true });
                this.renderQuakeRadiusZone(this.currentRadiusKm);
                this.calculateRealRoadRoutes();

                if (window.showToast) {
                    window.showToast(`GPS Terhubung! Akurasi: ${accuracyMeters} meter. Rute Google Maps diperbarui.`, 'success');
                }
            },
            (err) => {
                console.warn('Geolocation error:', err.message);
                alert(`Gagal mengambil koordinat GPS: ${err.message}\nPastikan izin lokasi (Location Permission) diizinkan di browser.`);
            },
            {
                enableHighAccuracy: true,
                timeout: 10000,
                maximumAge: 0
            }
        );
    }

    // Toggle Antara Google Hybrid, Google Roads, dan OSM
    toggleLayerMode() {
        if (this.currentLayerMode === 'google-hybrid') {
            this.leafletMap.removeLayer(this.layerGoogleHybrid);
            this.layerGoogleRoad.addTo(this.leafletMap);
            this.currentLayerMode = 'google-road';
            if (window.showToast) window.showToast('Mode Peta: Google Maps Road View 🛣️', 'info');
        } else if (this.currentLayerMode === 'google-road') {
            this.leafletMap.removeLayer(this.layerGoogleRoad);
            this.layerGoogleTerrain.addTo(this.leafletMap);
            this.currentLayerMode = 'google-terrain';
            if (window.showToast) window.showToast('Mode Peta: Google Maps Topografi & Terrain ⛰️', 'info');
        } else if (this.currentLayerMode === 'google-terrain') {
            this.leafletMap.removeLayer(this.layerGoogleTerrain);
            this.layerOsm.addTo(this.leafletMap);
            this.currentLayerMode = 'osm';
            if (window.showToast) window.showToast('Mode Peta: OpenStreetMap Standard 🗺️', 'info');
        } else {
            this.leafletMap.removeLayer(this.layerOsm);
            this.layerGoogleHybrid.addTo(this.leafletMap);
            this.currentLayerMode = 'google-hybrid';
            if (window.showToast) window.showToast('Mode Peta: Google Maps Satellite Hybrid 🛰️', 'info');
        }
    }

    // Terima pembaruan telemetri otomatis dari sensor ESP32
    updateFromTelemetry(telemetry) {
        if (!telemetry) return;

        // 1. Update koordinat stasiun jika ESP32 mengirim GPS hardware
        if (telemetry.location && telemetry.location.lat && telemetry.location.lng) {
            const newLat = parseFloat(telemetry.location.lat);
            const newLng = parseFloat(telemetry.location.lng);
            if (Math.abs(newLat - this.station.lat) > 0.0001 || Math.abs(newLng - this.station.lng) > 0.0001) {
                this.station.lat = newLat;
                this.station.lng = newLng;
                if (this.stationMarker) {
                    this.stationMarker.setLatLng([newLat, newLng]);
                    this.updateStationPopup();
                }
                this.calculateRealRoadRoutes();
            }
        }

        // 2. Hitung Ulang Radius Gempa Berdasarkan PGA MPU-6050
        let pga = 0.045;
        if (telemetry.seismic && telemetry.seismic.pga) {
            pga = parseFloat(telemetry.seismic.pga);
        } else if (telemetry.pga_g) {
            pga = parseFloat(telemetry.pga_g);
        }

        // Formula radius atenuasi gempa (km)
        const dynamicRadiusKm = Math.max(5, Math.min(80, Math.round(pga * 550)));
        if (dynamicRadiusKm !== this.currentRadiusKm) {
            this.renderQuakeRadiusZone(dynamicRadiusKm);
        }

        // Update Badge PGA di Panel Kiri
        const badgePga = document.getElementById('badge-loc-pga');
        if (badgePga) {
            badgePga.textContent = `PGA: ${pga.toFixed(3)}g`;
        }
    }

    bindEvents() {
        // Zoom buttons
        const btnZoomIn = document.getElementById('btn-zoom-in');
        const btnZoomOut = document.getElementById('btn-zoom-out');
        if (btnZoomIn) btnZoomIn.addEventListener('click', () => this.leafletMap.zoomIn());
        if (btnZoomOut) btnZoomOut.addEventListener('click', () => this.leafletMap.zoomOut());

        // Tombol Deteksi GPS Riil
        const btnGps = document.getElementById('btn-use-my-gps');
        if (btnGps) {
            btnGps.addEventListener('click', () => this.detectUserGPS());
        }

        // Reset / Undo View button
        const btnUndo = document.getElementById('top-btn-undo');
        if (btnUndo) {
            btnUndo.addEventListener('click', () => {
                this.leafletMap.setView([this.station.lat, this.station.lng], 13, { animate: true });
            });
        }

        // Tombol Ganti Layer Peta Google Maps
        const btnToggleSat = document.getElementById('btn-toggle-satellite');
        if (btnToggleSat) {
            btnToggleSat.addEventListener('click', () => this.toggleLayerMode());
        }

        // Fullscreen Toggle
        const btnFullscreen = document.getElementById('btn-fullscreen');
        if (btnFullscreen) {
            btnFullscreen.addEventListener('click', () => {
                if (!document.fullscreenElement) {
                    document.documentElement.requestFullscreen();
                } else {
                    document.exitFullscreen();
                }
            });
        }

        // Get Directions button (Kalkulasi Rute Jalan Real)
        const btnGetDirections = document.getElementById('btn-get-directions');
        if (btnGetDirections) {
            btnGetDirections.addEventListener('click', () => {
                this.calculateRealRoadRoutes();
                if (this.routePolylines.length > 0) {
                    const group = L.featureGroup(this.routePolylines);
                    this.leafletMap.fitBounds(group.getBounds(), { padding: [60, 60] });
                }
                if (window.showToast) window.showToast('Rute Jalan Nyata Google Maps berhasil dimuat!', 'success');
            });
        }

        // Lasso / Hitung Radius Bahaya
        const btnLasso = document.getElementById('btn-lasso-zone');
        if (btnLasso) {
            btnLasso.addEventListener('click', () => {
                if (this.leafletMap && this.quakeRadiusCircle) {
                    this.leafletMap.fitBounds(this.quakeRadiusCircle.getBounds(), { padding: [50, 50] });
                    if (window.showToast) window.showToast(`Fokus Zona Bahaya Gempa: Radius ${this.currentRadiusKm} KM`, 'info');
                }
            });
        }

        // Delete All button (bottom dock)
        const btnDeleteAll = document.getElementById('btn-dock-reset-all');
        if (btnDeleteAll) {
            btnDeleteAll.addEventListener('click', () => {
                if (confirm('Reset semua radius gempa dan rute evakuasi Google Maps?')) {
                    this.renderQuakeRadiusZone(25);
                    this.calculateRealRoadRoutes();
                    this.leafletMap.setView([this.station.lat, this.station.lng], 13);
                    if (window.showToast) window.showToast('Semua parameter GIS telah di-reset ke nilai awal.', 'success');
                }
            });
        }

        // Color Picker
        const colorNative = document.getElementById('route-color-native');
        const colorHex = document.getElementById('route-color-hex');
        const colorSwatch = document.getElementById('color-swatch-display');

        if (colorNative && colorHex && colorSwatch) {
            colorNative.addEventListener('input', (e) => {
                const c = e.target.value;
                colorHex.value = c;
                colorSwatch.style.backgroundColor = c;
                this.activeRouteColor = c;
                if (this.routePolylines[0]) this.routePolylines[0].setStyle({ color: c });
            });
            colorHex.addEventListener('change', (e) => {
                const c = e.target.value;
                colorNative.value = c;
                colorSwatch.style.backgroundColor = c;
                this.activeRouteColor = c;
                if (this.routePolylines[0]) this.routePolylines[0].setStyle({ color: c });
            });
        }
    }
}

// Instantiate on DOM ready
document.addEventListener('DOMContentLoaded', () => {
    window.geoMap = new GeoMappingEngine();
});
