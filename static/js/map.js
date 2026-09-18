/**
 * GeoShield EWS - Full Map Tracking & GIS Mapping Engine
 * Matched with Vehicle & Seismic Monitoring Layout
 * Centered at Institut Teknologi Sumatera (ITERA), Lampung
 */

class GeoMappingEngine {
    constructor() {
        this.station = {
            name: 'Institut Teknologi Sumatera (ITERA)',
            lat: -5.4267,
            lng: 105.3179
        };

        this.leafletMap = null;
        this.stationMarker = null;
        this.radarCircle = null;
        this.routePolyline = null;
        this.showRoute = true;

        // UI Element bindings
        this.leafletDiv = document.getElementById('leaflet-map');
        this.btnRecenter = document.getElementById('btn-recenter-map');
        this.btnZoomIn = document.getElementById('btn-zoom-in');
        this.btnZoomOut = document.getElementById('btn-zoom-out');
        this.btnFullscreen = document.getElementById('btn-fullscreen-map');
        this.btnModeRealtime = document.getElementById('btn-mode-realtime');
        this.btnModeRoute = document.getElementById('btn-mode-route');

        this.tooltipSpeed = document.getElementById('map-tooltip-speed');
        this.tooltipLat = document.getElementById('map-tooltip-lat');
        this.tooltipLon = document.getElementById('map-tooltip-lon');
        this.markerInfoCard = document.getElementById('map-marker-infocard');

        this.init();
    }

    init() {
        this.loadSavedSettings();
        this.initLeaflet();
        this.initEvents();
    }

    loadSavedSettings() {
        const saved = localStorage.getItem('geoshield_station_config');
        if (saved) {
            try {
                const parsed = JSON.parse(saved);
                if (parsed.lat) this.station.lat = parseFloat(parsed.lat);
                if (parsed.lng) this.station.lng = parseFloat(parsed.lng);
                if (parsed.name) this.station.name = parsed.name;
            } catch (e) {
                console.warn('Failed to parse station config', e);
            }
        }
        this.updateCoordsUI(this.station.lat, this.station.lng);
    }

    updateCoordsUI(lat, lng) {
        if (this.tooltipLat) this.tooltipLat.textContent = lat.toFixed(4);
        if (this.tooltipLon) this.tooltipLon.textContent = lng.toFixed(4);

        const specLoc = document.getElementById('spec-val-location');
        if (specLoc) specLoc.textContent = `${lat.toFixed(4)}, ${lng.toFixed(4)}`;
    }

