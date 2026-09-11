/**
 * GeoShield EWS - Sensor Data Processing & Real-time Seismograph
 * Processes 4 main sensors:
 * 1. Seismic / Vibration Sensor (MPU6050 / SW420)
 * 2. Flood / Water Level Sensor (HC-SR04 Ultrasonic)
 * 3. Rain Sensor (FC-37 / Raindrop Analog)
 * 4. TDS Water Quality Sensor (Total Dissolved Solids Meter)
 */

class SensorProcessor {
    constructor() {
        // UI DOM Elements
        this.canvas = document.getElementById('seismographCanvas');
        this.ctx = this.canvas ? this.canvas.getContext('2d') : null;
        
        this.valPga = document.getElementById('val-pga');
        this.valFreq = document.getElementById('val-freq');
        this.valGal = document.getElementById('val-gal');
        this.valMagnitude = document.getElementById('val-magnitude');
        this.valMmi = document.getElementById('val-mmi');
        this.seismicBadge = document.getElementById('seismic-status-badge');

        this.valWaterLevel = document.getElementById('val-water-level');
        this.waterFillLevel = document.getElementById('water-fill-level');
        this.floodBadge = document.getElementById('flood-badge');

        this.valRainRate = document.getElementById('val-rain-rate');
        this.valRainRaw = document.getElementById('val-rain-raw');
        this.rainConditionText = document.getElementById('rain-condition-text');
        this.rainIconBox = document.getElementById('rain-icon-box');
        this.rainBadge = document.getElementById('rain-badge');

        this.valTds = document.getElementById('val-tds');
        this.tdsQualityText = document.getElementById('tds-quality-text');
        this.tdsCircleProgress = document.getElementById('tds-circle-progress');
        this.tdsBadge = document.getElementById('tds-badge');

        // Seismograph Waveform Data Buffer
        this.waveBuffer = [];
        this.bufferLength = 240; // ~4-5 seconds history at ~60fps
        for (let i = 0; i < this.bufferLength; i++) {
            this.waveBuffer.push(0);
        }

        // Sensor States
        this.currentData = {
            seismic: {
                pga: 0.002, // in g
                gal: 1.96,  // in cm/s² (1g = 980 cm/s²)
                freq: 0.2,  // in Hz
                magnitude: 0.0,
                mmi: 'I (Tidak Terasa)',
                rawZ: 0,
                status: 'AMAN'
            },
            flood: {
                waterLevelCm: 42.5,
                status: 'NORMAL' // NORMAL, WASPADA, SIAGA, AWAS
            },
            rain: {
                rateMmh: 0.0,
                rawAnalog: 4095, // 4095 = Kering (ESP32 ADC 12-bit)
                condition: 'Tidak ada presipitasi',
                status: 'NORMAL'
            },
            tds: {
                ppm: 148,
                quality: 'Air Layak / Aman',
                status: 'BERSIH'
            }
        };

        // Simulation Baseline Modifiers
        this.simState = {
            activeQuakeMag: 0,
            quakeDecay: 0,
            simRainIntensity: 0,
            simFloodLevel: 42.5
        };

        this.initCanvasResize();
        this.startRenderLoop();
    }

    initCanvasResize() {
        if (!this.canvas) return;
        const resize = () => {
            const rect = this.canvas.parentElement.getBoundingClientRect();
            this.canvas.width = rect.width;
            this.canvas.height = rect.height;
        };
        resize();
        window.addEventListener('resize', resize);
    }

