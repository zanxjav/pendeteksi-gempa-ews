/**
 * GeoShield EWS - Interactive GIS Mapping Engine
 * Supports OpenStreetMap (CartoDB Dark / Satellite / OSM Standard) & Google Maps JavaScript API
 */

class GeoMappingEngine {
    constructor() {
        this.station = {
            name: 'Stasiun Sensor 01 - Jakarta Pusat',
            lat: -6.2088,
            lng: 106.8456
        };

        this.currentEngine = 'leaflet-dark';
        this.leafletMap = null;
        this.leafletStationMarker = null;
        this.leafletQuakeCircle = null;
        this.leafletFloodCircle = null;
        this.leafletBmkgMarkers = [];
        this.tileLayers = {};

        this.googleMap = null;
        this.googleStationMarker = null;
        this.googleQuakeCircle = null;
        this.googleFloodCircle = null;
        this.isGoogleMapsLoaded = false;
        this.googleMapsApiKey = '';

        this.showRadius = true;

        // UI Element bindings
        this.leafletDiv = document.getElementById('leaflet-map');
        this.gmapsDiv = document.getElementById('google-map');
        this.mapSelect = document.getElementById('map-engine-select');
        this.btnRecenter = document.getElementById('btn-recenter-map');
        this.btnToggleRadius = document.getElementById('btn-toggle-radius');
        this.displayName = document.getElementById('display-station-name');
        this.displayCoords = document.getElementById('display-station-coords');
        
        // Location Setter controls
        this.mapSearchInput = document.getElementById('map-search-input');
        this.btnMapSearchSubmit = document.getElementById('btn-map-search-submit');
        this.btnQuickSetLocation = document.getElementById('btn-quick-set-location');
        this.stationFloatingCard = document.getElementById('station-floating-card');

        this.loadSavedSettings();
        this.initLeaflet();
        this.initEvents();
        this.initLocationSetterEvents();
    }

    loadSavedSettings() {
        const saved = localStorage.getItem('geoshield_station_config');
        if (saved) {
            try {
                const parsed = JSON.parse(saved);
                if (parsed.name) this.station.name = parsed.name;
                if (parsed.lat) this.station.lat = parseFloat(parsed.lat);
                if (parsed.lng) this.station.lng = parseFloat(parsed.lng);
                if (parsed.gmapsKey) this.googleMapsApiKey = parsed.gmapsKey;
            } catch (e) {
                console.warn('Failed to parse saved station config', e);
            }
        }
        this.updateStationUI();
    }

    saveSettings() {
        localStorage.setItem('geoshield_station_config', JSON.stringify({
            name: this.station.name,
            lat: this.station.lat,
            lng: this.station.lng,
            gmapsKey: this.googleMapsApiKey
        }));
        this.updateStationUI();
    }

    updateStationUI() {
        if (this.displayName) this.displayName.textContent = this.station.name;
        if (this.displayCoords) {
            const latDir = this.station.lat >= 0 ? '° N' : '° S';
            const lngDir = this.station.lng >= 0 ? '° E' : '° W';
            this.displayCoords.textContent = `${Math.abs(this.station.lat).toFixed(4)}${latDir}, ${Math.abs(this.station.lng).toFixed(4)}${lngDir}`;
        }
    }

    initEvents() {
        if (this.mapSelect) {
            this.mapSelect.addEventListener('change', (e) => this.switchMapLayer(e.target.value));
        }
        if (this.btnRecenter) {
            this.btnRecenter.addEventListener('click', () => this.recenter());
        }
        if (this.btnToggleRadius) {
            this.btnToggleRadius.addEventListener('click', () => {
                this.showRadius = !this.showRadius;
                this.btnToggleRadius.classList.toggle('active', this.showRadius);
                this.updateCirclesVisibility();
            });
        }
    }

