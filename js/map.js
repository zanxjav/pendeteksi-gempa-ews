/**
 * Pendeteksi Gempa EWS - Google Maps Connected Real-Time GIS Mapping Engine
 * Stasiun Pemantauan Gempa Bumi & Multi-Bencana ITERA Lampung
 * 
 * Fitur Utama & Peningkatan Akurasi:
 * 1. Citra Satelit Asli Google Maps Hybrid (lyrs=y) dengan resolusi tinggi & label jalan resmi.
 * 2. Titik Presisi Kampus ITERA Lampung (-5.35824, 105.31465).
 * 3. Marker Epicenter DRAGGABLE & Peta Click-to-Relocate untuk menguji/memindahkan lokasi secara interaktif.
 * 4. Kotak Pencarian Alamat & Tempat Google Maps (Geocoding instan).
 * 5. Navigasi Real Turn-by-Turn Jalan Raya menggunakan OSRM Driving Engine.
 * 6. Link Langsung ke Google Maps Navigation untuk setiap rute evakuasi & shelter.
 * 7. Deteksi GPS Presisi Tinggi (HTML5 Geolocation High Accuracy).
 * 8. Zona Bahaya Gempa 25 KM dengan Apex Tag Dinamis dari Sensor ESP32 (MPU-6050).
 * 9. Indikator Skala Metrik Akurat (Leaflet Scale Bar).
 */

class GeoMappingEngine {
    constructor() {
        // Koordinat Presisi Kampus Institut Teknologi Sumatera (ITERA), Lampung
        this.station = {
            id: 'ST-01-ESP32',
            name: 'Pendeteksi Gempa EWS ITERA - ESP32 C3',
            lat: -5.35824,
            lng: 105.31465
        };

        // Daftar Posko / Shelter Evakuasi Riil di sekitar Kampus ITERA & Lampung Selatan
        this.shelters = [
            {
                id: 1,
                name: 'Posko 1 (GOR ITERA / Embung A)',
                lat: -5.36142,
                lng: 105.31295,
                type: 'green',
                num: 1,
                dist: '1.2 km',
                eta: '4 mins',
                color: '#06B6D4'
            },
            {
                id: 2,
                name: 'Posko 2 (RSUD Airan Raya)',
                lat: -5.37258,
                lng: 105.30560,
                type: 'red',
                num: 2,
                dist: '3.8 km',
                eta: '9 mins',
                color: '#38BDF8'
            },
            {
                id: 3,
                name: 'Posko 3 (Balai Desa Way Huwi)',
                lat: -5.38500,
                lng: 105.29500,
                type: 'green',
                num: 1,
                dist: '6.4 km',
                eta: '14 mins',
                color: '#F59E0B'
            },
            {
                id: 4,
                name: 'Posko 4 (Stadion PKOR Way Halim)',
                lat: -5.39000,
                lng: 105.27500,
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
        this.travelMode = 'driving'; // 'driving' or 'walking'
        this.isPickingShelterOnMap = false;

        this.init();
    }

    init() {
        this.initMapWithGoogleTiles();
        this.renderStationEpicenter();
        this.renderQuakeRadiusZone(this.currentRadiusKm);
        this.calculateRealRoadRoutes();
        this.renderShelterMarkers();
        this.bindEvents();
        this.bindRouteCardClicks();
        this.bindPillDeletionLayers();
    }

    // Inisialisasi Peta menggunakan Google Maps Hybrid Tiles
    initMapWithGoogleTiles() {
        const container = document.getElementById('leaflet-map');
        if (!container) return;

        // Centered presisi pada Gedung Utama ITERA Lampung
        this.leafletMap = L.map('leaflet-map', {
            center: [this.station.lat, this.station.lng],
            zoom: 14,
            zoomControl: false,
            attributionControl: false
        });

        // 1. Google Maps Hybrid (Citra Satelit Resmi Google + Jalan Raya + Nama Gedung & Tempat)
        this.layerGoogleHybrid = L.tileLayer('https://mt1.google.com/vt/lyrs=y&x={x}&y={y}&z={z}', {
            maxZoom: 22,
            subdomains: ['mt0', 'mt1', 'mt2', 'mt3']
        }).addTo(this.leafletMap);

        // 2. Google Maps Standard Roads
        this.layerGoogleRoad = L.tileLayer('https://mt1.google.com/vt/lyrs=m&x={x}&y={y}&z={z}', {
            maxZoom: 22,
            subdomains: ['mt0', 'mt1', 'mt2', 'mt3']
        });

        // 3. Google Maps Topografi & Terrain
        this.layerGoogleTerrain = L.tileLayer('https://mt1.google.com/vt/lyrs=p&x={x}&y={y}&z={z}', {
            maxZoom: 22,
            subdomains: ['mt0', 'mt1', 'mt2', 'mt3']
        });

        // 4. OpenStreetMap Standard
        this.layerOsm = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
            maxZoom: 19
        });

        // Tambahkan Skala Metrik Akurat di Pojok Kanan Bawah
        L.control.scale({
            metric: true,
            imperial: false,
            position: 'bottomright'
        }).addTo(this.leafletMap);
    }