    // Convert Peak Ground Acceleration (PGA) to Richter Magnitude & MMI
    calculateSeismicMetrics(pgaG) {
        const gal = pgaG * 980.665; // cm/s²
        let mmiText = 'I (Tidak Terasa)';
        let status = 'AMAN';
        let badgeClass = 'badge-normal';
        let magnitude = 0;

        if (gal < 1.4) {
            mmiText = 'I (Tidak Terasa)';
            status = 'AMAN';
            magnitude = 0.0;
        } else if (gal < 3.0) {
            mmiText = 'II (Getaran Lemah)';
            status = 'AMAN';
            magnitude = 2.0 + (gal / 3.0);
        } else if (gal < 9.0) {
            mmiText = 'III (Getaran Ringan)';
            status = 'WASPADA';
            badgeClass = 'badge-warning';
            magnitude = 3.0 + (gal / 9.0);
        } else if (gal < 30.0) {
            mmiText = 'IV - V (Sedang / Terasa Nyata)';
            status = 'SIAGA';
            badgeClass = 'badge-warning';
            magnitude = 4.2 + (gal / 40.0);
        } else if (gal < 100.0) {
            mmiText = 'VI (Kuat - Benda Terguncang)';
            status = 'AWAS';
            badgeClass = 'badge-danger';
            magnitude = 5.5 + (gal / 150.0);
        } else {
            mmiText = 'VII+ (Sangat Kuat / Merusak)';
            status = 'AWAS';
            badgeClass = 'badge-danger';
            magnitude = 6.5 + Math.min(2.5, gal / 300.0);
        }

        return { gal, mmiText, status, badgeClass, magnitude: Math.min(8.9, magnitude) };
    }

