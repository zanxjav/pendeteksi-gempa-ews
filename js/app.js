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

    // Open/Close Config Modal
    if (btnOpenConfig && modalConfig) {
        btnOpenConfig.addEventListener('click', () => {
            if (window.geoMap) {
                document.getElementById('cfg-station-name').value = window.geoMap.station.name;
                document.getElementById('cfg-lat').value = window.geoMap.station.lat;
                document.getElementById('cfg-lng').value = window.geoMap.station.lng;
                document.getElementById('cfg-gmaps-key').value = window.geoMap.googleMapsApiKey || '';
            }
            modalConfig.classList.remove('hidden');
        });
    }

    const closeConfig = () => modalConfig && modalConfig.classList.add('hidden');
    if (btnCloseConfig) btnCloseConfig.addEventListener('click', closeConfig);
    if (btnCancelConfig) btnCancelConfig.addEventListener('click', closeConfig);

    // Save Station Config Form
    if (formConfig) {
        formConfig.addEventListener('submit', (e) => {
            e.preventDefault();
            const name = document.getElementById('cfg-station-name').value.trim();
            const lat = parseFloat(document.getElementById('cfg-lat').value);
            const lng = parseFloat(document.getElementById('cfg-lng').value);
            const gkey = document.getElementById('cfg-gmaps-key').value.trim();
            const endpoint = document.getElementById('cfg-iot-endpoint').value.trim();

            if (window.geoMap) {
                window.geoMap.googleMapsApiKey = gkey;
                window.geoMap.updateStationLocation(lat, lng, name);
            }
            if (window.iotService && endpoint) {
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
            btnGetCurrentLoc.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i> Mendeteksi Lokasi GPS...';
            navigator.geolocation.getCurrentPosition(
                (pos) => {
                    btnGetCurrentLoc.innerHTML = '<i class="fa-solid fa-check text-success"></i> Lokasi GPS Ditemukan!';
                    document.getElementById('cfg-lat').value = pos.coords.latitude.toFixed(6);
                    document.getElementById('cfg-lng').value = pos.coords.longitude.toFixed(6);
                    setTimeout(() => {
                        btnGetCurrentLoc.innerHTML = '<i class="fa-solid fa-location-crosshairs"></i> Gunakan Lokasi GPS Saya Saat Ini';
                    }, 2000);
                },
                (err) => {
                    alert('Gagal mengambil lokasi GPS: ' + err.message);
                    btnGetCurrentLoc.innerHTML = '<i class="fa-solid fa-location-crosshairs"></i> Gunakan Lokasi GPS Saya Saat Ini';
                }
            );
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

    // 3. Disaster Simulator Buttons
    const btnSimNormal = document.getElementById('btn-sim-normal');
    const btnSimLight = document.getElementById('btn-sim-light-quake');
    const btnSimHeavy = document.getElementById('btn-sim-heavy-quake');
    const btnSimFlood = document.getElementById('btn-sim-rain-flood');
    const btnSimEmergency = document.getElementById('btn-sim-emergency');

    if (btnSimNormal) btnSimNormal.addEventListener('click', () => window.sensorProcessor && window.sensorProcessor.simulateScenario('normal'));
    if (btnSimLight) btnSimLight.addEventListener('click', () => window.sensorProcessor && window.sensorProcessor.simulateScenario('light-quake'));
    if (btnSimHeavy) btnSimHeavy.addEventListener('click', () => window.sensorProcessor && window.sensorProcessor.simulateScenario('heavy-quake'));
    if (btnSimFlood) btnSimFlood.addEventListener('click', () => window.sensorProcessor && window.sensorProcessor.simulateScenario('rain-flood'));
    if (btnSimEmergency) btnSimEmergency.addEventListener('click', () => window.sensorProcessor && window.sensorProcessor.simulateScenario('emergency'));
});
