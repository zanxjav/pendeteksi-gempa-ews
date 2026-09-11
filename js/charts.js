/**
 * GeoShield EWS - Telemetry Charts Engine (Chart.js)
 * Real-time dynamic graphing for Seismic (PGA/Gal), Flood Water Level, Rainfall, and TDS.
 */

class TelemetryChartsEngine {
    constructor() {
        this.maxPoints = 30; // 30 data points (~30 seconds at 1s interval)
        this.isPaused = false;
        
        // Chart instances
        this.seismicChart = null;
        this.floodRainChart = null;
        this.tdsChart = null;

        // Data arrays
        this.labels = [];
        this.seismicData = { pga: [], gal: [], mag: [] };
        this.floodData = [];
        this.rainData = [];
        this.tdsData = [];

        // Initialize empty time series
        const now = new Date();
        for (let i = this.maxPoints - 1; i >= 0; i--) {
            const t = new Date(now.getTime() - i * 1000);
            const timeStr = t.toLocaleTimeString('id-ID', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
            this.labels.push(timeStr);
            this.seismicData.pga.push(0.002);
            this.seismicData.gal.push(1.96);
            this.seismicData.mag.push(0.0);
            this.floodData.push(42.5);
            this.rainData.push(0.0);
            this.tdsData.push(148);
        }

        this.initCharts();
        this.startDataFeed();
        this.initControls();
    }

    initCharts() {
        // Chart 1: Seismic (PGA & Gal)
        const ctxSeismic = document.getElementById('chart-seismic-canvas');
        if (ctxSeismic) {
            this.seismicChart = new Chart(ctxSeismic, {
                type: 'line',
                data: {
                    labels: this.labels,
                    datasets: [
                        {
                            label: 'PGA (g)',
                            data: this.seismicData.pga,
                            borderColor: '#38bdf8',
                            backgroundColor: 'rgba(56, 189, 248, 0.15)',
                            borderWidth: 2,
                            fill: true,
                            tension: 0.35,
                            yAxisID: 'y'
                        },
                        {
                            label: 'Percepatan Gal (cm/s²)',
                            data: this.seismicData.gal,
                            borderColor: '#f59e0b',
                            backgroundColor: 'transparent',
                            borderWidth: 1.5,
                            borderDash: [4, 4],
                            tension: 0.35,
                            yAxisID: 'y1'
                        }
                    ]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    animation: { duration: 250 },
                    scales: {
                        x: {
                            grid: { color: 'rgba(255, 255, 255, 0.05)' },
                            ticks: { color: '#64748b', font: { size: 10 } }
                        },
                        y: {
                            type: 'linear',
                            position: 'left',
                            title: { display: true, text: 'PGA (g)', color: '#38bdf8', font: { size: 10 } },
                            grid: { color: 'rgba(255, 255, 255, 0.05)' },
                            ticks: { color: '#38bdf8', font: { size: 10 } },
                            min: 0,
                            suggestedMax: 0.1
                        },
                        y1: {
                            type: 'linear',
                            position: 'right',
                            title: { display: true, text: 'Gal (cm/s²)', color: '#f59e0b', font: { size: 10 } },
                            grid: { drawOnChartArea: false },
                            ticks: { color: '#f59e0b', font: { size: 10 } },
                            min: 0,
                            suggestedMax: 100
                        }
                    },
                    plugins: {
                        legend: { labels: { color: '#cbd5e1', font: { size: 11 } } },
                        tooltip: { mode: 'index', intersect: false }
                    }
                }
            });
        }

        // Chart 2: Flood Water Level & Rainfall
        const ctxFloodRain = document.getElementById('chart-flood-rain-canvas');
        if (ctxFloodRain) {
            this.floodRainChart = new Chart(ctxFloodRain, {
                type: 'line',
                data: {
                    labels: this.labels,
                    datasets: [
                        {
                            label: 'Muka Air Banjir (cm)',
                            data: this.floodData,
                            borderColor: '#0284c7',
                            backgroundColor: 'rgba(2, 132, 199, 0.25)',
                            borderWidth: 2,
                            fill: true,
                            tension: 0.35,
                            yAxisID: 'yWater'
                        },
                        {
                            label: 'Curah Hujan (mm/jam)',
                            data: this.rainData,
                            borderColor: '#06b6d4',
                            backgroundColor: 'rgba(6, 182, 212, 0.15)',
                            borderWidth: 1.8,
                            tension: 0.35,
                            yAxisID: 'yRain'
                        }
                    ]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    animation: { duration: 250 },
                    scales: {
                        x: {
                            grid: { color: 'rgba(255, 255, 255, 0.05)' },
                            ticks: { color: '#64748b', font: { size: 10 } }
                        },
                        yWater: {
                            type: 'linear',
                            position: 'left',
                            title: { display: true, text: 'Muka Air (cm)', color: '#0284c7', font: { size: 10 } },
                            grid: { color: 'rgba(255, 255, 255, 0.05)' },
                            ticks: { color: '#0284c7', font: { size: 10 } },
                            min: 0,
                            suggestedMax: 180
                        },
                        yRain: {
                            type: 'linear',
                            position: 'right',
                            title: { display: true, text: 'Hujan (mm/h)', color: '#06b6d4', font: { size: 10 } },
                            grid: { drawOnChartArea: false },
                            ticks: { color: '#06b6d4', font: { size: 10 } },
                            min: 0,
                            suggestedMax: 100
                        }
                    },
                    plugins: {
                        legend: { labels: { color: '#cbd5e1', font: { size: 11 } } },
                        tooltip: { mode: 'index', intersect: false }
                    }
                }
            });
        }

        // Chart 3: TDS Water Purity
        const ctxTds = document.getElementById('chart-tds-canvas');
        if (ctxTds) {
            this.tdsChart = new Chart(ctxTds, {
                type: 'line',
                data: {
                    labels: this.labels,
                    datasets: [
                        {
                            label: 'Partikel Terlarut TDS (PPM)',
                            data: this.tdsData,
                            borderColor: '#10b981',
                            backgroundColor: 'rgba(16, 185, 129, 0.2)',
                            borderWidth: 2,
                            fill: true,
                            tension: 0.35
                        }
                    ]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    animation: { duration: 250 },
                    scales: {
                        x: {
                            grid: { color: 'rgba(255, 255, 255, 0.05)' },
                            ticks: { color: '#64748b', font: { size: 10 } }
                        },
                        y: {
                            title: { display: true, text: 'PPM', color: '#10b981', font: { size: 10 } },
                            grid: { color: 'rgba(255, 255, 255, 0.05)' },
                            ticks: { color: '#10b981', font: { size: 10 } },
                            min: 0,
                            suggestedMax: 1000
                        }
                    },
                    plugins: {
                        legend: { labels: { color: '#cbd5e1', font: { size: 11 } } },
                        tooltip: { mode: 'index', intersect: false }
                    }
                }
            });
        }
    }

    startDataFeed() {
        // Ingest latest sensor values every 1 second
        setInterval(() => {
            if (this.isPaused || !window.sensorProcessor) return;

            const now = new Date();
            const timeStr = now.toLocaleTimeString('id-ID', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
            const sData = window.sensorProcessor.currentData;

            // Shift timestamps
            this.labels.push(timeStr);
            this.labels.shift();

            // 1. Seismic
            this.seismicData.pga.push(sData.seismic.pga);
            this.seismicData.pga.shift();
            this.seismicData.gal.push(sData.seismic.gal);
            this.seismicData.gal.shift();

            // 2. Flood & Rain
            this.floodData.push(sData.flood.waterLevelCm);
            this.floodData.shift();
            this.rainData.push(sData.rain.rateMmh);
            this.rainData.shift();

            // 3. TDS
            this.tdsData.push(sData.tds.ppm);
            this.tdsData.shift();

            // Dynamic color adjustment on earthquake
            if (this.seismicChart) {
                const latestPga = sData.seismic.pga;
                if (latestPga > 0.08) {
                    this.seismicChart.data.datasets[0].borderColor = '#ef4444';
                    this.seismicChart.data.datasets[0].backgroundColor = 'rgba(239, 68, 68, 0.3)';
                } else if (latestPga > 0.02) {
                    this.seismicChart.data.datasets[0].borderColor = '#f59e0b';
                    this.seismicChart.data.datasets[0].backgroundColor = 'rgba(245, 158, 11, 0.2)';
                } else {
                    this.seismicChart.data.datasets[0].borderColor = '#38bdf8';
                    this.seismicChart.data.datasets[0].backgroundColor = 'rgba(56, 189, 248, 0.15)';
                }
                this.seismicChart.update('none');
            }

            if (this.floodRainChart) this.floodRainChart.update('none');
            if (this.tdsChart) this.tdsChart.update('none');
        }, 1000);
    }

    initControls() {
        const btnTogglePause = document.getElementById('btn-pause-charts');
        if (btnTogglePause) {
            btnTogglePause.addEventListener('click', () => {
                this.isPaused = !this.isPaused;
                if (this.isPaused) {
                    btnTogglePause.innerHTML = '<i class="fa-solid fa-play"></i> Lanjutkan Grafik';
                    btnTogglePause.classList.add('paused');
                } else {
                    btnTogglePause.innerHTML = '<i class="fa-solid fa-pause"></i> Jeda';
                    btnTogglePause.classList.remove('paused');
                }
            });
        }

        const btnClearCharts = document.getElementById('btn-clear-charts');
        if (btnClearCharts) {
            btnClearCharts.addEventListener('click', () => {
                this.seismicData.pga.fill(0.002);
                this.seismicData.gal.fill(1.96);
                this.floodData.fill(42.5);
                this.rainData.fill(0.0);
                this.tdsData.fill(148);
                if (this.seismicChart) this.seismicChart.update();
                if (this.floodRainChart) this.floodRainChart.update();
                if (this.tdsChart) this.tdsChart.update();
            });
        }
    }
}

// Global Charts Engine Instance
window.telemetryCharts = new TelemetryChartsEngine();
