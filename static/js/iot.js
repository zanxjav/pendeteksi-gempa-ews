/**
 * GeoShield EWS - IoT Data Logger & Hardware Webhook Client
 * Handles telemetry logs, CSV export, and REST/WebSocket bridge for ESP32
 */

class IoTDataService {
    constructor() {
        this.logTableBody = document.getElementById('log-table-body');
        this.btnExportCsv = document.getElementById('btn-export-csv');
        this.iotConnBadge = document.getElementById('iot-conn-status');
        
        this.logs = [];
        this.maxLogs = 50;
        this.pollingTimer = null;
        this.customEndpoint = '';

        this.init();
    }

    init() {
        if (this.btnExportCsv) {
            this.btnExportCsv.addEventListener('click', () => this.exportCsv());
        }

        // Add initial system boot log
        this.addLog('Sistem Boot', 'Inisialisasi Sensor Selesai', 'NORMAL');

        // Periodic simulated logger (every 4 seconds) to populate logs
        setInterval(() => {
            if (window.sensorProcessor) {
                const data = window.sensorProcessor.currentData;
                if (data.seismic.status !== 'AMAN') {
                    this.addLog('Gempa Seismik', `${data.seismic.magnitude.toFixed(1)} SR (${data.seismic.pga.toFixed(3)}g)`, data.seismic.status);
                } else if (data.flood.status !== 'NORMAL') {
                    this.addLog('Muka Air Banjir', `${data.flood.waterLevelCm.toFixed(1)} cm`, data.flood.status);
                } else if (data.rain.status !== 'CERAH') {
                    this.addLog('Curah Hujan', `${data.rain.rateMmh.toFixed(1)} mm/jam`, data.rain.status);
                } else {
                    // Periodic normal heartbeat
                    if (Math.random() < 0.25) {
                        this.addLog('Telemetri Normal', `PGA: ${data.seismic.pga.toFixed(3)}g | Air: ${data.flood.waterLevelCm.toFixed(0)}cm | TDS: ${data.tds.ppm}ppm`, 'NORMAL');
                    }
                }
            }
        }, 3500);
    }

    addLog(eventType, sensorValue, status) {
        const now = new Date();
        const timeStr = now.toLocaleTimeString('id-ID', { hour: '2-digit', minute: '2-digit', second: '2-digit' });

        const logEntry = {
            time: timeStr,
            iso: now.toISOString(),
            eventType,
            sensorValue,
            status
        };

        this.logs.unshift(logEntry);
        if (this.logs.length > this.maxLogs) {
            this.logs.pop();
        }

        this.renderLogRow(logEntry);
    }

    renderLogRow(entry) {
        if (!this.logTableBody) return;

        let statusBadgeClass = 'badge-normal';
        if (entry.status === 'AWAS' || entry.status === 'BAHAYA' || entry.status === 'EKSTREM') {
            statusBadgeClass = 'badge-danger';
        } else if (entry.status === 'SIAGA' || entry.status === 'WASPADA' || entry.status === 'LEBAT') {
            statusBadgeClass = 'badge-warning';
        }

        const tr = document.createElement('tr');
        tr.innerHTML = `
            <td>${entry.time}</td>
            <td><strong>${entry.eventType}</strong></td>
            <td>${entry.sensorValue}</td>
            <td><span class="badge ${statusBadgeClass}">${entry.status}</span></td>
        `;

        this.logTableBody.insertBefore(tr, this.logTableBody.firstChild);

        // Keep table row limit
        while (this.logTableBody.children.length > this.maxLogs) {
            this.logTableBody.removeChild(this.logTableBody.lastChild);
        }
    }

    exportCsv() {
        if (this.logs.length === 0) {
            alert('Belum ada data log telemetri untuk diexport.');
            return;
        }

        let csvContent = "data:text/csv;charset=utf-8,Timestamp,Event Type,Sensor Value,Status\n";
        this.logs.forEach(row => {
            csvContent += `"${row.iso}","${row.eventType}","${row.sensorValue}","${row.status}"\n`;
        });

        const encodedUri = encodeURI(csvContent);
        const link = document.createElement("a");
        link.setAttribute("href", encodedUri);
        link.setAttribute("download", `GeoShield_Telemetry_${Date.now()}.csv`);
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
    }

    // Connect to actual ESP32 HTTP Server or endpoint if configured
    // Connect to actual ESP32 HTTP Server or endpoint if configured
    setCustomEndpoint(url) {
        this.customEndpoint = url;
        if (this.pollingTimer) clearInterval(this.pollingTimer);

        if (url && url.startsWith('http')) {
            if (this.iotConnBadge) this.iotConnBadge.textContent = 'IoT: Terhubung ke ESP32';
            this.pollingTimer = setInterval(async () => {
                try {
                    const res = await fetch(url);
                    const data = await res.json();
                    if (data) {
                        this.processIncomingData(data);
                    }
                } catch (err) {
                    if (this.iotConnBadge) this.iotConnBadge.textContent = 'IoT: Gagal Polling ESP32';
                }
            }, 500);
        } else {
            if (this.iotConnBadge) this.iotConnBadge.textContent = 'IoT: Simulator Mode';
        }
    }

    // Process Ingested Data from ESP32 or Webhook
    processIncomingData(data) {
        // 1. Update Sensor Readings
        if (window.sensorProcessor) {
            window.sensorProcessor.updateData(data);
        }

        // 2. Auto-Calibrate Station Location if ESP32 sends coordinates
        const isAutoSync = localStorage.getItem('geoshield_auto_sync_esp') !== 'false';
        if (isAutoSync && data.location && data.location.lat && data.location.lng && window.geoMap) {
            const espLat = parseFloat(data.location.lat);
            const espLng = parseFloat(data.location.lng);
            const espName = data.station_name || (data.station_id ? `Stasiun ESP32 (${data.station_id})` : null);

            // Check if coordinates changed significantly
            const distDiff = Math.abs(window.geoMap.station.lat - espLat) + Math.abs(window.geoMap.station.lng - espLng);
            if (distDiff > 0.0001 || (espName && window.geoMap.station.name !== espName)) {
                window.geoMap.updateStationLocation(espLat, espLng, espName);
                this.addLog('Kalibrasi ESP32', `Lokasi disinkronkan: ${espName || 'Posko ESP32'} (${espLat.toFixed(4)}, ${espLng.toFixed(4)})`, 'NORMAL');
            }
        }
    }
}

window.iotService = new IoTDataService();