    // Ingest Data from ESP32 or Simulator
    updateData(data) {
        // 1. Seismic Update
        if (data.seismic) {
            const pga = data.seismic.pga !== undefined ? data.seismic.pga : this.currentData.seismic.pga;
            const metrics = this.calculateSeismicMetrics(pga);
            
            this.currentData.seismic = {
                pga: pga,
                gal: metrics.gal,
                freq: data.seismic.freq || (pga > 0.05 ? (3 + Math.random() * 8) : (0.1 + Math.random() * 0.3)),
                magnitude: metrics.magnitude,
                mmi: metrics.mmiText,
                status: metrics.status
            };

            // Update UI
            if (this.valPga) this.valPga.textContent = pga.toFixed(3);
            if (this.valGal) this.valGal.textContent = metrics.gal.toFixed(1);
            if (this.valFreq) this.valFreq.textContent = this.currentData.seismic.freq.toFixed(1);
            if (this.valMagnitude) this.valMagnitude.textContent = metrics.magnitude.toFixed(1);
            if (this.valMmi) this.valMmi.textContent = metrics.mmiText;
            if (this.seismicBadge) {
                this.seismicBadge.textContent = metrics.status;
                this.seismicBadge.className = `sensor-badge ${metrics.badgeClass}`;
            }

            // Trigger EWS if earthquake is severe
            if (metrics.status === 'AWAS' && window.ews && window.ews.currentAlertLevel !== 'AWAS') {
                window.ews.triggerEmergencyAlert({
                    type: 'quake',
                    title: 'GEMPA BUMI TERDETEKSI!',
                    desc: `Terjadi getaran berkekuatan ${metrics.magnitude.toFixed(1)} M (${metrics.mmiText}). Harap segera evakuasi ke tempat aman.`,
                    magnitude: metrics.magnitude,
                    mmi: metrics.mmiText,
                    floodLevel: this.currentData.flood.waterLevelCm
                });
                
                // Trigger map radius pulse
                if (window.geoMap) {
                    window.geoMap.showEarthquakeShakingZone(metrics.magnitude);
                }
            }
        }

        // 2. Flood / Water Level Update
        if (data.flood) {
            const wl = data.flood.waterLevelCm;
            this.currentData.flood.waterLevelCm = wl;
            
            // Calculate status
            let status = 'NORMAL';
            let badgeClass = 'badge-normal';
            if (wl >= 150) {
                status = 'AWAS';
                badgeClass = 'badge-danger';
            } else if (wl >= 100) {
                status = 'SIAGA';
                badgeClass = 'badge-warning';
            } else if (wl >= 75) {
                status = 'WASPADA';
                badgeClass = 'badge-warning';
            }

            this.currentData.flood.status = status;

            if (this.valWaterLevel) this.valWaterLevel.textContent = wl.toFixed(1);
            if (this.floodBadge) {
                this.floodBadge.textContent = status;
                this.floodBadge.className = `badge ${badgeClass}`;
            }
            if (this.waterFillLevel) {
                const pct = Math.min(100, Math.max(5, (wl / 200) * 100));
                this.waterFillLevel.style.height = `${pct}%`;
                if (status === 'AWAS') {
                    this.waterFillLevel.style.background = 'linear-gradient(180deg, #ef4444, #b91c1c)';
                } else if (status === 'SIAGA' || status === 'WASPADA') {
                    this.waterFillLevel.style.background = 'linear-gradient(180deg, #f59e0b, #d97706)';
                } else {
                    this.waterFillLevel.style.background = 'linear-gradient(180deg, #38bdf8, #0284c7)';
                }
            }

            // EWS Trigger for Flood
            if (status === 'AWAS' && window.ews && window.ews.currentAlertLevel !== 'AWAS') {
                window.ews.triggerEmergencyAlert({
                    type: 'flood',
                    title: 'BAHAYA BANJIR BANDANG!',
                    desc: `Ketinggian air telah mencapai ${wl.toFixed(1)} cm (Status AWAS). Aliran air berpotensi meluap ke pemukiman.`,
                    floodLevel: wl.toFixed(1),
                    tdsPpm: this.currentData.tds.ppm
                });
            }
        }

        // 3. Rain Sensor Update
        if (data.rain) {
            const raw = data.rain.rawAnalog !== undefined ? data.rain.rawAnalog : 4095;
            // 4095 (kering) -> 0 (basah kuyup)
            const rainRate = data.rain.rateMmh !== undefined ? data.rain.rateMmh : Math.max(0, ((4095 - raw) / 4095) * 80);
            
            let condition = 'Cerah / Kering';
            let iconClass = 'fa-solid fa-cloud-sun';
            let badgeClass = 'badge-normal';
            let badgeText = 'CERAH';

            if (rainRate < 0.5) {
                condition = 'Tidak ada presipitasi';
                badgeText = 'CERAH';
            } else if (rainRate < 5) {
                condition = 'Hujan Ringan / Gerimis';
                iconClass = 'fa-solid fa-cloud-rain';
                badgeText = 'GERIMIS';
            } else if (rainRate < 20) {
                condition = 'Hujan Sedang';
                iconClass = 'fa-solid fa-cloud-showers-heavy';
                badgeText = 'SEDANG';
                badgeClass = 'badge-warning';
            } else if (rainRate < 50) {
                condition = 'Hujan Lebat';
                iconClass = 'fa-solid fa-cloud-showers-water';
                badgeText = 'LEBAT';
                badgeClass = 'badge-danger';
            } else {
                condition = 'Hujan Sangat Lebat (Ekstrem)';
                iconClass = 'fa-solid fa-cloud-bolt';
                badgeText = 'EKSTREM';
                badgeClass = 'badge-danger';
            }

            this.currentData.rain = {
                rateMmh: rainRate,
                rawAnalog: raw,
                condition: condition,
                status: badgeText
            };

            if (this.valRainRate) this.valRainRate.textContent = rainRate.toFixed(1);
            if (this.valRainRaw) this.valRainRaw.textContent = raw;
            if (this.rainConditionText) this.rainConditionText.textContent = condition;
            if (this.rainBadge) {
                this.rainBadge.textContent = badgeText;
                this.rainBadge.className = `badge ${badgeClass}`;
            }
            if (this.rainIconBox) {
                this.rainIconBox.innerHTML = `<i class="${iconClass}"></i>`;
            }
        }

        // 4. TDS Sensor Update
        if (data.tds) {
            const ppm = data.tds.ppm;
            let quality = 'Air Murni / Sangat Layak';
            let badgeClass = 'badge-normal';
            let badgeText = 'BERSIH';
            let strokeColor = 'var(--clr-emerald)';

            if (ppm < 100) {
                quality = 'Air Sangat Murni';
                badgeText = 'MURNI';
            } else if (ppm <= 300) {
                quality = 'Air Bersih & Aman';
                badgeText = 'BERSIH';
            } else if (ppm <= 600) {
                quality = 'Air Cukup Bersih / Sedang';
                badgeText = 'SEDANG';
                badgeClass = 'badge-warning';
                strokeColor = 'var(--clr-warning)';
            } else if (ppm <= 1000) {
                quality = 'Tercemar / Keruh';
                badgeText = 'TERCEMAR';
                badgeClass = 'badge-danger';
                strokeColor = 'var(--clr-danger)';
            } else {
                quality = 'Lumpur / Air Berbahaya';
                badgeText = 'BAHAYA';
                badgeClass = 'badge-danger';
                strokeColor = 'var(--clr-danger)';
            }

            this.currentData.tds = { ppm, quality, status: badgeText };

            if (this.valTds) this.valTds.textContent = Math.round(ppm);
            if (this.tdsQualityText) {
                this.tdsQualityText.textContent = quality;
                this.tdsQualityText.style.color = strokeColor;
            }
            if (this.tdsBadge) {
                this.tdsBadge.textContent = badgeText;
                this.tdsBadge.className = `badge ${badgeClass}`;
            }
            if (this.tdsCircleProgress) {
                const pct = Math.min(100, (ppm / 1000) * 100);
                this.tdsCircleProgress.setAttribute('stroke-dasharray', `${pct}, 100`);
                this.tdsCircleProgress.style.stroke = strokeColor;
            }
        }
    }

