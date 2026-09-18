/**
 * GeoShield EWS - Satellite GIS Command Center Mapping Engine
 * Matched 1:1 with User Reference Screenshot (Earthquake Zone / Routing GIS)
 * Features:
 * 1. Esri World Imagery (High-Resolution Satellite) default layer + CartoDB labels
 * 2. Giant Red Earthquake Impact Radius with top apex badge (25 KM)
 * 3. Multi-color Evacuation Route Polylines (Cyan, Blue, Yellow, Green)
 * 4. Evacuation Shelter Markers (Green 1, Red 2)
 * 5. Dynamic binding to ESP32 MPU-6050 PGA / Magnitude
 */

class GeoMappingEngine {
    constructor() {
        this.station = {
            id: 'ST-01-ESP32',
            name: 'Stasiun EWS ITERA (ESP32-C3)',
            lat: -5.4267,
            lng: 105.3179
        };

        // Shelters / Evacuation destinations (outside danger radius or intermediate safe points)
        this.shelters = [
            { id: 1, name: 'Posko 1 (GOR ITERA)', lat: -5.4050, lng: 105.3280, type: 'green', num: 1, dist: '3.2 km', eta: '6 mins', color: '#06B6D4' },
            { id: 2, name: 'Posko 2 (RSUD Airan Raya)', lat: -5.3920, lng: 105.3420, type: 'red', num: 2, dist: '6.5 km', eta: '14 mins', color: '#38BDF8' },
            { id: 3, name: 'Posko 3 (Kantor Desa Way Huwi)', lat: -5.4120, lng: 105.3580, type: 'green', num: 1, dist: '8.1 km', eta: '18 mins', color: '#F59E0B' },
            { id: 4, name: 'Posko 4 (Stadion Jati Agung)', lat: -5.3780, lng: 105.3680, type: 'red', num: 2, dist: '12.4 km', eta: '25 mins', color: '#10B981' }
        ];

        this.leafletMap = null;
        this.stationMarker = null;
        this.quakeRadiusCircle = null;
        this.apexBadgeMarker = null;
        this.routePolylines = [];
        this.shelterMarkers = [];

        this.currentRadiusKm = 25; // Default 25 KM matching reference photo '25MI'
        this.currentLayerMode = 'satellite'; // 'satellite', 'osm', 'dark'
        this.activeRouteColor = '#06B6D4';

        // Tile layer instances
        this.layerSatellite = null;
        this.layerLabels = null;
        this.layerOsm = null;
        this.layerDark = null;

        this.init();
    }

    init() {
        this.initLeaflet();
        this.renderStationEpicenter();
        this.renderQuakeRadiusZone(this.currentRadiusKm);
        this.renderEvacuationRoutes();
        this.renderShelterMarkers();
        this.bindEvents();
    }

