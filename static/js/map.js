/**
 * GeoShield EWS - GIS Monitoring Map Engine (Bandar Lampung & Indonesia)
 * Integrates Google Maps Satellite & Roadmap Tiles with OpenStreetMap Fallback
 * Features Bandar Lampung Station Pins, Rainfall Zones, Seismic Zones & Elevation
 */

class GISMonitoringEngine {
    constructor() {
        // Pusat Peta: Seluruh Wilayah Bandar Lampung (-5.4292, 105.2611)
        this.centerCoords = [-5.4292, 105.2611];
        this.currentZoom = 12; // Menampilkan 1 Kota Bandar Lampung penuh

        this.map = null;
        this.tileLayers = {};
        this.activeLayer = 'satellite';

        // Stasiun Sensor Pos Alat di Bandar Lampung & Sekitarnya
        this.stations = [
            {
                id: 'POS-01',
                name: 'Pos Pantau ITERA (Pusat EWS)',
                subdistrict: 'Jati Agung / Way Huwi',
                lat: -5.3582,
                lng: 105.3148,
                elevation: 124, // mdpl
                pga: 0.000,
                mmi: 'I (Aman)',
                quakeStatus: 'AMAN',
                rainRate: 0.0,
                rainStatus: 'Cerah',
                rainRisk: 'AMAN',
                waterLevel: 42.0,
                tds: 148,
                ph: 7.2,
                active: true
            },
            {
                id: 'POS-02',
                name: 'Pos Pantau Rajabasa',
                subdistrict: 'Kec. Rajabasa',
                lat: -5.3725,
                lng: 105.2340,
                elevation: 95,
                pga: 0.000,
                mmi: 'I (Aman)',
                quakeStatus: 'AMAN',
                rainRate: 4.5,
                rainStatus: 'Hujan Ringan',
                rainRisk: 'WASPADA',
                waterLevel: 55.0,
                tds: 180,
                ph: 7.0,
                active: true
            },
            {
                id: 'POS-03',
                name: 'Pos Tanjung Karang Pusat',
                subdistrict: 'Kec. Tanjung Karang Pusat',
                lat: -5.4180,
                lng: 105.2580,
                elevation: 110,
                pga: 0.000,
                mmi: 'I (Aman)',
                quakeStatus: 'AMAN',
                rainRate: 0.0,
                rainStatus: 'Cerah',
                rainRisk: 'AMAN',
                waterLevel: 38.0,
                tds: 165,
                ph: 7.1,
                active: true
            },
            {
                id: 'POS-04',
                name: 'Pos Pantau Teluk Betung',
                subdistrict: 'Kec. Teluk Betung Selatan (Pesisir)',
                lat: -5.4540,
                lng: 105.2630,
                elevation: 18,
                pga: 0.000,
                mmi: 'I (Aman)',
                quakeStatus: 'AMAN',
                rainRate: 18.0,
                rainStatus: 'Hujan Sedang',
                rainRisk: 'SIAGA',
                waterLevel: 88.0,
                tds: 290,
                ph: 6.8,
                active: true
            },
            {
                id: 'POS-05',
                name: 'Pos Pantau Sukarame',
                subdistrict: 'Kec. Sukarame',
                lat: -5.3890,
                lng: 105.2950,
                elevation: 85,
                pga: 0.000,
                mmi: 'I (Aman)',
                quakeStatus: 'AMAN',
                rainRate: 0.0,
                rainStatus: 'Cerah',
                rainRisk: 'AMAN',
                waterLevel: 30.0,
                tds: 140,
                ph: 7.3,
                active: true
            },
            {
                id: 'POS-06',
                name: 'Pos Pantau Pelabuhan Panjang',
                subdistrict: 'Kec. Panjang',
                lat: -5.4720,
                lng: 105.3190,
                elevation: 12,
                pga: 0.000,
                mmi: 'I (Aman)',
                quakeStatus: 'AMAN',
                rainRate: 12.5,
                rainStatus: 'Hujan Sedang',
                rainRisk: 'SIAGA',
                waterLevel: 92.0,
                tds: 320,
                ph: 6.9,
                active: true
            },
            {
                id: 'POS-07',
                name: 'Pos Pantau Kemiling',
                subdistrict: 'Kec. Kemiling (Perbukitan)',
                lat: -5.4050,
                lng: 105.2080,
                elevation: 215,
                pga: 0.000,
                mmi: 'I (Aman)',
                quakeStatus: 'AMAN',
                rainRate: 0.0,
                rainStatus: 'Cerah',
                rainRisk: 'AMAN',
                waterLevel: 25.0,
                tds: 110,
                ph: 7.4,
                active: true
            }
        ];

        this.markers = [];
        this.rainLayerGroup = L.layerGroup();
        this.quakeLayerGroup = L.layerGroup();
        this.stationLayerGroup = L.layerGroup();

        this.init();
    }