    // Seismograph Continuous Render Loop
    startRenderLoop() {
        let frameCount = 0;

        const animate = () => {
            frameCount++;

            // 1. Calculate next wave value (Noise + Quake vibration)
            let waveVal = 0;
            const pga = this.currentData.seismic.pga;

            if (this.simState.activeQuakeMag > 0) {
                // Simulating earthquake waveform decay envelope
                const t = frameCount * 0.15;
                const envelope = Math.exp(-this.simState.quakeDecay);
                waveVal = (Math.sin(t * 8) * 0.6 + Math.sin(t * 14) * 0.4 + (Math.random() - 0.5) * 0.3) * (this.simState.activeQuakeMag * 15) * envelope;
                this.simState.quakeDecay += 0.005;

                // When decayed, reset
                if (envelope < 0.05) {
                    this.simState.activeQuakeMag = 0;
                    this.simState.quakeDecay = 0;
                    this.updateData({ seismic: { pga: 0.002 } });
                }
            } else if (pga > 0.01) {
                const t = frameCount * 0.2;
                waveVal = (Math.sin(t * 10) * 0.5 + (Math.random() - 0.5) * 0.5) * (pga * 80);
            } else {
                // Ambient micro-tremor baseline
                waveVal = (Math.random() - 0.5) * 2.5;
            }

            this.waveBuffer.push(waveVal);
            this.waveBuffer.shift();

            // 2. Render to Canvas
            if (this.ctx && this.canvas) {
                const w = this.canvas.width;
                const h = this.canvas.height;
                const midY = h / 2;

                this.ctx.clearRect(0, 0, w, h);

                // Centerline
                this.ctx.strokeStyle = 'rgba(56, 189, 248, 0.15)';
                this.ctx.lineWidth = 1;
                this.ctx.beginPath();
                this.ctx.moveTo(0, midY);
                this.ctx.lineTo(w, midY);
                this.ctx.stroke();

                // Seismic Waveform
                this.ctx.beginPath();
                const step = w / (this.bufferLength - 1);
                
                // Dynamic wave color based on severity
                if (pga > 0.08 || this.simState.activeQuakeMag >= 5.0) {
                    this.ctx.strokeStyle = '#ef4444'; // Neon Red
                    this.ctx.shadowColor = '#ef4444';
                    this.ctx.shadowBlur = 8;
                } else if (pga > 0.02 || this.simState.activeQuakeMag > 2.5) {
                    this.ctx.strokeStyle = '#f59e0b'; // Amber
                    this.ctx.shadowColor = '#f59e0b';
                    this.ctx.shadowBlur = 4;
                } else {
                    this.ctx.strokeStyle = '#38bdf8'; // Cyan
                    this.ctx.shadowColor = '#38bdf8';
                    this.ctx.shadowBlur = 2;
                }
                
                this.ctx.lineWidth = 1.8;

                for (let i = 0; i < this.bufferLength; i++) {
                    const x = i * step;
                    const y = midY - this.waveBuffer[i];
                    if (i === 0) this.ctx.moveTo(x, y);
                    else this.ctx.lineTo(x, y);
                }
                this.ctx.stroke();
                this.ctx.shadowBlur = 0; // Reset shadow
            }

            requestAnimationFrame(animate);
        };

        requestAnimationFrame(animate);
    }