    initLeaflet() {
        if (!this.leafletDiv) return;

        // Initialize Map centered on ITERA Lampung (Lat -5.4267, Lon 105.3179)
        this.leafletMap = L.map('leaflet-map', {
            center: [this.station.lat, this.station.lng],
            zoom: 14,
            zoomControl: false
        });

        // Clean OpenStreetMap Tiles
        L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
            attribution: '&copy; <a href="https://www.openstreetmap.org/">OpenStreetMap</a>',
            maxZoom: 19
        }).addTo(this.leafletMap);

        // 1. Draw Route Polyline along Jl. Terusan Ryacudu (Matching Reference Screenshot)
        const routeCoords = [
            [-5.4295, 105.3010],
            [-5.4290, 105.3040],
            [-5.4280, 105.3080],
            [-5.4270, 105.3110],
            [-5.4260, 105.3140],
            [-5.4265, 105.3160],
            [-5.4267, 105.3179] // Current car/station position
        ];

        this.routePolyline = L.polyline(routeCoords, {
            color: '#1A68FF',
            weight: 5,
            opacity: 0.85,
            lineJoin: 'round',
            lineCap: 'round'
        }).addTo(this.leafletMap);

        // 2. Translucent Blue Radar / Buffer Ring around Vehicle/Sensor
        this.radarCircle = L.circle([this.station.lat, this.station.lng], {
            radius: 400, // 400 meters
            color: '#1A68FF',
            weight: 1.5,
            fillColor: '#38BDF8',
            fillOpacity: 0.2
        }).addTo(this.leafletMap);

        // 3. Custom Vehicle / Sensor Beacon Marker Icon
        const vehicleIcon = L.divIcon({
            className: 'leaflet-radar-marker',
            html: `
                <div class="pulse-ring"></div>
                <div class="core-pin">
                    <i class="fa-solid fa-car-side"></i>
                </div>
            `,
            iconSize: [48, 48],
            iconAnchor: [24, 24]
        });

        this.stationMarker = L.marker([this.station.lat, this.station.lng], {
            icon: vehicleIcon,
            draggable: true
        }).addTo(this.leafletMap);

        // Drag marker to update location
        this.stationMarker.on('drag', (e) => {
            const pos = e.target.getLatLng();
            this.radarCircle.setLatLng(pos);
            this.updateCoordsUI(pos.lat, pos.lng);
        });

        this.stationMarker.on('dragend', (e) => {
            const pos = e.target.getLatLng();
            this.station.lat = pos.lat;
            this.station.lng = pos.lng;
            localStorage.setItem('geoshield_station_config', JSON.stringify(this.station));
            this.updateCoordsUI(pos.lat, pos.lng);
        });

        // Click map to relocate
        this.leafletMap.on('click', (e) => {
            this.updateLocation(e.latlng.lat, e.latlng.lng);
        });
    }

    updateLocation(lat, lng) {
        this.station.lat = lat;
        this.station.lng = lng;
        if (this.stationMarker) this.stationMarker.setLatLng([lat, lng]);
        if (this.radarCircle) this.radarCircle.setLatLng([lat, lng]);
        this.updateCoordsUI(lat, lng);
        localStorage.setItem('geoshield_station_config', JSON.stringify(this.station));
    }

    initEvents() {
        // Zoom controls
        if (this.btnZoomIn) {
            this.btnZoomIn.addEventListener('click', () => {
                if (this.leafletMap) this.leafletMap.zoomIn();
            });
        }
        if (this.btnZoomOut) {
            this.btnZoomOut.addEventListener('click', () => {
                if (this.leafletMap) this.leafletMap.zoomOut();
            });
        }

        // Recenter
        if (this.btnRecenter) {
            this.btnRecenter.addEventListener('click', () => {
                if (this.leafletMap) {
                    this.leafletMap.setView([this.station.lat, this.station.lng], 15, { animate: true });
                }
            });
        }

        // Mode Realtime / Route
        if (this.btnModeRealtime) {
            this.btnModeRealtime.addEventListener('click', () => {
                this.btnModeRealtime.classList.add('active');
                if (this.btnModeRoute) this.btnModeRoute.classList.remove('active');
                if (this.leafletMap) {
                    this.leafletMap.setView([this.station.lat, this.station.lng], 15);
                }
            });
        }
        if (this.btnModeRoute) {
            this.btnModeRoute.addEventListener('click', () => {
                this.btnModeRoute.classList.add('active');
                if (this.btnModeRealtime) this.btnModeRealtime.classList.remove('active');
                if (this.routePolyline && this.leafletMap) {
                    this.leafletMap.fitBounds(this.routePolyline.getBounds(), { padding: [50, 50] });
                }
            });
        }

        // Fullscreen Toggle
        if (this.btnFullscreen) {
            this.btnFullscreen.addEventListener('click', () => {
                const card = document.querySelector('.full-map-card');
                if (!document.fullscreenElement) {
                    if (card.requestFullscreen) card.requestFullscreen();
                } else {
                    if (document.exitFullscreen) document.exitFullscreen();
                }
            });
        }

        // Spec location link click
        const specLoc = document.getElementById('spec-val-location');
        if (specLoc) {
            specLoc.addEventListener('click', () => {
                if (this.leafletMap) {
                    this.leafletMap.setView([this.station.lat, this.station.lng], 16, { animate: true });
                }
            });
        }
    }

    setSpeed(speedVal) {
        if (this.tooltipSpeed) {
            this.tooltipSpeed.textContent = `${speedVal} km/h`;
        }
    }
}

// Instantiate on DOM load
document.addEventListener('DOMContentLoaded', () => {
    window.geoMap = new GeoMappingEngine();
});