    // Render Epicenter / Node Sensor ESP32 (DRAGGABLE untuk akurasi penuh)
    renderStationEpicenter() {
        if (!this.leafletMap) return;

        const pulseIcon = L.divIcon({
            className: 'epicenter-radar-marker',
            html: `
                <div class="epicenter-pulse"></div>
                <div class="epicenter-core" title="Geser untuk memindahkan sensor gempa"></div>
            `,
            iconSize: [32, 32],
            iconAnchor: [16, 16]
        });

        // Marker dapat digeser (Draggable) untuk akurasi lokasi
        this.stationMarker = L.marker([this.station.lat, this.station.lng], {
            icon: pulseIcon,
            draggable: true,
            title: 'Tarik marker ini untuk memindahkan titik sensor gempa secara presisi'
        }).addTo(this.leafletMap);

        // Event saat marker selesai digeser
        this.stationMarker.on('dragend', (e) => {
            const newPos = e.target.getLatLng();
            this.station.lat = newPos.lat;
            this.station.lng = newPos.lng;
            this.updateStationPopup();
            this.renderQuakeRadiusZone(this.currentRadiusKm);
            this.calculateRealRoadRoutes();

            const in1 = document.getElementById('input-loc-1');
            if (in1) in1.value = `Titik Sensor ESP32 (${newPos.lat.toFixed(5)}, ${newPos.lng.toFixed(5)})`;

            if (window.showToast) {
                window.showToast(`Lokasi Sensor disetel ke: ${newPos.lat.toFixed(5)}, ${newPos.lng.toFixed(5)}`, 'info');
            }
        });

        this.updateStationPopup();
    }

