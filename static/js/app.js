/**
 * GeoShield EWS - Main Application Controller
 * Coordinates GIS Map, Sensors, Early Warning System, and UI Modals
 */

document.addEventListener('DOMContentLoaded', () => {
    // 1. Live Clock System
    const liveTimeDisplay = document.getElementById('live-time-display');
    const updateClock = () => {
        if (!liveTimeDisplay) return;
        const now = new Date();
        const options = { timeZone: 'Asia/Jakarta', hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false };
        liveTimeDisplay.textContent = `${now.toLocaleTimeString('id-ID', options)} WIB`;
    };
    setInterval(updateClock, 1000);
    updateClock();

    // 2. Modals Control (Config & Hardware)
    const modalConfig = document.getElementById('modal-config');
    const modalHardware = document.getElementById('modal-hardware');
    
    const btnOpenConfig = document.getElementById('btn-open-config');
    const btnCloseConfig = document.getElementById('btn-close-config');
    const btnCancelConfig = document.getElementById('btn-cancel-config');
    const formConfig = document.getElementById('form-station-config');

    const btnOpenHardware = document.getElementById('btn-open-hardware');
    const btnCloseHardware = document.getElementById('btn-close-hardware');

    const btnGetCurrentLoc = document.getElementById('btn-get-current-loc');
    const btnSyncEspLocation = document.getElementById('btn-sync-esp-location');
    const cfgAutoSyncEsp = document.getElementById('cfg-auto-sync-esp');

    // Load auto-sync setting
    const savedAutoSync = localStorage.getItem('geoshield_auto_sync_esp');
    if (cfgAutoSyncEsp && savedAutoSync !== null) {
        cfgAutoSyncEsp.checked = savedAutoSync === 'true';
    }

    // Open/Close Config Modal
    if (btnOpenConfig && modalConfig) {
        btnOpenConfig.addEventListener('click', () => {
            if (window.geoMap) {
                document.getElementById('cfg-station-name').value = window.geoMap.station.name;
                document.getElementById('cfg-lat').value = window.geoMap.station.lat;
                document.getElementById('cfg-lng').value = window.geoMap.station.lng;
                document.getElementById('cfg-gmaps-key').value = window.geoMap.googleMapsApiKey || '';
            }
            if (window.iotService) {
                document.getElementById('cfg-iot-endpoint').value = window.iotService.customEndpoint || '';
            }
            modalConfig.classList.remove('hidden');
        });
    }

    const closeConfig = () => modalConfig && modalConfig.classList.add('hidden');
    if (btnCloseConfig) btnCloseConfig.addEventListener('click', closeConfig);
    if (btnCancelConfig) btnCancelConfig.addEventListener('click', closeConfig);

    // Save Station Config Form (Supporting both comma ',' and dot '.')
    if (formConfig) {
        formConfig.addEventListener('submit', (e) => {
            e.preventDefault();
            const name = document.getElementById('cfg-station-name').value.trim();
            
            // Normalize Indonesian comma decimal into standard dot decimal
            const rawLat = document.getElementById('cfg-lat').value.trim().replace(',', '.');
            const rawLng = document.getElementById('cfg-lng').value.trim().replace(',', '.');
            const lat = parseFloat(rawLat);
            const lng = parseFloat(rawLng);

            if (isNaN(lat) || isNaN(lng)) {
                alert('Format koordinat Latitude atau Longitude tidak valid. Masukkan angka seperti -6.2088 atau -5,4520');
                return;
            }

            const gkey = document.getElementById('cfg-gmaps-key').value.trim();
            const endpoint = document.getElementById('cfg-iot-endpoint').value.trim();
            const autoSync = cfgAutoSyncEsp ? cfgAutoSyncEsp.checked : true;

            localStorage.setItem('geoshield_auto_sync_esp', autoSync.toString());

            if (window.geoMap) {
                window.geoMap.googleMapsApiKey = gkey;
                window.geoMap.updateStationLocation(lat, lng, name);
            }
            if (window.iotService) {
                window.iotService.setCustomEndpoint(endpoint);
            }

            closeConfig();
        });
    }

    // Geolocation API (Browser GPS)
    if (btnGetCurrentLoc) {
        btnGetCurrentLoc.addEventListener('click', () => {
            if (!navigator.geolocation) {
                alert('Browser Anda tidak mendukung geolokasi GPS.');
                return;
            }
            btnGetCurrentLoc.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i> Mendeteksi GPS...';
            navigator.geolocation.getCurrentPosition(
                async (pos) => {
                    btnGetCurrentLoc.innerHTML = '<i class="fa-solid fa-check text-success"></i> GPS Ditemukan!';
                    const lat = pos.coords.latitude;
                    const lng = pos.coords.longitude;
                    document.getElementById('cfg-lat').value = lat.toFixed(6);
                    document.getElementById('cfg-lng').value = lng.toFixed(6);

                    // Reverse geocode to suggest name
                    if (window.geoMap) {
                        try {
                            const url = `https://nominatim.openstreetmap.org/reverse?format=json&lat=${lat}&lon=${lng}`;
                            const res = await fetch(url, { headers: { 'Accept-Language': 'id,en' } });
                            const data = await res.json();
                            if (data && data.address) {
                                const city = data.address.city || data.address.town || data.address.county || data.address.state || 'Posko Saya';
                                document.getElementById('cfg-station-name').value = `Stasiun Sensor - ${city}`;
                            }
                        } catch(e) {}
                    }

                    setTimeout(() => {
                        btnGetCurrentLoc.innerHTML = '<i class="fa-solid fa-location-crosshairs text-primary"></i> Gunakan GPS Saya';
                    }, 2000);
                },
                (err) => {
                    alert('Gagal mengambil lokasi GPS: ' + err.message);
                    btnGetCurrentLoc.innerHTML = '<i class="fa-solid fa-location-crosshairs text-primary"></i> Gunakan GPS Saya';
                }
            );
        });
    }

    // Manual Trigger: Pull & Sync Location from ESP32 Endpoint
    if (btnSyncEspLocation) {
        btnSyncEspLocation.addEventListener('click', async () => {
            const endpoint = document.getElementById('cfg-iot-endpoint').value.trim();
            btnSyncEspLocation.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i> Menghubungi ESP32...';

            if (endpoint && endpoint.startsWith('http')) {
                try {
                    const res = await fetch(endpoint);
                    const data = await res.json();
                    if (data && data.location) {
                        document.getElementById('cfg-lat').value = data.location.lat;
                        document.getElementById('cfg-lng').value = data.location.lng;
                        if (data.station_name || data.station_id) {
                            document.getElementById('cfg-station-name').value = data.station_name || `Stasiun ESP32 (${data.station_id})`;
                        }
                        btnSyncEspLocation.innerHTML = '<i class="fa-solid fa-check text-success"></i> Berhasil Disinkronkan!';
                    } else {
                        throw new Error('Format response ESP32 tidak memiliki field location');
                    }
                } catch (err) {
                    alert('Gagal mengambil lokasi dari endpoint ESP32: ' + err.message + '\nPastikan ESP32 satu jaringan WiFi dan endpoint aktif.');
                    btnSyncEspLocation.innerHTML = '<i class="fa-solid fa-microchip text-warning"></i> Kalibrasi dari ESP32';
                }
            } else {
                // If in demo simulator mode, simulate pulling from connected ESP32 node
                setTimeout(() => {
                    btnSyncEspLocation.innerHTML = '<i class="fa-solid fa-check text-success"></i> Kalibrasi ESP32 OK!';
                    document.getElementById('cfg-station-name').value = 'Stasiun ESP32 Posko Kalianda (Lampung)';
                    document.getElementById('cfg-lat').value = '-5.452078';
                    document.getElementById('cfg-lng').value = '105.395752';
                    setTimeout(() => {
                        btnSyncEspLocation.innerHTML = '<i class="fa-solid fa-microchip text-warning"></i> Kalibrasi dari ESP32';
                    }, 2500);
                }, 800);
            }
        });
    }

    // Open/Close Hardware Modal
    if (btnOpenHardware && modalHardware) {
        btnOpenHardware.addEventListener('click', () => modalHardware.classList.remove('hidden'));
    }
    const closeHardware = () => modalHardware && modalHardware.classList.add('hidden');
    if (btnCloseHardware) btnCloseHardware.addEventListener('click', closeHardware);

    // Hardware Modal Tabs
    const tabBtns = document.querySelectorAll('.tab-btn');
    const tabPanes = document.querySelectorAll('.tab-pane');
    tabBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const targetTab = btn.getAttribute('data-tab');
            tabBtns.forEach(b => b.classList.remove('active'));
            tabPanes.forEach(p => p.classList.remove('active'));

            btn.classList.add('active');
            const activePane = document.getElementById(`pane-${targetTab}`);
            if (activePane) activePane.classList.add('active');
        });
    });

    // 4. View Mode Switcher (OpenStreetMap vs Live Telemetry Charts vs Split)
    const tabViewBtns = document.querySelectorAll('.btn-tab-view');
    const mainMapCard = document.getElementById('main-map-card');
    const mainChartsCard = document.getElementById('main-charts-card');
    const rightPanel = document.querySelector('.right-panel');

    tabViewBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const view = btn.getAttribute('data-view');
            tabViewBtns.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');

            if (view === 'map') {
                if (mainMapCard) mainMapCard.classList.remove('hidden');
                if (mainChartsCard) mainChartsCard.classList.add('hidden');
                if (rightPanel) rightPanel.classList.remove('split-view-active');
                if (window.geoMap && window.geoMap.leafletMap) {
                    setTimeout(() => window.geoMap.leafletMap.invalidateSize(), 150);
                }
            } else if (view === 'charts') {
                if (mainMapCard) mainMapCard.classList.add('hidden');
                if (mainChartsCard) mainChartsCard.classList.remove('hidden');
                if (rightPanel) rightPanel.classList.remove('split-view-active');
            } else if (view === 'split') {
                if (mainMapCard) mainMapCard.classList.remove('hidden');
                if (mainChartsCard) mainChartsCard.classList.remove('hidden');
                if (rightPanel) rightPanel.classList.add('split-view-active');
                if (window.geoMap && window.geoMap.leafletMap) {
                    setTimeout(() => window.geoMap.leafletMap.invalidateSize(), 150);
                }
            }
        });
    });
});