    // Trigger Specific Simulation Scenarios
    simulateScenario(type) {
        switch (type) {
            case 'normal':
                this.simState.activeQuakeMag = 0;
                this.simState.quakeDecay = 0;
                this.updateData({
                    seismic: { pga: 0.002, freq: 0.2 },
                    flood: { waterLevelCm: 42.5 },
                    rain: { rawAnalog: 4095, rateMmh: 0.0 },
                    tds: { ppm: 148 }
                });
                if (window.ews) window.ews.resetNormal();
                if (window.geoMap) window.geoMap.clearDangerZones();
                break;

            case 'light-quake':
                this.simState.activeQuakeMag = 3.5;
                this.simState.quakeDecay = 0;
                this.updateData({
                    seismic: { pga: 0.028, freq: 4.2 },
                    flood: { waterLevelCm: 45.0 },
                    rain: { rawAnalog: 3800, rateMmh: 1.2 },
                    tds: { ppm: 165 }
                });
                if (window.ews) window.ews.triggerWarningAlert('Getaran gempa ringan (3.5 SR) terdeteksi.', 'WASPADA');
                if (window.geoMap) window.geoMap.showEarthquakeShakingZone(3.5);
                break;

            case 'heavy-quake':
                this.simState.activeQuakeMag = 6.8;
                this.simState.quakeDecay = 0;
                this.updateData({
                    seismic: { pga: 0.185, freq: 8.5 },
                    flood: { waterLevelCm: 55.0 },
                    rain: { rawAnalog: 3200, rateMmh: 5.0 },
                    tds: { ppm: 280 }
                });
                // Alert is auto-triggered in updateData
                break;

            case 'rain-flood':
                this.simState.activeQuakeMag = 0;
                this.updateData({
                    seismic: { pga: 0.003, freq: 0.3 },
                    flood: { waterLevelCm: 168.5 }, // Exceeds 150cm (AWAS)
                    rain: { rawAnalog: 250, rateMmh: 68.4 }, // Ekstrem
                    tds: { ppm: 890 } // Muddy flood water
                });
                if (window.geoMap) window.geoMap.showFloodZone(168.5);
                break;

            case 'emergency':
                this.simState.activeQuakeMag = 7.2;
                this.simState.quakeDecay = 0;
                this.updateData({
                    seismic: { pga: 0.25, freq: 11.2 },
                    flood: { waterLevelCm: 185.0 },
                    rain: { rawAnalog: 120, rateMmh: 92.0 },
                    tds: { ppm: 1250 }
                });
                if (window.geoMap) {
                    window.geoMap.showEarthquakeShakingZone(7.2);
                    window.geoMap.showFloodZone(185.0);
                }
                break;
        }
    }
}

// Global Sensor Processor Instance
window.sensorProcessor = new SensorProcessor();