    initLocationSetterEvents() {
        // 1. Search Location Submission
        const executeSearch = () => {
            if (!this.mapSearchInput) return;
            const query = this.mapSearchInput.value.trim();
            if (query) {
                this.searchAndSetLocation(query);
            }
        };

        if (this.btnMapSearchSubmit) {
            this.btnMapSearchSubmit.addEventListener('click', executeSearch);
        }
        if (this.mapSearchInput) {
            this.mapSearchInput.addEventListener('keypress', (e) => {
                if (e.key === 'Enter') executeSearch();
            });
        }

        // 2. Preset Buttons Click
        const presetBtns = document.querySelectorAll('.btn-preset');
        presetBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                const name = btn.getAttribute('data-name');
                const lat = parseFloat(btn.getAttribute('data-lat'));
                const lng = parseFloat(btn.getAttribute('data-lng'));
                this.updateStationLocation(lat, lng, name);
                this.recenter();
            });
        });

        // 3. Quick Set Location Button & Floating Card Click
        if (this.btnQuickSetLocation) {
            this.btnQuickSetLocation.addEventListener('click', () => {
                const modal = document.getElementById('modal-config');
                if (modal) modal.classList.remove('hidden');
            });
        }
        if (this.stationFloatingCard) {
            this.stationFloatingCard.addEventListener('click', () => {
                const modal = document.getElementById('modal-config');
                if (modal) modal.classList.remove('hidden');
            });
        }
    }

    // Geocoding: Search city / region and update map pin
    async searchAndSetLocation(query) {
        if (this.btnMapSearchSubmit) {
            this.btnMapSearchSubmit.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i>';
            this.btnMapSearchSubmit.disabled = true;
        }

        try {
            // Using OpenStreetMap Nominatim Geocoder API
            const url = `https://nominatim.openstreetmap.org/search?format=json&q=${encodeURIComponent(query)}&limit=1`;
            const res = await fetch(url, {
                headers: { 'Accept-Language': 'id,en' }
            });
            const data = await res.json();

            if (data && data.length > 0) {
                const item = data[0];
                const lat = parseFloat(item.lat);
                const lng = parseFloat(item.lon);
                const shortName = `Stasiun Sensor - ${item.display_name.split(',')[0]}`;

                this.updateStationLocation(lat, lng, shortName);
                this.recenter();
                if (this.mapSearchInput) this.mapSearchInput.value = item.display_name.split(',').slice(0, 2).join(',');
            } else {
                alert(`Lokasi "${query}" tidak ditemukan. Coba ketik nama kota lain.`);
            }
        } catch (err) {
            console.warn('Geocoding error:', err);
            alert('Gagal mencari lokasi. Pastikan terhubung ke internet.');
        } finally {
            if (this.btnMapSearchSubmit) {
                this.btnMapSearchSubmit.innerHTML = 'Cari & Set';
                this.btnMapSearchSubmit.disabled = false;
            }
        }
    }

    // Reverse Geocoding: Lookup address from Lat, Lng when dragging marker
    async reverseGeocodeAddress(lat, lng) {
        try {
            const url = `https://nominatim.openstreetmap.org/reverse?format=json&lat=${lat}&lon=${lng}`;
            const res = await fetch(url, { headers: { 'Accept-Language': 'id,en' } });
            const data = await res.json();
            if (data && data.address) {
                const city = data.address.city || data.address.town || data.address.county || data.address.state || 'Lokasi Kustom';
                const name = `Stasiun Sensor - ${city}`;
                this.station.name = name;
                this.saveSettings();
            }
        } catch (e) {
            // Silent fallback
        }
    }

    // Initialize Leaflet Open GIS Map
    initLeaflet() {
        if (!this.leafletDiv) return;

        this.leafletMap = L.map('leaflet-map', {
            center: [this.station.lat, this.station.lng],
            zoom: 13,
            zoomControl: false
        });

        // Add Zoom Control top right
        L.control.zoom({ position: 'topright' }).addTo(this.leafletMap);

        // Tile Providers
        this.tileLayers['leaflet-dark'] = L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
            attribution: '&copy; <a href="https://carto.com/">CARTO</a> & OpenStreetMap',
            maxZoom: 19
        });

        this.tileLayers['leaflet-satellite'] = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', {
            attribution: 'Tiles &copy; Esri & GIS Community',
            maxZoom: 18
        });

        this.tileLayers['leaflet-street'] = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
            attribution: '&copy; OpenStreetMap contributors',
            maxZoom: 19
        });

        // Default to OpenStreetMap Standard Layer
        this.tileLayers['leaflet-street'].addTo(this.leafletMap);

        // Create Custom Animated Sensor Beacon Marker
        const beaconIcon = L.divIcon({
            className: 'custom-station-pin',
            html: `
                <div style="position: relative; width: 28px; height: 28px; display: flex; align-items: center; justify-content: center;">
                    <div style="position: absolute; width: 28px; height: 28px; border-radius: 50%; background: rgba(56, 189, 248, 0.4); animation: pulse-ring-anim 1.8s infinite;"></div>
                    <div style="width: 14px; height: 14px; border-radius: 50%; background: #38bdf8; border: 2px solid #ffffff; box-shadow: 0 0 10px #38bdf8;"></div>
                </div>
            `,
            iconSize: [28, 28],
            iconAnchor: [14, 14]
        });

        this.leafletStationMarker = L.marker([this.station.lat, this.station.lng], {
            icon: beaconIcon,
            draggable: true
        }).addTo(this.leafletMap);

        this.leafletStationMarker.bindPopup(`
            <div style="font-family: sans-serif; padding: 4px;">
                <strong style="color: #0284c7;">📍 ${this.station.name}</strong><br>
                <small style="color: #475569;">Stasiun Sensor IoT Utama</small><br>
                <div style="margin-top: 4px; font-size: 11px; color: #64748b;">
                    Lat: ${this.station.lat.toFixed(4)}<br>
                    Lng: ${this.station.lng.toFixed(4)}
                </div>
            </div>
        `);

        // Handle Marker Drag & Drop to relocate station
        this.leafletStationMarker.on('dragend', (e) => {
            const pos = e.target.getLatLng();
            this.updateStationLocation(pos.lat, pos.lng);
            this.reverseGeocodeAddress(pos.lat, pos.lng);
        });

        // Click anywhere to relocate station
        this.leafletMap.on('click', (e) => {
            this.updateStationLocation(e.latlng.lat, e.latlng.lng);
            this.reverseGeocodeAddress(e.latlng.lat, e.latlng.lng);
        });
    }

    switchMapLayer(layerKey) {
        this.currentEngine = layerKey;

        if (layerKey === 'gmaps') {
            this.showGoogleMapsMode();
            return;
        }

        // Leaflet Mode
        if (this.gmapsDiv) this.gmapsDiv.classList.add('hidden');
        if (this.leafletDiv) this.leafletDiv.classList.remove('hidden');

        // Remove old layers
        Object.values(this.tileLayers).forEach(layer => {
            if (this.leafletMap.hasLayer(layer)) {
                this.leafletMap.removeLayer(layer);
            }
        });

        // Add selected layer
        if (this.tileLayers[layerKey]) {
            this.tileLayers[layerKey].addTo(this.leafletMap);
        }

        setTimeout(() => {
            this.leafletMap.invalidateSize();
        }, 150);
    }

    showGoogleMapsMode() {
        if (!this.googleMapsApiKey) {
            const enterKey = prompt('Masukkan Google Maps JavaScript API Key Anda (atau klik Batal untuk tetap memakai OpenStreetMap):', '');
            if (enterKey && enterKey.trim() !== '') {
                this.googleMapsApiKey = enterKey.trim();
                this.saveSettings();
            } else {
                this.mapSelect.value = 'leaflet-dark';
                this.switchMapLayer('leaflet-dark');
                return;
            }
        }

        if (this.leafletDiv) this.leafletDiv.classList.add('hidden');
        if (this.gmapsDiv) this.gmapsDiv.classList.remove('hidden');

        if (!this.isGoogleMapsLoaded) {
            this.loadGoogleMapsScript();
        } else if (this.googleMap) {
            google.maps.event.trigger(this.googleMap, 'resize');
            this.googleMap.setCenter({ lat: this.station.lat, lng: this.station.lng });
        }
    }

    loadGoogleMapsScript() {
        if (window.google && window.google.maps) {
            this.isGoogleMapsLoaded = true;
            this.initGoogleMaps();
            return;
        }

        const script = document.createElement('script');
        script.src = `https://maps.googleapis.com/maps/api/js?key=${this.googleMapsApiKey}&callback=initGMapCallback`;
        script.async = true;
        script.defer = true;

        window.initGMapCallback = () => {
            this.isGoogleMapsLoaded = true;
            this.initGoogleMaps();
        };

        script.onerror = () => {
            alert('Gagal memuat Google Maps API. Periksa API Key atau koneksi internet Anda. Kembali ke OpenStreetMap.');
            this.mapSelect.value = 'leaflet-dark';
            this.switchMapLayer('leaflet-dark');
        };

        document.head.appendChild(script);
    }

    initGoogleMaps() {
        if (!this.gmapsDiv || !window.google) return;

        const darkMapStyles = [
            { elementType: "geometry", stylers: [{ color: "#242f3e" }] },
            { elementType: "labels.text.stroke", stylers: [{ color: "#242f3e" }] },
            { elementType: "labels.text.fill", stylers: [{ color: "#746855" }] },
            { featureType: "water", elementType: "geometry", stylers: [{ color: "#17263c" }] }
        ];

        this.googleMap = new google.maps.Map(this.gmapsDiv, {
            center: { lat: this.station.lat, lng: this.station.lng },
            zoom: 13,
            styles: darkMapStyles,
            disableDefaultUI: false
        });

        this.googleStationMarker = new google.maps.Marker({
            position: { lat: this.station.lat, lng: this.station.lng },
            map: this.googleMap,
            draggable: true,
            title: this.station.name
        });

        this.googleStationMarker.addListener('dragend', (e) => {
            this.updateStationLocation(e.latLng.lat(), e.latLng.lng());
        });

        this.googleMap.addListener('click', (e) => {
            this.updateStationLocation(e.latLng.lat(), e.latLng.lng());
        });
    }

    updateStationLocation(lat, lng, name = null) {
        this.station.lat = lat;
        this.station.lng = lng;
        if (name) this.station.name = name;

        this.saveSettings();

        // Update Leaflet Marker
        if (this.leafletStationMarker) {
            this.leafletStationMarker.setLatLng([lat, lng]);
            this.leafletMap.panTo([lat, lng]);
        }

        // Update Google Maps Marker
        if (this.googleStationMarker) {
            this.googleStationMarker.setPosition({ lat, lng });
            if (this.googleMap) this.googleMap.panTo({ lat, lng });
        }

        // Reposition Danger Circles if active
        if (this.leafletQuakeCircle) this.leafletQuakeCircle.setLatLng([lat, lng]);
        if (this.leafletFloodCircle) this.leafletFloodCircle.setLatLng([lat, lng]);

        if (this.googleQuakeCircle) this.googleQuakeCircle.setCenter({ lat, lng });
        if (this.googleFloodCircle) this.googleFloodCircle.setCenter({ lat, lng });

        // Update Form Inputs if config modal open
        const latInput = document.getElementById('cfg-lat');
        const lngInput = document.getElementById('cfg-lng');
        const nameInput = document.getElementById('cfg-station-name');
        if (latInput) latInput.value = lat.toFixed(6);
        if (lngInput) lngInput.value = lng.toFixed(6);
        if (nameInput && name) nameInput.value = name;
    }

    recenter() {
        if (this.leafletMap && this.currentEngine !== 'gmaps') {
            this.leafletMap.setView([this.station.lat, this.station.lng], 13);
        } else if (this.googleMap && this.currentEngine === 'gmaps') {
            this.googleMap.setCenter({ lat: this.station.lat, lng: this.station.lng });
            this.googleMap.setZoom(13);
        }
    }

    showEarthquakeShakingZone(magnitude) {
        if (!this.showRadius) return;
        // Radius based on magnitude (e.g. 5.0 -> ~15km, 7.0 -> ~80km)
        const radiusMeters = Math.max(1000, Math.pow(10, (magnitude - 3.2) * 0.8) * 1000);

        // Leaflet Circle
        if (this.leafletQuakeCircle) {
            this.leafletMap.removeLayer(this.leafletQuakeCircle);
        }

        this.leafletQuakeCircle = L.circle([this.station.lat, this.station.lng], {
            color: '#ef4444',
            fillColor: '#ef4444',
            fillOpacity: 0.25,
            radius: radiusMeters,
            weight: 2
        }).addTo(this.leafletMap);

        this.leafletQuakeCircle.bindPopup(`<b>Zona Guncangan Gempa</b><br>Estimasi Radius Kerusakan: ${(radiusMeters/1000).toFixed(1)} km`);

        // Google Maps Circle
        if (this.googleMap && window.google) {
            if (this.googleQuakeCircle) this.googleQuakeCircle.setMap(null);
            this.googleQuakeCircle = new google.maps.Circle({
                strokeColor: '#ef4444',
                strokeOpacity: 0.8,
                strokeWeight: 2,
                fillColor: '#ef4444',
                fillOpacity: 0.25,
                map: this.googleMap,
                center: { lat: this.station.lat, lng: this.station.lng },
                radius: radiusMeters
            });
        }
    }

    showFloodZone(waterLevelCm) {
        if (!this.showRadius) return;
        const radiusMeters = Math.min(5000, Math.max(500, waterLevelCm * 15));

        if (this.leafletFloodCircle) {
            this.leafletMap.removeLayer(this.leafletFloodCircle);
        }

        this.leafletFloodCircle = L.circle([this.station.lat, this.station.lng], {
            color: '#0284c7',
            fillColor: '#38bdf8',
            fillOpacity: 0.3,
            radius: radiusMeters,
            weight: 2
        }).addTo(this.leafletMap);

        this.leafletFloodCircle.bindPopup(`<b>Zona Bahaya Luapan Banjir</b><br>Radius Genangan: ${(radiusMeters/1000).toFixed(2)} km`);
    }

    clearDangerZones() {
        if (this.leafletQuakeCircle && this.leafletMap) {
            this.leafletMap.removeLayer(this.leafletQuakeCircle);
            this.leafletQuakeCircle = null;
        }
        if (this.leafletFloodCircle && this.leafletMap) {
            this.leafletMap.removeLayer(this.leafletFloodCircle);
            this.leafletFloodCircle = null;
        }
        if (this.googleQuakeCircle) {
            this.googleQuakeCircle.setMap(null);
            this.googleQuakeCircle = null;
        }
        if (this.googleFloodCircle) {
            this.googleFloodCircle.setMap(null);
            this.googleFloodCircle = null;
        }
    }

    updateCirclesVisibility() {
        if (!this.showRadius) {
            this.clearDangerZones();
        }
    }

    // Add External BMKG/USGS Earthquake Markers
    renderBmkgMarkers(quakes) {
        // Clear previous BMKG markers
        this.leafletBmkgMarkers.forEach(m => this.leafletMap.removeLayer(m));
        this.leafletBmkgMarkers = [];

        quakes.forEach(q => {
            const customIcon = L.divIcon({
                className: 'bmkg-marker',
                html: `<div style="background: #f59e0b; color: #000; font-weight: 800; font-size: 10px; width: 22px; height: 22px; border-radius: 50%; display: flex; align-items: center; justify-content: center; border: 2px solid #fff; box-shadow: 0 0 8px #f59e0b;">${q.mag.toFixed(1)}</div>`,
                iconSize: [22, 22],
                iconAnchor: [11, 11]
            });

            const marker = L.marker([q.lat, q.lng], { icon: customIcon }).addTo(this.leafletMap);
            marker.bindPopup(`
                <div style="font-family: sans-serif; font-size: 12px;">
                    <strong style="color: #dc2626;">🌋 Gempa ${q.mag} M</strong><br>
                    <span>Lokasi: ${q.location}</span><br>
                    <span>Kedalaman: ${q.depth}</span><br>
                    <small style="color: #64748b;">Waktu: ${q.time}</small>
                </div>
            `);
            this.leafletBmkgMarkers.push(marker);
        });
    }
}

// Global GIS Instance
window.geoMap = new GeoMappingEngine();
