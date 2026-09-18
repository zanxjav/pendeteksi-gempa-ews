/**
 * GeoShield EWS - Main Application Controller
 * Connects ESP32 Telemetry Engine, Leaflet GIS Map, and Command Center UI
 */

document.addEventListener('DOMContentLoaded', () => {
    // 1. Live Telemetry Polling from Django REST API
    let isPolling = true;
    let latestRecordId = null;

    async function fetchLiveTelemetry() {
        if (!isPolling) return;
        try {
            const res = await fetch('/api/telemetry/');
            if (res.ok) {
                const data = await res.json();
                if (Array.isArray(data) && data.length > 0) {
                    const latest = data[0];
                    if (!latestRecordId || latest.id !== latestRecordId) {
                        latestRecordId = latest.id;
                        processIncomingTelemetry(latest);
                    }
                }
            }
        } catch (err) {
            console.warn('[Telemetry] Fetch warning:', err.message);
        }
    }

    function processIncomingTelemetry(record) {
        // 1. Update Sensor Hub (MPU-6050, HC-SR04, FC-37, TDS)
        if (window.sensorHub) {
            window.sensorHub.updateData({
                seismic: {
                    pga: record.pga_g || 0.045,
                    gal: record.gal || (record.pga_g * 980.665),
                    magnitude: record.magnitude || 5.8,
                    mmi: record.mmi_scale || 'VI+ (Kuat)',
                    status: record.seismic_status || 'AWAS'
                },
                flood: {
                    waterLevelCm: record.water_level_cm || 35.2,
                    status: record.flood_status || 'NORMAL'
                },
                rain: {
                    rateMmh: record.rain_rate_mmh || 15.4,
                    rawAnalog: record.rain_raw_analog || 3200,
                    condition: record.rain_condition || 'Hujan Sedang'
                },
                tds: {
                    ppm: record.tds_ppm || 128.5,
                    quality: record.water_quality || 'Air Bersih / Layak'
                }
            });
        }

        // 2. Update Map Shaking Radius Circle & Epicenter
        if (window.geoMap) {
            window.geoMap.updateFromTelemetry(record);
        }

        // 3. Update Hardware Status in Telemetry HUD
        const elPga = document.getElementById('val-pga');
        if (elPga) elPga.textContent = `${(record.pga_g || 0.045).toFixed(3)} g`;

        const elGal = document.getElementById('val-gal');
        if (elGal) elGal.textContent = (record.gal || 44.1).toFixed(1);

        const elMag = document.getElementById('val-magnitude');
        if (elMag) elMag.textContent = `${(record.magnitude || 5.8).toFixed(1)} M`;

        const elMmi = document.getElementById('val-mmi');
        if (elMmi) elMmi.textContent = record.mmi_scale || 'VI+ (Kuat)';

        const elWater = document.getElementById('val-water-level');
        if (elWater) elWater.textContent = `${(record.water_level_cm || 35.2).toFixed(1)} cm`;

        const elWaterFill = document.getElementById('water-fill-level');
        if (elWaterFill) {
            const pct = Math.min(100, Math.round(((record.water_level_cm || 35.2) / 150) * 100));
            elWaterFill.style.width = `${pct}%`;
        }

        const elRain = document.getElementById('val-rain-rate');
        if (elRain) elRain.textContent = `${(record.rain_rate_mmh || 15.4).toFixed(1)} mm/jam`;

        const elTds = document.getElementById('val-tds');
        if (elTds) elTds.textContent = `${(record.tds_ppm || 128.5).toFixed(1)} ppm`;

        // Update Bottom Dock Pills
        const pillFlood = document.querySelector('#pill-sensor-flood span');
        if (pillFlood) pillFlood.innerHTML = `Air HC-SR04: <strong>${(record.water_level_cm || 35.2).toFixed(1)} cm</strong>`;

        const pillRain = document.querySelector('#pill-sensor-rain span');
        if (pillRain) pillRain.innerHTML = `Hujan FC-37: <strong>${(record.rain_rate_mmh || 15.4).toFixed(1)} mm/jam</strong>`;

        const pillTds = document.querySelector('#pill-sensor-tds span');
        if (pillTds) pillTds.innerHTML = `TDS: <strong>${(record.tds_ppm || 128.5).toFixed(1)} ppm</strong>`;

        // Buzzer local status
        const isEmergency = ((record.pga_g || 0.045) >= 0.08 || (record.water_level_cm || 35.2) >= 150);
        const buzzerText = document.getElementById('buzzer-status-text');
        if (buzzerText) {
            if (isEmergency) {
                buzzerText.textContent = '🚨 SIRINE BERBUNYI!';
                buzzerText.className = 'text-danger font-bold';
            } else {
                buzzerText.textContent = 'SIAGA (AMAN)';
                buzzerText.className = 'text-success font-bold';
            }
        }
    }

    // Poll every 2 seconds
    setInterval(fetchLiveTelemetry, 2000);
    fetchLiveTelemetry();

    // 2. Toggle ESP32 Telemetry HUD Panel
    const btnToggleHud = document.getElementById('btn-toggle-hud');
    const telemetryHud = document.getElementById('telemetry-hud');
    const btnMinHud = document.getElementById('btn-min-hud');

    if (btnToggleHud && telemetryHud) {
        btnToggleHud.addEventListener('click', () => {
            telemetryHud.classList.toggle('hidden');
            btnToggleHud.classList.toggle('active-accent');
        });
    }

    if (btnMinHud && telemetryHud) {
        btnMinHud.addEventListener('click', () => {
            telemetryHud.classList.toggle('minimized');
            const icon = btnMinHud.querySelector('i');
            if (telemetryHud.classList.contains('minimized')) {
                if (icon) icon.className = 'fa-solid fa-chevron-up';
            } else {
                if (icon) icon.className = 'fa-solid fa-chevron-down';
            }
        });
    }

    // 3. Audio Alarm Toggle
    const btnToggleSound = document.getElementById('btn-toggle-sound');
    const soundIcon = document.getElementById('sound-icon');
    const alarmAudio = document.getElementById('alarm-sound');
    let soundEnabled = true;

    if (btnToggleSound && soundIcon) {
        btnToggleSound.addEventListener('click', () => {
            soundEnabled = !soundEnabled;
            if (soundEnabled) {
                soundIcon.className = 'fa-solid fa-volume-high';
                btnToggleSound.title = 'Sirine Suara Aktif';
                if (window.showToast) window.showToast('Sirine Suara EWS Diaktifkan 🔊', 'info');
            } else {
                soundIcon.className = 'fa-solid fa-volume-xmark';
                btnToggleSound.title = 'Sirine Dibisukan';
                if (alarmAudio) {
                    alarmAudio.pause();
                    alarmAudio.currentTime = 0;
                }
                if (window.showToast) window.showToast('Sirine Suara EWS Dibisukan 🔇', 'warn');
            }
        });
    }

    // 4. Print & Export File Actions (Bottom Dock)
    const btnDockPrint = document.getElementById('btn-dock-print');
    if (btnDockPrint) {
        btnDockPrint.addEventListener('click', () => {
            window.print();
        });
    }

    const btnDockExport = document.getElementById('btn-dock-export');
    if (btnDockExport) {
        btnDockExport.addEventListener('click', () => {
            // Generate GeoJSON of earthquake zone & evacuation shelters
            const exportData = {
                type: 'FeatureCollection',
                metadata: {
                    title: 'GeoShield EWS - Earthquake Impact & Evacuation Routes',
                    timestamp: new Date().toISOString(),
                    station_id: 'ST-01-ESP32',
                    pga_g: 0.045,
                    radius_km: 25
                },
                features: [
                    {
                        type: 'Feature',
                        properties: { name: 'Pusat Gempa ITERA (ESP32-C3)', category: 'Epicenter' },
                        geometry: { type: 'Point', coordinates: [105.3179, -5.4267] }
                    },
                    {
                        type: 'Feature',
                        properties: { name: 'Posko 1 (GOR ITERA)', category: 'Shelter', distance_km: 3.2 },
                        geometry: { type: 'Point', coordinates: [105.3280, -5.4050] }
                    },
                    {
                        type: 'Feature',
                        properties: { name: 'Posko 2 (RSUD Airan)', category: 'Shelter', distance_km: 6.5 },
                        geometry: { type: 'Point', coordinates: [105.3420, -5.3920] }
                    }
                ]
            };

            const blob = new Blob([JSON.stringify(exportData, null, 2)], { type: 'application/json' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `geoshield_earthquake_zone_${Date.now()}.geojson`;
            a.click();
            URL.revokeObjectURL(url);
            if (window.showToast) window.showToast('File GeoJSON Laporan Gempa berhasil diunduh!', 'success');
        });
    }

    // 5. Delete individual filter pills in bottom dock
    document.querySelectorAll('.pill-del-btn').forEach(btn => {
        btn.addEventListener('click', (e) => {
            e.stopPropagation();
            const pill = btn.closest('.dock-pill');
            if (pill) {
                pill.style.opacity = '0';
                pill.style.transform = 'scale(0.8)';
                pill.style.transition = 'all 0.2s';
                setTimeout(() => pill.remove(), 200);
            }
        });
    });

    // 6. Rail Sidebar Navigation
    const railButtons = document.querySelectorAll('.rail-icon-btn');
    railButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            if (btn.id === 'rail-btn-bluetooth') {
                if (window.espProvisioning) window.espProvisioning.openModal();
                return;
            }
            if (btn.id === 'rail-btn-sensors') {
                if (telemetryHud) telemetryHud.classList.remove('hidden');
                return;
            }
            railButtons.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
        });
    });

    // Reconnect Bluetooth button in HUD
    const btnReconnectBt = document.getElementById('btn-reconnect-bt');
    if (btnReconnectBt) {
        btnReconnectBt.addEventListener('click', () => {
            if (window.espProvisioning) window.espProvisioning.openModal();
        });
    }

    // 7. Toast Notification Helper
    window.showToast = (msg, type = 'info') => {
        const toast = document.createElement('div');
        toast.className = `gis-toast toast-${type}`;
        toast.style.cssText = `
            position: fixed;
            bottom: 60px;
            right: 20px;
            background: #0F172A;
            color: #FFFFFF;
            padding: 10px 18px;
            border-radius: 8px;
            box-shadow: 0 10px 25px rgba(0,0,0,0.5);
            font-size: 12.5px;
            font-weight: 600;
            display: flex;
            align-items: center;
            gap: 10px;
            z-index: 99999;
            border: 1px solid rgba(255,255,255,0.15);
            border-left: 4px solid ${type === 'success' ? '#10B981' : (type === 'warn' ? '#F59E0B' : '#06B6D4')};
            animation: slideUp 0.3s ease-out;
        `;
        toast.innerHTML = `<i class="fa-solid ${type === 'success' ? 'fa-circle-check text-green' : 'fa-circle-info text-cyan'}"></i> ${msg}`;
        document.body.appendChild(toast);
        setTimeout(() => {
            toast.style.opacity = '0';
            toast.style.transition = 'opacity 0.4s ease';
            setTimeout(() => toast.remove(), 400);
        }, 3200);
    };
});