    init() {
        this.initLeafletMap();
        this.renderStationList();
        this.renderMapMarkers();
        this.bindEvents();
    }

    initLeafletMap() {
        const mapContainer = document.getElementById('gis-map-viewport');
        if (!mapContainer) return;

        // Inisialisasi Peta Leaflet
        this.map = L.map('gis-map-viewport', {
            center: this.centerCoords,
            zoom: this.currentZoom,
            zoomControl: false
        });

        // 1. Google Maps Satelit Hybrid Tiles (Sangat Akurat untuk Bandar Lampung)
        this.tileLayers['google-satellite'] = L.tileLayer('https://mt1.google.com/vt/lyrs=y&x={x}&y={y}&z={z}', {
            attribution: '&copy; Google Maps Satelit & GIS Indonesia',
            maxZoom: 20
        });

        // 2. Google Maps Peta Jalan (Roadmap)
        this.tileLayers['google-roadmap'] = L.tileLayer('https://mt1.google.com/vt/lyrs=m&x={x}&y={y}&z={z}', {
            attribution: '&copy; Google Maps Roadmap',
            maxZoom: 20
        });

        // 3. Esri World Imagery (Fallback Satelit GIS Beresolusi Tinggi)
        this.tileLayers['esri-satellite'] = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{x}/{y}', {
            attribution: '&copy; Esri World Imagery',
            maxZoom: 18
        });

        // 4. OpenStreetMap Standar
        this.tileLayers['osm'] = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
            attribution: '&copy; OpenStreetMap contributors',
            maxZoom: 19
        });

        // Default: Google Maps Satelit Hybrid
        this.tileLayers['google-satellite'].addTo(this.map);

        // Layer groups
        this.rainLayerGroup.addTo(this.map);
        this.quakeLayerGroup.addTo(this.map);
        this.stationLayerGroup.addTo(this.map);
    }

    renderStationList() {
        const listContainer = document.getElementById('station-list-container');
        if (!listContainer) return;

        listContainer.innerHTML = '';
        this.stations.forEach((st, idx) => {
            const item = document.createElement('div');
            item.className = `pos-item ${idx === 0 ? 'active' : ''}`;
            item.id = `station-item-${st.id}`;

            const badgeColor = st.quakeStatus === 'AMAN' ? 'bg-success' : 'bg-danger';
            const rainBadge = st.rainRate > 10 ? 'badge-warning' : 'badge-normal';

            item.innerHTML = `
                <div class="pos-item-header">
                    <span class="pos-status-dot ${st.quakeStatus === 'AMAN' ? 'green' : 'red'}"></span>
                    <div class="pos-texts">
                        <strong class="pos-name">${st.name}</strong>
                        <div class="pos-meta">
                            <span><i class="fa-solid fa-mountain"></i> ${st.elevation} mdpl</span> • 
                            <span>${st.subdistrict}</span>
                        </div>
                    </div>
                    <i class="fa-solid fa-chevron-right pos-arrow"></i>
                </div>
                <div class="pos-quick-stats">
                    <span class="stat-tag"><i class="fa-solid fa-wave-square"></i> PGA: ${st.pga.toFixed(3)}g</span>
                    <span class="stat-tag ${rainBadge}"><i class="fa-solid fa-cloud-rain"></i> ${st.rainRate} mm/h</span>
                    <span class="stat-tag"><i class="fa-solid fa-droplet"></i> Air: ${st.waterLevel}cm</span>
                </div>
            `;

            item.addEventListener('click', () => {
                this.selectStation(st);
            });

            listContainer.appendChild(item);
        });
    }

    renderMapMarkers() {
        this.stationLayerGroup.clearLayers();
        this.rainLayerGroup.clearLayers();
        this.quakeLayerGroup.clearLayers();

        this.stations.forEach(st => {
            // Icon Pin Khusus Pos Pantau
            const isAlert = st.quakeStatus !== 'AMAN' || st.rainRate >= 20;
            const pinColor = isAlert ? '#EF4444' : '#10B981';

            const customIcon = L.divIcon({
                className: 'gis-station-marker',
                html: `
                    <div class="gis-marker-container">
                        ${isAlert ? '<div class="gis-marker-pulse danger"></div>' : '<div class="gis-marker-pulse"></div>'}
                        <div class="gis-marker-pin" style="background-color: ${pinColor};">
                            <i class="fa-solid fa-tower-broadcast"></i>
                        </div>
                        <div class="gis-marker-label">${st.name} (${st.elevation} mdpl)</div>
                    </div>
                `,
                iconSize: [36, 36],
                iconAnchor: [18, 18]
            });

            const marker = L.marker([st.lat, st.lng], { icon: customIcon }).addTo(this.stationLayerGroup);

            // Popup detail interaktif
            const popupContent = `
                <div class="gis-popup-content">
                    <div class="popup-title">
                        <i class="fa-solid fa-satellite-dish"></i> ${st.name}
                    </div>
                    <div class="popup-subtitle">${st.subdistrict}, Kota Bandar Lampung</div>
                    <hr class="popup-divider">
                    <div class="popup-grid">
                        <div class="p-item">
                            <span class="p-label">Elevasi (Ketinggian)</span>
                            <strong class="p-val text-primary">${st.elevation} mdpl</strong>
                        </div>
                        <div class="p-item">
                            <span class="p-label">Status Gempa</span>
                            <strong class="p-val ${st.quakeStatus === 'AMAN' ? 'text-success' : 'text-danger'}">${st.quakeStatus} (${st.mmi})</strong>
                        </div>
                        <div class="p-item">
                            <span class="p-label">Curah Hujan</span>
                            <strong class="p-val">${st.rainRate} mm/jam (${st.rainStatus})</strong>
                        </div>
                        <div class="p-item">
                            <span class="p-label">Level Air Sungai</span>
                            <strong class="p-val">${st.waterLevel} cm</strong>
                        </div>
                        <div class="p-item">
                            <span class="p-label">Kualitas Air TDS & pH</span>
                            <strong class="p-val">${st.tds} ppm | pH ${st.ph}</strong>
                        </div>
                        <div class="p-item">
                            <span class="p-label">Koordinat</span>
                            <span class="p-val">${st.lat.toFixed(4)}, ${st.lng.toFixed(4)}</span>
                        </div>
                    </div>
                </div>
            `;
            marker.bindPopup(popupContent);

            // Jika ada hujan di wilayah tersebut, gambarkan lingkaran zona sebaran hujan
            if (st.rainRate > 0) {
                const rainCircle = L.circle([st.lat, st.lng], {
                    radius: 2000, // 2 km radius
                    color: '#38BDF8',
                    weight: 1.5,
                    fillColor: '#0284C7',
                    fillOpacity: 0.25
                }).addTo(this.rainLayerGroup);
            }

            this.markers.push({ stationId: st.id, marker });
        });
    }

    selectStation(station) {
        // Highlight di daftar pos alat
        document.querySelectorAll('.pos-item').forEach(el => el.classList.remove('active'));
        const activeItem = document.getElementById(`station-item-${station.id}`);
        if (activeItem) activeItem.classList.add('active');

        // Pusatkan peta ke stasiun yang dipilih
        if (this.map) {
            this.map.flyTo([station.lat, station.lng], 15, { duration: 1.2 });
        }

        // Buka popup marker
        const match = this.markers.find(m => m.stationId === station.id);
        if (match && match.marker) {
            match.marker.openPopup();
        }

        // Perbarui panel metrik detail di UI
        this.updateActiveStationUI(station);
    }

    updateActiveStationUI(st) {
        const titleEl = document.getElementById('active-station-title');
        const elevEl = document.getElementById('active-station-elevation');
        const pgaEl = document.getElementById('active-station-pga');
        const galEl = document.getElementById('active-station-gal');
        const mmiEl = document.getElementById('active-station-mmi');
        const rainEl = document.getElementById('active-station-rain');
        const waterEl = document.getElementById('active-station-water');
        const tdsEl = document.getElementById('active-station-tds');
        const phEl = document.getElementById('active-station-ph');

        if (titleEl) titleEl.textContent = st.name;
        if (elevEl) elevEl.textContent = `${st.elevation} mdpl`;
        if (pgaEl) pgaEl.textContent = `${st.pga.toFixed(4)} g`;
        if (galEl) galEl.textContent = `${(st.pga * 980.665).toFixed(1)} gal`;
        if (mmiEl) mmiEl.textContent = st.mmi;
        if (rainEl) rainEl.textContent = `${st.rainRate} mm/jam (${st.rainStatus})`;
        if (waterEl) waterEl.textContent = `${st.waterLevel} cm`;
        if (tdsEl) tdsEl.textContent = `${st.tds} ppm`;
        if (phEl) phEl.textContent = `${st.ph}`;
    }

    bindEvents() {
        // Layer Switcher: Satelit vs Peta
        const radioSat = document.getElementById('layer-satellite');
        const radioRoad = document.getElementById('layer-roadmap');

        if (radioSat) {
            radioSat.addEventListener('change', () => {
                if (radioSat.checked) {
                    this.switchBaseLayer('google-satellite');
                }
            });
        }
        if (radioRoad) {
            radioRoad.addEventListener('change', () => {
                if (radioRoad.checked) {
                    this.switchBaseLayer('google-roadmap');
                }
            });
        }

        // Layer Overlays Checkbox
        const chkRain = document.getElementById('chk-layer-rain');
        const chkQuake = document.getElementById('chk-layer-quake');

        if (chkRain) {
            chkRain.addEventListener('change', () => {
                if (chkRain.checked) {
                    this.map.addLayer(this.rainLayerGroup);
                } else {
                    this.map.removeLayer(this.rainLayerGroup);
                }
            });
        }
        if (chkQuake) {
            chkQuake.addEventListener('change', () => {
                if (chkQuake.checked) {
                    this.map.addLayer(this.quakeLayerGroup);
                } else {
                    this.map.removeLayer(this.quakeLayerGroup);
                }
            });
        }

        // Reset Peta ke Seluruh Bandar Lampung
        const btnResetBdl = document.getElementById('btn-reset-bandarlampung');
        if (btnResetBdl) {
            btnResetBdl.addEventListener('click', () => {
                this.map.flyTo(this.centerCoords, this.currentZoom, { duration: 1 });
            });
        }

        // Filter Pos Alat Search Input
        const filterInput = document.getElementById('filter-pos-input');
        if (filterInput) {
            filterInput.addEventListener('input', (e) => {
                const q = e.target.value.toLowerCase();
                document.querySelectorAll('.pos-item').forEach(el => {
                    const txt = el.textContent.toLowerCase();
                    el.style.display = txt.includes(q) ? 'block' : 'none';
                });
            });
        }
    }

    switchBaseLayer(layerKey) {
        Object.values(this.tileLayers).forEach(layer => {
            if (this.map.hasLayer(layer)) {
                this.map.removeLayer(layer);
            }
        });
        if (this.tileLayers[layerKey]) {
            this.tileLayers[layerKey].addTo(this.map);
            this.activeLayer = layerKey;
        }
    }
}

// Instantiate on DOM load
document.addEventListener('DOMContentLoaded', () => {
    window.gisEngine = new GISMonitoringEngine();
});