    initLeaflet() {
        const container = document.getElementById('leaflet-map');
        if (!container) return;

        // Centered around ITERA Lampung
        this.leafletMap = L.map('leaflet-map', {
            center: [this.station.lat, this.station.lng],
            zoom: 12,
            zoomControl: false,
            attributionControl: false
        });

        // 1. Esri World Imagery (High-Res Satellite Tiles)
        this.layerSatellite = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', {
            maxZoom: 19,
            subdomains: ['server', 'services']
        }).addTo(this.leafletMap);

        // 2. CartoDB Labels Overlay for Roads & Cities
        this.layerLabels = L.tileLayer('https://{s}.basemaps.cartocdn.com/rastertiles/voyager_only_labels/{z}/{x}/{y}{r}.png', {
            maxZoom: 19,
            subdomains: 'abcd'
        }).addTo(this.leafletMap);

        // 3. Fallback OpenStreetMap Standard
        this.layerOsm = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
            maxZoom: 19
        });

        // 4. CartoDB Dark Matter
        this.layerDark = L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
            maxZoom: 19
        });
    }

    // Render Center Epicenter & Sensor Node Marker
    renderStationEpicenter() {
        if (!this.leafletMap) return;

        const pulseIcon = L.divIcon({
            className: 'epicenter-radar-marker',
            html: `
                <div class="epicenter-pulse"></div>
                <div class="epicenter-core" title="Stasiun EWS ITERA (ESP32-C3)"></div>
            `,
            iconSize: [32, 32],
            iconAnchor: [16, 16]
        });

        this.stationMarker = L.marker([this.station.lat, this.station.lng], { icon: pulseIcon }).addTo(this.leafletMap);
        this.stationMarker.bindPopup(`
            <div style="color: #0F172A; font-family: sans-serif; font-size: 12px; line-height: 1.4;">
                <strong style="color: #1D4ED8; font-size: 13px;">📍 ${this.station.name}</strong><br>
                <span>ID Node: <code>${this.station.id}</code></span><br>
                <span>Koordinat: ${this.station.lat.toFixed(4)}, ${this.station.lng.toFixed(4)}</span><br>
                <div style="margin-top: 6px; padding: 4px 8px; background: #FEE2E2; color: #DC2626; border-radius: 4px; font-weight: bold;">
                    ⚠️ Titik Pusat Guncangan Seismik ESP32
                </div>
            </div>
        `);
    }

    // Render Red Earthquake Impact Radius Circle with Top Apex 25 KM Badge (Matches Photo 1:1)
    renderQuakeRadiusZone(radiusKm) {
        this.currentRadiusKm = radiusKm;
        const radiusMeters = radiusKm * 1000;

        // Remove previous circle & badge
        if (this.quakeRadiusCircle && this.leafletMap) {
            this.leafletMap.removeLayer(this.quakeRadiusCircle);
        }
        if (this.apexBadgeMarker && this.leafletMap) {
            this.leafletMap.removeLayer(this.apexBadgeMarker);
        }

        // 1. Red Translucent Circular Danger Zone
        this.quakeRadiusCircle = L.circle([this.station.lat, this.station.lng], {
            radius: radiusMeters,
            color: '#DC2626',
            weight: 2,
            opacity: 0.9,
            fillColor: '#EF4444',
            fillOpacity: 0.32
        }).addTo(this.leafletMap);

        this.quakeRadiusCircle.bindPopup(`
            <div style="color: #0F172A; font-family: sans-serif; font-size: 12px;">
                <strong style="color: #DC2626; font-size: 13px;">🔴 ZONA BAHAYA GUNCANGAN GEMPA</strong><br>
                <span>Estimasi Radius Kerusakan: <strong>${radiusKm} KM</strong></span><br>
                <span>Intensitas Getaran ESP32: PGA ≥ 0.040g (MMI VI+)</span><br>
                <small style="color: #64748B;">Jalur evakuasi diarahkan keluar dari lingkaran ini.</small>
            </div>
        `);

        // 2. Apex Badge (Red rounded tag at the very top apex of the circle matching '25MI' in reference photo)
        // 1 deg latitude ≈ 111.32 km
        const latOffset = radiusKm / 111.32;
        const apexLat = this.station.lat + latOffset;
        const apexLng = this.station.lng;

        const apexBadgeIcon = L.divIcon({
            className: 'quake-apex-badge-wrap',
            html: `<div class="quake-apex-badge" id="map-apex-radius-tag">${radiusKm} KM</div>`,
            iconSize: [80, 26],
            iconAnchor: [40, 13]
        });

        this.apexBadgeMarker = L.marker([apexLat, apexLng], {
            icon: apexBadgeIcon,
            interactive: true
        }).addTo(this.leafletMap);

        this.apexBadgeMarker.bindPopup(`<b>Radius Gempa: ${radiusKm} KM</b><br>Dihitung dari data akselerasi MPU-6050 ESP32.`);

        // Update Pill in Bottom Dock
        const pillText = document.querySelector('#pill-radius-quake span');
        if (pillText) {
            pillText.innerHTML = `Radius Gempa ESP32 (PGA): <strong>${radiusKm} KM</strong>`;
        }
    }

    // Render Multi-Color Evacuation Routes (Matches Cyan, Yellow, Green lines in Screenshot)
    renderEvacuationRoutes() {
        if (!this.leafletMap) return;

        // Clear existing routes
        this.routePolylines.forEach(p => this.leafletMap.removeLayer(p));
        this.routePolylines = [];

        // Route 1 (Cyan line heading northeast)
        const coordsRoute1 = [
            [this.station.lat, this.station.lng],
            [-5.4200, 105.3210],
            [-5.4120, 105.3230],
            [-5.4050, 105.3280]
        ];
        const line1 = L.polyline(coordsRoute1, {
            color: '#06B6D4',
            weight: 4.5,
            opacity: 0.9,
            lineCap: 'round',
            lineJoin: 'round'
        }).addTo(this.leafletMap);
        line1.bindPopup('<b>ROUTE #1</b><br>Jalur Evakuasi GOR ITERA (3.2 km — 6 mins)');
        this.routePolylines.push(line1);

        // Route 2 (Light Blue heading further east)
        const coordsRoute2 = [
            [-5.4050, 105.3280],
            [-5.4020, 105.3350],
            [-5.3920, 105.3420]
        ];
        const line2 = L.polyline(coordsRoute2, {
            color: '#38BDF8',
            weight: 4,
            opacity: 0.85
        }).addTo(this.leafletMap);
        line2.bindPopup('<b>ROUTE #2</b><br>Jalur Evakuasi RSUD Airan (6.5 km — 14 mins)');
        this.routePolylines.push(line2);

        // Route 3 (Yellow/Orange heading south-east)
        const coordsRoute3 = [
            [-5.4200, 105.3210],
            [-5.4210, 105.3360],
            [-5.4120, 105.3580]
        ];
        const line3 = L.polyline(coordsRoute3, {
            color: '#F59E0B',
            weight: 4,
            opacity: 0.85
        }).addTo(this.leafletMap);
        line3.bindPopup('<b>ROUTE #3</b><br>Jalur Evakuasi Way Huwi (8.1 km — 18 mins)');
        this.routePolylines.push(line3);

        // Route 4 (Green heading north towards Jati Agung)
        const coordsRoute4 = [
            [-5.3920, 105.3420],
            [-5.3850, 105.3520],
            [-5.3780, 105.3680]
        ];
        const line4 = L.polyline(coordsRoute4, {
            color: '#10B981',
            weight: 4,
            opacity: 0.85
        }).addTo(this.leafletMap);
        line4.bindPopup('<b>ROUTE #4</b><br>Jalur Evakuasi Cadangan Jati Agung (12.4 km — 25 mins)');
        this.routePolylines.push(line4);
    }

    // Render Shelter Markers with Green Badge 1 & Red Badge 2 (Matches photo markers 1:1)
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
            marker.bindPopup(`
                <div style="color: #0F172A; font-family: sans-serif; font-size: 12px; line-height: 1.4;">
                    <strong style="color: ${s.type === 'green' ? '#10B981' : '#EF4444'}; font-size: 13px;">${s.name}</strong><br>
                    <span>Status: <strong>${s.type === 'green' ? '🟢 Zona Aman Luar Radius' : '🔴 Posko Siaga Transit'}</strong></span><br>
                    <span>Jarak: ${s.dist} | Estimasi: ${s.eta}</span><br>
                    <span>Fasilitas: Tenda Medis, Pasokan Air Bersih (TDS Teruji), Logistik</span>
                </div>
            `);
            this.shelterMarkers.push(marker);
        });
    }

    // Switch between Satellite, Dark, and OSM
    toggleLayerMode() {
        if (this.currentLayerMode === 'satellite') {
            this.leafletMap.removeLayer(this.layerSatellite);
            this.leafletMap.removeLayer(this.layerLabels);
            this.layerDark.addTo(this.leafletMap);
            this.currentLayerMode = 'dark';
            if (window.showToast) window.showToast('Mode Peta: Dark CartoDB GIS', 'info');
        } else if (this.currentLayerMode === 'dark') {
            this.leafletMap.removeLayer(this.layerDark);
            this.layerOsm.addTo(this.leafletMap);
            this.currentLayerMode = 'osm';
            if (window.showToast) window.showToast('Mode Peta: OpenStreetMap Standard', 'info');
        } else {
            this.leafletMap.removeLayer(this.layerOsm);
            this.layerSatellite.addTo(this.leafletMap);
            this.layerLabels.addTo(this.leafletMap);
            this.currentLayerMode = 'satellite';
            if (window.showToast) window.showToast('Mode Peta: Esri World Satellite Imagery', 'info');
        }
    }

    // Dynamic update when ESP32 telemetry packet arrives
    updateFromTelemetry(telemetry) {
        if (!telemetry) return;

        // 1. Update station coordinates if ESP32 transmits updated GPS
        if (telemetry.location && telemetry.location.lat && telemetry.location.lng) {
            const newLat = parseFloat(telemetry.location.lat);
            const newLng = parseFloat(telemetry.location.lng);
            if (Math.abs(newLat - this.station.lat) > 0.0001 || Math.abs(newLng - this.station.lng) > 0.0001) {
                this.station.lat = newLat;
                this.station.lng = newLng;
                if (this.stationMarker) {
                    this.stationMarker.setLatLng([newLat, newLng]);
                }
            }
        }

        // 2. Recalculate Quake Shaking Radius based on ESP32 MPU-6050 PGA / Gal
        let pga = 0.045;
        if (telemetry.seismic && telemetry.seismic.pga) {
            pga = parseFloat(telemetry.seismic.pga);
        } else if (telemetry.pga_g) {
            pga = parseFloat(telemetry.pga_g);
        }

        // Realistic attenuation radius (km) based on Peak Ground Acceleration
        // PGA 0.005g -> ~5km, PGA 0.045g -> ~25km, PGA 0.08g+ -> ~45km+
        const dynamicRadiusKm = Math.max(5, Math.min(80, Math.round(pga * 550)));
        if (dynamicRadiusKm !== this.currentRadiusKm) {
            this.renderQuakeRadiusZone(dynamicRadiusKm);
        }

        // Update Badge in Left Panel
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

        // Recenter / Undo button
        const btnUndo = document.getElementById('top-btn-undo');
        if (btnUndo) {
            btnUndo.addEventListener('click', () => {
                this.leafletMap.setView([this.station.lat, this.station.lng], 12, { animate: true });
            });
        }

        // Satellite layer toggle button
        const btnToggleSat = document.getElementById('btn-toggle-satellite');
        if (btnToggleSat) {
            btnToggleSat.addEventListener('click', () => this.toggleLayerMode());
        }

        // Fullscreen toggle
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

        // Routing panel collapse button
        const btnCollapse = document.getElementById('btn-collapse-panel');
        const routingPanel = document.getElementById('routing-panel');
        if (btnCollapse && routingPanel) {
            btnCollapse.addEventListener('click', () => {
                routingPanel.classList.toggle('collapsed');
                const icon = document.getElementById('collapse-icon');
                if (routingPanel.classList.contains('collapsed')) {
                    if (icon) icon.className = 'fa-solid fa-chevron-right';
                } else {
                    if (icon) icon.className = 'fa-solid fa-chevron-left';
                }
            });
        }

        // Color Picker for Routes
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

        // Color Preset Dots
        document.querySelectorAll('.preset-dot').forEach(dot => {
            dot.addEventListener('click', () => {
                const c = dot.getAttribute('data-color');
                if (colorHex) colorHex.value = c;
                if (colorNative) colorNative.value = c;
                if (colorSwatch) colorSwatch.style.backgroundColor = c;
                this.activeRouteColor = c;
                if (this.routePolylines[0]) this.routePolylines[0].setStyle({ color: c });
            });
        });

        // Get Directions button
        const btnGetDirections = document.getElementById('btn-get-directions');
        if (btnGetDirections) {
            btnGetDirections.addEventListener('click', () => {
                if (this.leafletMap && this.routePolylines.length > 0) {
                    const group = L.featureGroup(this.routePolylines);
                    this.leafletMap.fitBounds(group.getBounds(), { padding: [60, 60] });
                    if (window.showToast) window.showToast('Jalur Evakuasi Gempa berhasil dihitung & dioptimalkan!', 'success');
                }
            });
        }

        // Lasso / Hitung Radius button
        const btnLasso = document.getElementById('btn-lasso-zone');
        if (btnLasso) {
            btnLasso.addEventListener('click', () => {
                if (this.leafletMap && this.quakeRadiusCircle) {
                    this.leafletMap.fitBounds(this.quakeRadiusCircle.getBounds(), { padding: [50, 50] });
                    if (window.showToast) window.showToast(`Zona Bahaya Gempa terfokus: Radius ${this.currentRadiusKm} KM`, 'info');
                }
            });
        }

        // Delete All button (bottom dock)
        const btnDeleteAll = document.getElementById('btn-dock-reset-all');
        if (btnDeleteAll) {
            btnDeleteAll.addEventListener('click', () => {
                if (confirm('Reset semua radius gempa dan jalur evakuasi ke posisi default?')) {
                    this.renderQuakeRadiusZone(25);
                    this.renderEvacuationRoutes();
                    this.leafletMap.setView([this.station.lat, this.station.lng], 12);
                    if (window.showToast) window.showToast('Semua filter & zona berhasil di-reset!', 'success');
                }
            });
        }

        // Clear Form button in panel
        const btnClear = document.getElementById('btn-clear-routes');
        if (btnClear) {
            btnClear.addEventListener('click', () => {
                const in2 = document.getElementById('input-loc-2');
                if (in2) in2.value = '';
                if (window.showToast) window.showToast('Form lokasi telah dibersihkan.', 'info');
            });
        }
    }
}

// Instantiate on DOM ready
document.addEventListener('DOMContentLoaded', () => {
    window.geoMap = new GeoMappingEngine();
});