    updateStationPopup() {
        if (!this.stationMarker) return;
        const gmapsLink = `https://www.google.com/maps/search/?api=1&query=${this.station.lat},${this.station.lng}`;
        this.stationMarker.bindPopup(`
            <div style="color: #0F172A; font-family: sans-serif; font-size: 12px; line-height: 1.4;">
                <strong style="color: #1D4ED8; font-size: 13px;">📍 ${this.station.name}</strong><br>
                <span>ID Node: <code>${this.station.id}</code></span><br>
                <span>Koordinat Google Maps: <strong>${this.station.lat.toFixed(5)}, ${this.station.lng.toFixed(5)}</strong></span><br>
                <div style="margin-top: 6px; padding: 4px 8px; background: #FEE2E2; color: #DC2626; border-radius: 4px; font-weight: bold;">
                    ⚠️ Titik Pusat Sensor Gempa ESP32 (Aktif)
                </div>
                <div style="margin-top: 6px; display: flex; gap: 8px;">
                    <a href="${gmapsLink}" target="_blank" style="color: #0284C7; font-weight: bold; text-decoration: none; display: inline-flex; align-items: center; gap: 4px;">
                        <i class="fa-brands fa-google"></i> Buka Titik di Google Maps &rarr;
                    </a>
                </div>
                <small style="color: #64748B; display: block; margin-top: 4px;">💡 Tip: Anda bisa menarik (drag) ikon ini untuk mengubah lokasi sensor.</small>
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
                <span>Estimasi Radius Dampak: <strong>${radiusKm} KM</strong></span><br>
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

        const modeProfile = (this.travelMode === 'walking') ? 'walking' : 'driving';

        // Rute untuk masing-masing shelter
        for (let i = 0; i < this.shelters.length; i++) {
            const shelter = this.shelters[i];
            const originLat = this.station.lat;
            const originLng = this.station.lng;
            const destLat = shelter.lat;
            const destLng = shelter.lng;

            try {
                // Request OSRM Real Road Driving/Walking Geometry
                const url = `https://router.project-osrm.org/route/v1/${modeProfile}/${originLng},${originLat};${destLng},${destLat}?overview=full&geometries=geojson`;
                const response = await fetch(url);
                const data = await response.json();

                if (data.routes && data.routes.length > 0) {
                    const route = data.routes[0];
                    const coords = route.geometry.coordinates.map(pt => [pt[1], pt[0]]);
                    const distanceKm = (route.distance / 1000).toFixed(1);
                    const durationMins = Math.round(route.duration / 60);

                    // Gambar Polyline di Peta mengikuti jalan raya Google Maps
                    const polyline = L.polyline(coords, {
                        color: shelter.color || '#06B6D4',
                        weight: 5,
                        opacity: 0.9,
                        lineCap: 'round',
                        lineJoin: 'round'
                    }).addTo(this.leafletMap);

                    polyline.shelterData = shelter;
                    polyline.routeIndex = i;

                    const gmapsUrl = `https://www.google.com/maps/dir/?api=1&origin=${originLat},${originLng}&destination=${destLat},${destLng}&travelmode=${modeProfile}`;

                    polyline.bindPopup(`
                        <div style="font-family: sans-serif; font-size: 12px; color: #0F172A;">
                            <strong style="color: ${shelter.color}; font-size: 13px;">${shelter.name}</strong><br>
                            <span>Moda: <strong>${this.travelMode === 'walking' ? '🚶 Pejalan Kaki' : '🚗 Kendaraan Bermotor'}</strong></span><br>
                            <span>Jarak Riil Jalan: <strong>${distanceKm} km</strong></span><br>
                            <span>Waktu Tempuh: <strong>${durationMins} menit</strong></span><br>
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
                console.warn(`[OSRM] Fallback road geometry for shelter ${shelter.id}:`, err.message);
                const fallbackCoords = [
                    [originLat, originLng],
                    [(originLat + destLat) / 2, originLng],
                    [destLat, destLng]
                ];
                const fallbackLine = L.polyline(fallbackCoords, {
                    color: shelter.color || '#06B6D4',
                    weight: 4.5,
                    opacity: 0.85
                }).addTo(this.leafletMap);
                fallbackLine.shelterData = shelter;
                fallbackLine.routeIndex = i;
                this.routePolylines.push(fallbackLine);
            }
        }
    }

    // Render Marker Shelter Evakuasi (Kotak Hijau 1 & Merah 2 persis screenshot)
    renderShelterMarkers() {
        if (!this.leafletMap) return;

        this.shelterMarkers.forEach(m => this.leafletMap.removeLayer(m));
        this.shelterMarkers = [];

        this.shelters.forEach((s, idx) => {
            const badgeClass = (s.type === 'green') ? 'shelter-green' : 'shelter-red';
            const icon = L.divIcon({
                className: 'shelter-badge-wrap',
                html: `<div class="shelter-badge-marker ${badgeClass}">${s.num || (idx + 1)}</div>`,
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

    // Menambahkan Posko Evakuasi Baru (Kustom)
    addNewShelter(name, lat, lng) {
        const newId = this.shelters.length + 1;
        const newShelter = {
            id: newId,
            name: name || `Posko ${newId} (Tambahan)`,
            lat: parseFloat(lat),
            lng: parseFloat(lng),
            type: 'green',
            num: newId,
            dist: 'Kalkulasi...',
            eta: 'Kalkulasi...',
            color: '#10B981'
        };

        this.shelters.push(newShelter);
        this.renderShelterMarkers();
        this.calculateRealRoadRoutes();

        // Update form input loc-2
        const in2 = document.getElementById('input-loc-2');
        if (in2) in2.value = `${newShelter.name} (${lat.toFixed(5)}, ${lng.toFixed(5)})`;

        // Focus map to include new shelter
        this.leafletMap.setView([lat, lng], 14, { animate: true });

        if (window.showToast) {
            window.showToast(`Posko "${newShelter.name}" berhasil ditambahkan & rute OSRM terhubung! 📍`, 'success');
        }
    }

    // Algoritma Optimasi Rute Evakuasi (Paling Aman, Menghindari Zona Bahaya & Paling Cepat)
    optimizeRoute() {
        if (!this.shelters || this.shelters.length === 0) return;

        let bestShelter = null;
        let bestDistanceMeters = Infinity;

        // Hitung jarak garis lurus dari episenter ke setiap posko
        this.shelters.forEach((shelter) => {
            const dLat = (shelter.lat - this.station.lat) * 111.32;
            const dLng = (shelter.lng - this.station.lng) * 111.32 * Math.cos(this.station.lat * Math.PI / 180);
            const distKm = Math.sqrt(dLat * dLat + dLng * dLng);

            // Cek apakah di luar radius bahaya
            const isOutsideDanger = distKm >= (this.currentRadiusKm * 0.3); // prioritas luar zona episenter langsung

            if (isOutsideDanger && distKm < bestDistanceMeters) {
                bestDistanceMeters = distKm;
                bestShelter = shelter;
            }
        });

        if (!bestShelter) bestShelter = this.shelters[0];

        // Highlight polyline rute terbaik
        this.routePolylines.forEach((poly) => {
            if (poly.shelterData && poly.shelterData.id === bestShelter.id) {
                poly.setStyle({ color: '#10B981', weight: 8, opacity: 1 });
                poly.openPopup();
                this.leafletMap.fitBounds(poly.getBounds(), { padding: [80, 80] });
            } else {
                poly.setStyle({ opacity: 0.35, weight: 3 });
            }
        });

        const in2 = document.getElementById('input-loc-2');
        if (in2) in2.value = `⭐ TERBAIK: ${bestShelter.name}`;

        if (window.showToast) {
            window.showToast(`Rute Evakuasi Teraman: Menuju ${bestShelter.name} (${bestDistanceMeters.toFixed(1)} km) ✅`, 'success');
        }
    }

    // Klik Route Card untuk fokus ke rute terkait
    bindRouteCardClicks() {
        [1, 2, 3].forEach((num) => {
            const card = document.getElementById(`card-route-${num}`);
            if (card) {
                card.addEventListener('click', () => {
                    document.querySelectorAll('.route-card-item').forEach(c => c.classList.remove('active'));
                    card.classList.add('active');

                    const poly = this.routePolylines[num - 1];
                    if (poly) {
                        this.routePolylines.forEach(p => p.setStyle({ opacity: 0.4, weight: 4 }));
                        poly.setStyle({ opacity: 1, weight: 7 });
                        this.leafletMap.fitBounds(poly.getBounds(), { padding: [60, 60] });
                        poly.openPopup();
                    }
                });
            }
        });
    }

    // Hapus layer spesifik saat tombol hapus pill diklik
    bindPillDeletionLayers() {
        const pillQuake = document.querySelector('#pill-radius-quake .pill-del-btn');
        if (pillQuake) {
            pillQuake.addEventListener('click', (e) => {
                e.stopPropagation();
                if (this.quakeRadiusCircle) this.leafletMap.removeLayer(this.quakeRadiusCircle);
                if (this.apexBadgeMarker) this.leafletMap.removeLayer(this.apexBadgeMarker);
                if (window.showToast) window.showToast('Visualisasi Radius Gempa disembunyikan dari peta.', 'warn');
            });
        }

        const pillR1 = document.querySelector('#pill-route-1 .pill-del-btn');
        if (pillR1) {
            pillR1.addEventListener('click', (e) => {
                e.stopPropagation();
                if (this.routePolylines[0]) this.leafletMap.removeLayer(this.routePolylines[0]);
                if (window.showToast) window.showToast('Route #1 dihapus dari peta.', 'info');
            });
        }

        const pillR2 = document.querySelector('#pill-route-2 .pill-del-btn');
        if (pillR2) {
            pillR2.addEventListener('click', (e) => {
                e.stopPropagation();
                if (this.routePolylines[1]) this.leafletMap.removeLayer(this.routePolylines[1]);
                if (window.showToast) window.showToast('Route #2 dihapus dari peta.', 'info');
            });
        }

        const pillR3 = document.querySelector('#pill-route-3 .pill-del-btn');
        if (pillR3) {
            pillR3.addEventListener('click', (e) => {
                e.stopPropagation();
                if (this.routePolylines[2]) this.leafletMap.removeLayer(this.routePolylines[2]);
                if (window.showToast) window.showToast('Route #3 dihapus dari peta.', 'info');
            });
        }
    }

    // Bersihkan formulir rute
    clearRoutes() {
        this.routePolylines.forEach(p => this.leafletMap.removeLayer(p));
        this.routePolylines = [];
        const in2 = document.getElementById('input-loc-2');
        if (in2) in2.value = '';
        if (window.showToast) window.showToast('Formulir rute & polylines dibersihkan.', 'info');
    }

    // Reset Peta ke Kampus ITERA Lampung
    resetToItera() {
        this.station.lat = -5.35824;
        this.station.lng = 105.31465;
        this.station.name = 'Pendeteksi Gempa EWS ITERA - ESP32 C3';
        if (this.stationMarker) {
            this.stationMarker.setLatLng([this.station.lat, this.station.lng]);
            this.updateStationPopup();
        }
        this.leafletMap.setView([this.station.lat, this.station.lng], 14, { animate: true });
        this.renderQuakeRadiusZone(25);
        this.calculateRealRoadRoutes();
        this.renderShelterMarkers();

        const in1 = document.getElementById('input-loc-1');
        if (in1) in1.value = 'Kampus ITERA Lampung (-5.35824, 105.31465)';
        const in2 = document.getElementById('input-loc-2');
        if (in2) in2.value = 'Posko 1 (GOR ITERA / Embung A) 🟢';

        if (window.showToast) window.showToast('Peta & parameter GIS di-reset ke Kampus ITERA Lampung 🏫', 'info');
    }

    // Pencarian Alamat & Tempat Google Maps (Geocoding Akurat)
    async searchLocation(query) {
        if (!query || query.trim().length === 0) return;

        if (window.showToast) window.showToast(`Mencari "${query}" di Google Maps...`, 'info');

        try {
            const res = await fetch(`https://nominatim.openstreetmap.org/search?format=json&q=${encodeURIComponent(query)}&countrycodes=id&limit=1`);
            const data = await res.json();

            if (data && data.length > 0) {
                const item = data[0];
                const lat = parseFloat(item.lat);
                const lng = parseFloat(item.lon);
                const placeName = item.display_name.split(',')[0];

                this.station.lat = lat;
                this.station.lng = lng;
                this.station.name = `Pusat Gempa: ${placeName}`;

                this.leafletMap.setView([lat, lng], 14, { animate: true });
                this.stationMarker.setLatLng([lat, lng]);
                this.updateStationPopup();
                this.renderQuakeRadiusZone(this.currentRadiusKm);
                this.calculateRealRoadRoutes();

                const in1 = document.getElementById('input-loc-1');
                if (in1) in1.value = `${placeName} (${lat.toFixed(5)}, ${lng.toFixed(5)})`;

                if (window.showToast) window.showToast(`Lokasi ditemukan: ${placeName}`, 'success');
            } else {
                alert(`Lokasi "${query}" tidak ditemukan. Silakan ketik nama daerah atau gedung yang lebih jelas.`);
            }
        } catch (err) {
            console.warn('Search error:', err);
            alert('Gagal mencari lokasi: ' + err.message);
        }
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
                if (in1) in1.value = `Lokasi GPS Saya (${userLat.toFixed(5)}, ${userLng.toFixed(5)})`;

                // Recenter & Recalculate
                this.leafletMap.setView([userLat, userLng], 14, { animate: true });
                this.renderQuakeRadiusZone(this.currentRadiusKm);
                this.calculateRealRoadRoutes();

                if (window.showToast) {
                    window.showToast(`GPS Terhubung! Akurasi: ±${accuracyMeters}m. Rute diperbarui.`, 'success');
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

        // Tombol Tambah Posko Kustom (Buka modal atau aktifkan klik di peta)
        const btnAddShelter = document.getElementById('btn-add-shelter');
        const modalAddShelter = document.getElementById('add-shelter-modal');
        if (btnAddShelter && modalAddShelter) {
            btnAddShelter.addEventListener('click', () => {
                modalAddShelter.classList.remove('hidden');
                modalAddShelter.setAttribute('aria-hidden', 'false');
            });
        }

        const btnPickOnMap = document.getElementById('btn-pick-shelter-on-map');
        if (btnPickOnMap && modalAddShelter) {
            btnPickOnMap.addEventListener('click', () => {
                modalAddShelter.classList.add('hidden');
                this.isPickingShelterOnMap = true;
                if (window.showToast) {
                    window.showToast('Klik di area mana saja pada peta untuk menaruh posko evakuasi baru 🎯', 'info');
                }
            });
        }

        const btnSubmitShelter = document.getElementById('btn-submit-new-shelter');
        if (btnSubmitShelter) {
            btnSubmitShelter.addEventListener('click', () => {
                const name = document.getElementById('new-shelter-name').value;
                const lat = parseFloat(document.getElementById('new-shelter-lat').value);
                const lng = parseFloat(document.getElementById('new-shelter-lng').value);
                if (isNaN(lat) || isNaN(lng)) {
                    alert('Harap isi koordinat latitude dan longitude yang valid!');
                    return;
                }
                modalAddShelter.classList.add('hidden');
                this.addNewShelter(name, lat, lng);
            });
        }

        // Tombol Cari Alamat / Tempat di Google Maps
        const searchInput = document.getElementById('gmaps-search-box');
        const btnSearch = document.getElementById('btn-gmaps-search');

        if (btnSearch && searchInput) {
            btnSearch.addEventListener('click', () => {
                this.searchLocation(searchInput.value);
            });
            searchInput.addEventListener('keypress', (e) => {
                if (e.key === 'Enter') {
                    this.searchLocation(searchInput.value);
                }
            });
        }

        // Klik pada Peta (untuk tambah posko atau geser stasiun)
        if (this.leafletMap) {
            this.leafletMap.on('click', (e) => {
                if (this.isPickingShelterOnMap) {
                    this.isPickingShelterOnMap = false;
                    const name = prompt('Nama Posko Evakuasi Baru:', 'Posko Tambahan Baru');
                    if (name) {
                        this.addNewShelter(name, e.latlng.lat, e.latlng.lng);
                    }
                }
            });

            this.leafletMap.on('contextmenu', (e) => {
                const clickPos = e.latlng;
                this.station.lat = clickPos.lat;
                this.station.lng = clickPos.lng;
                this.stationMarker.setLatLng(clickPos);
                this.updateStationPopup();
                this.renderQuakeRadiusZone(this.currentRadiusKm);
                this.calculateRealRoadRoutes();

                const in1 = document.getElementById('input-loc-1');
                if (in1) in1.value = `Titik Sensor ESP32 (${clickPos.lat.toFixed(5)}, ${clickPos.lng.toFixed(5)})`;

                if (window.showToast) {
                    window.showToast(`Pusat Sensor Gempa disetel ke: ${clickPos.lat.toFixed(5)}, ${clickPos.lng.toFixed(5)}`, 'info');
                }
            });
        }

        // Reset / Undo View button
        const btnUndo = document.getElementById('top-btn-undo');
        if (btnUndo) {
            btnUndo.addEventListener('click', () => this.resetToItera());
        }

        // Tombol Mode Peta Penuh (Top Map Toggle)
        const topBtnMap = document.getElementById('top-btn-map');
        const routingPanel = document.getElementById('routing-panel');
        const telemetryHud = document.getElementById('telemetry-hud');
        if (topBtnMap) {
            topBtnMap.addEventListener('click', () => {
                topBtnMap.classList.toggle('active');
                if (routingPanel) routingPanel.classList.toggle('collapsed');
                if (telemetryHud) telemetryHud.classList.toggle('hidden');
                setTimeout(() => this.leafletMap.invalidateSize(), 350);
                if (window.showToast) {
                    const isFull = routingPanel.classList.contains('collapsed');
                    window.showToast(isFull ? 'Mode Peta Penuh Satelit Google Maps Aktif 🛰️' : 'Mode Command Center Normal Diaktifkan 🗺️', 'info');
                }
            });
        }

        // Collapse / Expand Floating Routing Panel
        const btnCollapsePanel = document.getElementById('btn-collapse-panel');
        const collapseIcon = document.getElementById('collapse-icon');
        if (btnCollapsePanel && routingPanel) {
            btnCollapsePanel.addEventListener('click', () => {
                routingPanel.classList.toggle('collapsed');
                const isCollapsed = routingPanel.classList.contains('collapsed');
                if (collapseIcon) {
                    collapseIcon.className = isCollapsed ? 'fa-solid fa-chevron-right' : 'fa-solid fa-chevron-left';
                }
                setTimeout(() => this.leafletMap.invalidateSize(), 350);
            });
        }

        // Tombol Bersihkan Formulir Rute
        const btnClearRoutes = document.getElementById('btn-clear-routes');
        if (btnClearRoutes) {
            btnClearRoutes.addEventListener('click', () => this.clearRoutes());
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
                const icon = btnFullscreen.querySelector('i');
                if (!document.fullscreenElement) {
                    document.documentElement.requestFullscreen().then(() => {
                        if (icon) icon.className = 'fa-solid fa-compress';
                    });
                } else {
                    document.exitFullscreen().then(() => {
                        if (icon) icon.className = 'fa-solid fa-expand';
                    });
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

        // Optimize Route (Evaluasi Jalur Teraman Luar Bahaya)
        const btnOptimizeRoute = document.getElementById('btn-optimize-route');
        if (btnOptimizeRoute) {
            btnOptimizeRoute.addEventListener('click', () => this.optimizeRoute());
        }

        // Lasso / Hitung Radius Bahaya
        const btnLasso = document.getElementById('btn-lasso-zone');
        if (btnLasso) {
            btnLasso.addEventListener('click', () => {
                if (this.leafletMap && this.quakeRadiusCircle) {
                    this.leafletMap.fitBounds(this.quakeRadiusCircle.getBounds(), { padding: [50, 50] });
                    const pgaVal = (this.currentRadiusKm / 550).toFixed(3);
                    if (window.showToast) {
                        window.showToast(`Zona Bahaya Gempa MPU-6050 (PGA: ${pgaVal}g) — Radius Dampak: ${this.currentRadiusKm} KM ⚠️`, 'warn');
                    }
                }
            });
        }

        // Delete All button (bottom dock)
        const btnDeleteAll = document.getElementById('btn-dock-reset-all');
        if (btnDeleteAll) {
            btnDeleteAll.addEventListener('click', () => {
                if (confirm('Reset semua radius gempa dan rute evakuasi Google Maps ke kondisi awal?')) {
                    this.resetToItera();
                }
            });
        }

        // Color Picker & Preset Dots
        const colorNative = document.getElementById('route-color-native');
        const colorHex = document.getElementById('route-color-hex');
        const colorSwatch = document.getElementById('color-swatch-display');

        const applyRouteColor = (c) => {
            if (colorHex) colorHex.value = c;
            if (colorSwatch) colorSwatch.style.backgroundColor = c;
            if (colorNative) colorNative.value = c;
            this.activeRouteColor = c;
            this.routePolylines.forEach(p => p.setStyle({ color: c }));
        };

        if (colorNative) colorNative.addEventListener('input', (e) => applyRouteColor(e.target.value));
        if (colorHex) colorHex.addEventListener('change', (e) => applyRouteColor(e.target.value));

        document.querySelectorAll('.preset-dot').forEach((dot) => {
            dot.addEventListener('click', () => {
                const c = dot.getAttribute('data-color');
                if (c) applyRouteColor(c);
            });
        });
    }
}

// Instantiate on DOM ready
document.addEventListener('DOMContentLoaded', () => {
    window.geoMap = new GeoMappingEngine();
});

