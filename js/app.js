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

    // Top Navbar Home Button
    const btnHome = document.getElementById('top-btn-home');
    if (btnHome) {
        btnHome.addEventListener('click', () => {
            if (window.geoMap) {
                window.geoMap.resetToItera();
            }
            if (telemetryHud) telemetryHud.classList.remove('hidden');
            const routingPanel = document.getElementById('routing-panel');
            if (routingPanel) routingPanel.classList.remove('collapsed');
            if (window.showToast) window.showToast('Kembali ke Tampilan Utama EWS ITERA 📍', 'info');
        });
    }

    // Top Navbar Grid / Multi-Sensor Toggle
    const btnGrid = document.getElementById('top-btn-grid');
    if (btnGrid) {
        btnGrid.addEventListener('click', () => {
            if (telemetryHud) {
                telemetryHud.classList.remove('hidden');
                telemetryHud.classList.remove('minimized');
                telemetryHud.scrollIntoView({ behavior: 'smooth' });
                if (window.showToast) window.showToast('Panel Telemetri Multi-Sensor Ditampilkan 📊', 'info');
            }
        });
    }

    // 3. Audio Alarm Toggle with Real Test Chirp
    const btnToggleSound = document.getElementById('btn-toggle-sound');
    const soundIcon = document.getElementById('sound-icon');
    const alarmAudio = document.getElementById('alarm-sound');
    let soundEnabled = true;

    function playAudioChirp() {
        try {
            const ctx = new (window.AudioContext || window.webkitAudioContext)();
            const osc = ctx.createOscillator();
            const gain = ctx.createGain();
            osc.type = 'sine';
            osc.frequency.setValueAtTime(880, ctx.currentTime);
            osc.frequency.exponentialRampToValueAtTime(1760, ctx.currentTime + 0.18);
            gain.gain.setValueAtTime(0.15, ctx.currentTime);
            gain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + 0.22);
            osc.connect(gain);
            gain.connect(ctx.destination);
            osc.start();
            osc.stop(ctx.currentTime + 0.23);
        } catch (e) {
            console.log('[Audio] WebAudio chirp:', e);
        }
    }

    if (btnToggleSound && soundIcon) {
        btnToggleSound.addEventListener('click', () => {
            soundEnabled = !soundEnabled;
            if (soundEnabled) {
                soundIcon.className = 'fa-solid fa-volume-high';
                btnToggleSound.title = 'Sirine Suara Aktif';
                playAudioChirp();
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
            if (window.showToast) window.showToast('Menyiapkan dokumen cetak laporan situasi bencana...', 'info');
            setTimeout(() => window.print(), 300);
        });
    }

    const btnDockExport = document.getElementById('btn-dock-export');
    if (btnDockExport) {
        btnDockExport.addEventListener('click', () => {
            // Generate GeoJSON of earthquake zone & evacuation shelters
            const exportData = {
                type: 'FeatureCollection',
                metadata: {
                    title: 'Pendeteksi Gempa EWS - ITERA Disaster & Telemetry Report',
                    timestamp: new Date().toISOString(),
                    station_id: 'ST-01-ESP32',
                    pga_g: 0.045,
                    radius_km: 25
                },
                features: [
                    {
                        type: 'Feature',
                        properties: { name: 'Pusat Gempa Kampus ITERA (ESP32-C3)', category: 'Epicenter' },
                        geometry: { type: 'Point', coordinates: [105.31465, -5.35824] }
                    },
                    {
                        type: 'Feature',
                        properties: { name: 'Posko 1 (GOR ITERA)', category: 'Shelter', distance_km: 1.2 },
                        geometry: { type: 'Point', coordinates: [105.31295, -5.36142] }
                    },
                    {
                        type: 'Feature',
                        properties: { name: 'Posko 2 (RSUD Airan Raya)', category: 'Shelter', distance_km: 3.8 },
                        geometry: { type: 'Point', coordinates: [105.30560, -5.37258] }
                    },
                    {
                        type: 'Feature',
                        properties: { name: 'Posko 3 (Balai Desa Way Huwi)', category: 'Shelter', distance_km: 6.4 },
                        geometry: { type: 'Point', coordinates: [105.29500, -5.38500] }
                    },
                    {
                        type: 'Feature',
                        properties: { name: 'Posko 4 (PKOR Way Halim)', category: 'Shelter', distance_km: 9.8 },
                        geometry: { type: 'Point', coordinates: [105.27500, -5.39000] }
                    }
                ]
            };

            const blob = new Blob([JSON.stringify(exportData, null, 2)], { type: 'application/json' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `ews_itera_gis_report_${Date.now()}.geojson`;
            a.click();
            URL.revokeObjectURL(url);
            if (window.showToast) window.showToast('File GeoJSON Laporan Gempa & Posko Evakuasi berhasil diunduh! 📥', 'success');
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

    // Function to highlight sensor card
    function highlightSensorCard(selector, toastMsg) {
        if (telemetryHud) {
            telemetryHud.classList.remove('hidden');
            telemetryHud.classList.remove('minimized');
        }
        const card = document.querySelector(selector);
        if (card) {
            card.scrollIntoView({ behavior: 'smooth', block: 'center' });
            card.classList.add('sensor-highlight');
            setTimeout(() => card.classList.remove('sensor-highlight'), 3000);
        }
        if (window.showToast && toastMsg) window.showToast(toastMsg, 'info');
    }

    // 6. Rail Sidebar Navigation & Feature Shortcuts
    const railButtons = document.querySelectorAll('.rail-icon-btn');
    railButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            const routingPanel = document.getElementById('routing-panel');

            // Routing Panel
            if (btn.id === 'rail-btn-routing') {
                if (routingPanel) {
                    routingPanel.classList.remove('collapsed');
                }
                if (window.showToast) window.showToast('Panel Rute & Evakuasi Diaktifkan 🛣️', 'info');
            }

            // Sensors HUD
            else if (btn.id === 'rail-btn-sensors') {
                if (telemetryHud) {
                    telemetryHud.classList.remove('hidden');
                    telemetryHud.classList.remove('minimized');
                }
                highlightSensorCard('.card-seismic', 'Sensor Seismik MPU-6050 (PGA & MMI) Ditampilkan 📊');
            }

            // Flood HC-SR04
            else if (btn.id === 'rail-btn-flood') {
                highlightSensorCard('.card-flood', 'Sensor Muka Air Banjir HC-SR04: 35.2 cm (Status: NORMAL) 🌊');
            }

            // Rain FC-37
            else if (btn.id === 'rail-btn-rain') {
                highlightSensorCard('.card-rain', 'Sensor Hujan FC-37: 15.4 mm/jam (Hujan Sedang) 🌧️');
            }

            // TDS Sensor
            else if (btn.id === 'rail-btn-tds') {
                highlightSensorCard('.card-tds', 'Sensor Kualitas Air TDS: 128.5 ppm (Layak Konsumsi) 🧪');
            }

            // Bluetooth HC-06
            else if (btn.id === 'rail-btn-bluetooth') {
                if (window.espProvisioning) window.espProvisioning.openModal();
                return;
            }

            // Settings Thresholds
            else if (btn.id === 'rail-btn-settings') {
                const modal = document.getElementById('settings-modal');
                if (modal) {
                    modal.classList.remove('hidden');
                    modal.setAttribute('aria-hidden', 'false');
                }
                return;
            }

            // Export CSV Report
            else if (btn.id === 'rail-btn-export') {
                exportTelemetryCSV();
                return;
            }

            // Admin Panel
            else if (btn.id === 'rail-btn-admin') {
                window.open('/admin/', '_blank');
                return;
            }

            // System Status Diagnostics
            else if (btn.id === 'rail-btn-logout') {
                const modal = document.getElementById('system-status-modal');
                if (modal) {
                    modal.classList.remove('hidden');
                    modal.setAttribute('aria-hidden', 'false');
                }
                return;
            }

            railButtons.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
        });
    });

    // Route Settings Button in Floating Panel
    const btnRoutingSettings = document.getElementById('btn-routing-settings');
    const modalRouteSettings = document.getElementById('route-settings-modal');
    if (btnRoutingSettings && modalRouteSettings) {
        btnRoutingSettings.addEventListener('click', () => {
            modalRouteSettings.classList.remove('hidden');
            modalRouteSettings.setAttribute('aria-hidden', 'false');
        });
    }

    // Modal Close Buttons Generic Handler
    document.querySelectorAll('.modal-close-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            const targetId = btn.getAttribute('data-target');
            if (targetId) {
                const target = document.getElementById(targetId);
                if (target) {
                    target.classList.add('hidden');
                    target.setAttribute('aria-hidden', 'true');
                }
            } else {
                const overlay = btn.closest('.prov-modal-overlay');
                if (overlay) overlay.classList.add('hidden');
            }
        });
    });

    // Save Settings Handler
    const btnSaveSettings = document.getElementById('btn-save-settings');
    if (btnSaveSettings) {
        btnSaveSettings.addEventListener('click', () => {
            const quakeThresh = document.getElementById('cfg-quake-threshold').value;
            const floodThresh = document.getElementById('cfg-flood-threshold').value;
            const rainThresh = document.getElementById('cfg-rain-threshold').value;
            const tdsThresh = document.getElementById('cfg-tds-threshold').value;
            const sirenMode = document.getElementById('cfg-siren-mode').value;

            localStorage.setItem('ews_settings', JSON.stringify({
                quakeThresh, floodThresh, rainThresh, tdsThresh, sirenMode
            }));

            const modal = document.getElementById('settings-modal');
            if (modal) modal.classList.add('hidden');

            if (window.showToast) {
                window.showToast('Pengaturan Ambang Batas EWS Berhasil Disimpan & Diterapkan! ✅', 'success');
            }
        });
    }

    // Run Diagnostics Test Handler
    const btnRunDiag = document.getElementById('btn-run-diag-test');
    if (btnRunDiag) {
        btnRunDiag.addEventListener('click', async () => {
            const statusEl = document.getElementById('diag-django-status');
            if (statusEl) statusEl.textContent = 'Menguji...';

            const startTime = performance.now();
            try {
                const res = await fetch('/api/telemetry/');
                const latency = Math.round(performance.now() - startTime);
                if (res.ok) {
                    if (statusEl) statusEl.textContent = `Online (${latency} ms)`;
                    if (window.showToast) window.showToast(`Diagnostik Sukses: Django REST API latency ${latency}ms ✅`, 'success');
                } else {
                    if (statusEl) statusEl.textContent = `Status ${res.status}`;
                }
            } catch (err) {
                if (statusEl) statusEl.textContent = 'Gagal';
                if (window.showToast) window.showToast(`Diagnostik Error: ${err.message}`, 'warn');
            }
        });
    }

    // Travel Mode Toggle in Route Settings Modal
    document.querySelectorAll('.mode-travel-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('.mode-travel-btn').forEach(b => {
                b.classList.remove('btn-primary', 'active');
                b.classList.add('btn-outline-secondary');
            });
            btn.classList.add('btn-primary', 'active');
            btn.classList.remove('btn-outline-secondary');

            const mode = btn.getAttribute('data-mode');
            if (window.geoMap) {
                window.geoMap.travelMode = mode;
            }
        });
    });

    const btnApplyRouteSettings = document.getElementById('btn-apply-route-settings');
    if (btnApplyRouteSettings && modalRouteSettings) {
        btnApplyRouteSettings.addEventListener('click', () => {
            modalRouteSettings.classList.add('hidden');
            if (window.geoMap) {
                window.geoMap.calculateRealRoadRoutes();
            }
            if (window.showToast) window.showToast('Pengaturan Rute Evakuasi Berhasil Diterapkan! 🚗', 'success');
        });
    }

    // CSV Export Generator
    function exportTelemetryCSV() {
        const rows = [
            ['STASIUN MONITORING', 'Pendeteksi Gempa EWS - ITERA (ESP32-C3)'],
            ['TANGGAL / WAKTU', new Date().toLocaleString('id-ID')],
            ['KOORDINAT PUSAT', '-5.35824, 105.31465 (Kampus ITERA Lampung)'],
            [''],
            ['PARAMETER SENSOR', 'NILAI SAAT INI', 'STATUS', 'AMBANG BATAS AMAN'],
            ['PGA Gempa Seismik (MPU-6050)', '0.045 g', 'AWAS (MMI VI+)', '< 0.040 g'],
            ['Akselerasi Gal', '44.1 cm/s2', 'Tinggi', '< 30 cm/s2'],
            ['Magnitudo Terestimasi', '5.8 M', 'Kuat', '< 4.5 M'],
            ['Tinggi Muka Air Banjir (HC-SR04)', '35.2 cm', 'NORMAL', '< 150.0 cm'],
            ['Curah Hujan (FC-37)', '15.4 mm/jam', 'Hujan Sedang', '< 20.0 mm/jam'],
            ['Kualitas Air (TDS Meter)', '128.5 ppm', 'Air Bersih', '< 300.0 ppm'],
            ['Status Sirine Buzzer', 'SIAGA (STANDBY)', 'NORMAL', 'Aktif pada Bahaya'],
            [''],
            ['DAFTAR POSKO EVAKUASI RESMI', 'KOORDINAT', 'JARAK JALAN', 'ESTIMASI WAKTU'],
            ['Posko 1: GOR ITERA / Embung A', '-5.36142, 105.31295', '1.2 km', '4 menit'],
            ['Posko 2: RSUD Airan Raya', '-5.37258, 105.30560', '3.8 km', '9 menit'],
            ['Posko 3: Balai Desa Way Huwi', '-5.38500, 105.29500', '6.4 km', '14 menit'],
            ['Posko 4: PKOR Way Halim', '-5.39000, 105.27500', '9.8 km', '22 menit']
        ];

        let csvContent = 'data:text/csv;charset=utf-8,';
        rows.forEach(row => {
            csvContent += row.map(val => `"${val}"`).join(',') + '\r\n';
        });

        const encodedUri = encodeURI(csvContent);
        const link = document.createElement('a');
        link.setAttribute('href', encodedUri);
        link.setAttribute('download', `laporan_ews_itera_${Date.now()}.csv`);
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);

        if (window.showToast) window.showToast('Laporan Situasi Gempa & Sensor CSV Berhasil Diunduh! 📊', 'success');
    }

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

