/**
 * GeoShield EWS - Real Hardware UART Serial & Bluetooth HC-06 Provisioning Engine
 * Supports Web Serial API (Chrome/Edge COM Port) and Web Bluetooth API (Wireless HC-06)
 * Zero Fake Simulation - 100% Real Hardware Communication with Command Console
 */

class ESPProvisioningManager {
    constructor() {
        this.isConnected = false;
        this.connectionType = null; // 'serial' or 'bluetooth'
        
        // Web Serial Objects
        this.serialPort = null;
        this.serialReader = null;
        this.serialWriter = null;
        this.serialInputDone = null;
        this.serialOutputDone = null;

        // Web Bluetooth Objects
        this.bluetoothDevice = null;
        this.bluetoothServer = null;
        this.txCharacteristic = null;
        this.rxCharacteristic = null;

        // HTTP AP Polling (WiFi Hotspot ESP32-C3-EWS)
        this.httpPollInterval = null;
        this.lastLogIndex = 0;

        // DOM elements
        this.modal = document.getElementById('provisioning-modal');
        this.btnConnectWifiAp = document.getElementById('btn-connect-wifi-ap');
        this.btnConnectSerial = document.getElementById('btn-connect-serial');
        this.btnConnectBt = document.getElementById('btn-connect-bt');
        this.btnDisconnect = document.getElementById('btn-disconnect-comm');
        this.btnSendWifi = document.getElementById('btn-send-wifi-config');
        this.btnEnterDashboard = document.getElementById('btn-enter-dashboard');
        this.lockNotice = document.getElementById('gateway-lock-notice');

        this.inputSsid = document.getElementById('prov-wifi-ssid');
        this.inputPass = document.getElementById('prov-wifi-pass');
        this.cmdInput = document.getElementById('prov-terminal-cmd-input');
        this.btnSendCmd = document.getElementById('btn-terminal-send');

        this.terminalOutput = document.getElementById('prov-terminal-output');
        this.statusBadge = document.getElementById('prov-status-indicator');

        this.incomingBuffer = '';
        this.init();
    }

    init() {
        this.bindEvents();
        // Pastikan modal gateway terkunci terbuka saat pertama kali halaman dimuat
        if (this.modal) {
            this.modal.classList.add('active');
            this.modal.setAttribute('aria-hidden', 'false');
            document.body.style.overflow = 'hidden';
        }
    }

    bindEvents() {
        // 0. WiFi Hotspot AP Connect (ESP32-C3-EWS at 192.168.4.1) - BEST FOR MOBILE & DESKTOP
        if (this.btnConnectWifiAp) {
            this.btnConnectWifiAp.addEventListener('click', () => this.connectWifiAp());
        }

        // 1. Web Serial Connect (USB COM Port / Bluetooth Serial Port Windows)
        if (this.btnConnectSerial) {
            this.btnConnectSerial.addEventListener('click', () => this.connectSerial());
        }

        // 2. Web Bluetooth Connect (Wireless HC-06 / BLE)
        if (this.btnConnectBt) {
            this.btnConnectBt.addEventListener('click', () => this.connectBluetooth());
        }

        // 3. Putuskan Sambungan
        if (this.btnDisconnect) {
            this.btnDisconnect.addEventListener('click', () => this.disconnectAll());
        }

        // 4. Form Kirim WiFi (set:SSID,PASS)
        if (this.btnSendWifi) {
            this.btnSendWifi.addEventListener('click', () => this.sendWifiCredentials());
        }

        // 5. Input Bar Terminal Send Command
        if (this.btnSendCmd) {
            this.btnSendCmd.addEventListener('click', () => this.handleTerminalInput());
        }
        if (this.cmdInput) {
            this.cmdInput.addEventListener('keydown', (e) => {
                if (e.key === 'Enter') {
                    e.preventDefault();
                    this.handleTerminalInput();
                }
            });
        }

        // Quick Action Buttons
        const quickScan = document.getElementById('btn-quick-scan');
        if (quickScan) quickScan.addEventListener('click', () => this.sendCommand('scan wifi'));

        const quickInput = document.getElementById('btn-quick-input');
        if (quickInput) quickInput.addEventListener('click', () => this.sendCommand('masukan wifi'));

        const quickStatus = document.getElementById('btn-quick-status');
        if (quickStatus) quickStatus.addEventListener('click', () => this.sendCommand('status'));

        const quickClear = document.getElementById('btn-quick-clear');
        if (quickClear) quickClear.addEventListener('click', () => this.sendCommand('hapus wifi'));

        // 6. Masuk ke Dashboard (Hanya aktif setelah ESP32 terhubung)
        if (this.btnEnterDashboard) {
            this.btnEnterDashboard.addEventListener('click', () => {
                if (this.modal) {
                    this.modal.classList.remove('active');
                    this.modal.setAttribute('aria-hidden', 'true');
                    document.body.style.overflow = '';
                }
                if (window.showToast) {
                    window.showToast('✅ ESP32 Terkoneksi Realtime! Selamat datang di Dashboard.', 'success');
                }
            });
        }
    }

    logTerminal(msg, type = 'default') {
        if (!this.terminalOutput) return;

        const time = new Date().toLocaleTimeString('id-ID', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
        const line = document.createElement('div');
        line.className = `terminal-line line-${type}`;

        let prefix = `<span class="t-time">[${time}]</span> `;
        if (type === 'rx') prefix += `<span class="t-rx">&lt;-- ESP32:</span> `;
        else if (type === 'tx') prefix += `<span class="t-tx">--&gt; ANDA:</span> `;
        else if (type === 'success') prefix += `<span class="t-success">✅</span> `;
        else if (type === 'warn') prefix += `<span class="t-warn">⚠️</span> `;
        else if (type === 'error') prefix += `<span class="t-error">❌</span> `;
        else prefix += `<span class="t-info">ℹ️</span> `;

        line.innerHTML = prefix + msg;
        this.terminalOutput.appendChild(line);
        this.terminalOutput.scrollTop = this.terminalOutput.scrollHeight;
    }

    setStatus(text, type = 'warning') {
        if (this.statusBadge) {
            this.statusBadge.textContent = text;
            this.statusBadge.className = `status-pill ${type}`;
        }
    }

    // ==========================================================================
    // A. KONEKSI VIA WIFI HOTSPOT ACCESS POINT (ESP32-C3-EWS at 192.168.4.1)
    // SOLUSI TERBAIK UNTUK HP (ANDROID / IPHONE) & LAPTOP
    // ==========================================================================
    async connectWifiAp() {
        this.logTerminal('Mencoba menyambung ke ESP32 via WiFi AP (http://192.168.4.1)...', 'info');
        this.setStatus('Menghubungkan ke AP ESP32...', 'warning');

        try {
            const controller = new AbortController();
            const timeoutId = setTimeout(() => controller.abort(), 4000);

            const res = await fetch('http://192.168.4.1/api/status', {
                signal: controller.signal,
                mode: 'cors',
                cache: 'no-cache'
            });
            clearTimeout(timeoutId);

            if (!res.ok) throw new Error('HTTP Status: ' + res.status);
            const data = await res.json();

            this.isConnected = true;
            this.connectionType = 'http_ap';
            this.setStatus('WiFi AP ESP32 Terhubung! (192.168.4.1)', 'success');
            this.logTerminal('Berhasil terhubung ke Access Point ESP32-C3-EWS!', 'success');
            this.logTerminal(`Status ESP32: Mode=${data.status || 'AP'} | IP=${data.ip || '192.168.4.1'}`, 'info');

            if (this.btnConnectWifiAp) this.btnConnectWifiAp.classList.add('hidden');
            if (this.btnConnectSerial) this.btnConnectSerial.classList.add('hidden');
            if (this.btnConnectBt) this.btnConnectBt.classList.add('hidden');
            if (this.btnDisconnect) this.btnDisconnect.classList.remove('hidden');

            // Mulai polling logs langsung
            this.startHttpLogsPolling();

        } catch (err) {
            this.setStatus('Gagal Terhubung ke Hotspot', 'warning');
            this.logTerminal(`Gagal konek ke http://192.168.4.1: ${err.message}`, 'error');
            this.logTerminal('💡 PETUNJUK HP: Buka Pengaturan WiFi di HP Anda, sambungkan ke WiFi "ESP32-C3-EWS" (tanpa sandi), lalu klik kembali tombol hijau ini!', 'warn');
        }
    }

    startHttpLogsPolling() {
        if (this.httpPollInterval) clearInterval(this.httpPollInterval);

        this.httpPollInterval = setInterval(async () => {
            if (!this.isConnected || this.connectionType !== 'http_ap') return;

            try {
                const res = await fetch('http://192.168.4.1/api/logs', { cache: 'no-cache' });
                if (res.ok) {
                    const text = await res.text();
                    if (text.length > this.lastLogIndex) {
                        const newChunk = text.substring(this.lastLogIndex);
                        this.lastLogIndex = text.length;
                        this.handleIncomingData(newChunk);
                    }
                }
            } catch (e) {
                // Abaikan kesalahan periodik polling
            }
        }, 1200);
    }

    // ==========================================================================
    // B. KONEKSI VIA WEB SERIAL (COM PORT BLUETOOTH / USB)
    // ==========================================================================
    async connectSerial() {
        if (!('serial' in navigator)) {
            this.logTerminal('Browser Anda tidak mendukung Web Serial API. Gunakan Google Chrome atau Microsoft Edge di Laptop/PC.', 'warn');
            return;
        }

        try {
            this.logTerminal('Membuka dialog pemilihan Serial COM Port...', 'info');
            this.logTerminal('💡 Pilih COM Port Bluetooth HC-06 atau ESP32 yang terhubung.', 'muted');
            
            this.serialPort = await navigator.serial.requestPort();
            await this.serialPort.open({ baudRate: 9600 });

            this.isConnected = true;
            this.connectionType = 'serial';
            this.setStatus('Serial COM Terhubung (9600 Baud)', 'success');
            this.logTerminal('Port Serial UART HC-06 berhasil dibuka pada 9600 Baud!', 'success');

            if (this.btnConnectSerial) this.btnConnectSerial.classList.add('hidden');
            if (this.btnConnectBt) this.btnConnectBt.classList.add('hidden');
            if (this.btnDisconnect) this.btnDisconnect.classList.remove('hidden');

            // Setup Reader
            const textDecoder = new TextDecoderStream();
            this.serialInputDone = this.serialPort.readable.pipeTo(textDecoder.writable);
            this.serialReader = textDecoder.readable.getReader();

            // Setup Writer
            const textEncoder = new TextEncoderStream();
            this.serialOutputDone = textEncoder.readable.pipeTo(this.serialPort.writable);
            this.serialWriter = textEncoder.writable.getWriter();

            // Tampilkan status & menu
            this.readSerialLoop();
            setTimeout(() => {
                this.sendRaw('status\r\n');
            }, 600);

        } catch (err) {
            if (err.name !== 'NotFoundError') {
                this.logTerminal(`Gagal membuka port serial: ${err.message}`, 'error');
            }
        }
    }

    async readSerialLoop() {
        try {
            while (this.isConnected && this.serialReader) {
                const { value, done } = await this.serialReader.read();
                if (done) break;
                if (value) {
                    this.handleIncomingData(value);
                }
            }
        } catch (err) {
            console.warn('Serial Read Error:', err);
        }
    }

    // ==========================================================================
    // B. KONEKSI VIA WEB BLUETOOTH (WIRELESS HC-06 / BLE)
    // ==========================================================================
    async connectBluetooth() {
        if (!navigator.bluetooth) {
            this.logTerminal('Browser Anda tidak mendukung Web Bluetooth API. Gunakan Chrome di Android atau gunakan Serial COM di Windows.', 'warn');
            return;
        }

        try {
            this.logTerminal('Mencari perangkat Bluetooth dengan nama HC-06 / ESP32...', 'info');
            this.setStatus('Scanning Bluetooth HC-06...', 'warning');

            let device;
            try {
                // Prioritas: cari dengan filter nama perangkat (HC-06 / ESP) agar namanya muncul, bukan ID acak!
                device = await navigator.bluetooth.requestDevice({
                    filters: [
                        { namePrefix: 'HC' },
                        { namePrefix: 'hc' },
                        { namePrefix: 'ESP' },
                        { namePrefix: 'esp' },
                        { name: 'HC-06' },
                        { name: 'HC06' },
                        { name: 'HC-05' }
                    ],
                    optionalServices: [
                        '00001101-0000-1000-8000-00805f9b34fb', // Serial Port Profile (SPP)
                        '0000ffe0-0000-1000-8000-00805f9b34fb', // HC-06 BLE Service
                        '6e400001-b5a3-f393-e0a9-e50e24dcca9e'  // Nordic UART
                    ]
                });
            } catch (filterErr) {
                if (filterErr.name === 'NotFoundError') {
                    // Fallback jika nama tidak cocok dengan filter
                    this.logTerminal('Membuka dialog pencarian semua perangkat Bluetooth...', 'warn');
                    device = await navigator.bluetooth.requestDevice({
                        acceptAllDevices: true,
                        optionalServices: [
                            '00001101-0000-1000-8000-00805f9b34fb',
                            '0000ffe0-0000-1000-8000-00805f9b34fb',
                            '6e400001-b5a3-f393-e0a9-e50e24dcca9e'
                        ]
                    });
                } else {
                    throw filterErr;
                }
            }

            this.bluetoothDevice = device;
            const deviceName = device.name || 'Perangkat HC-06';
            this.logTerminal(`Perangkat Dipilih: <strong>${deviceName}</strong>`, 'info');

            const server = await device.gatt.connect();
            this.bluetoothServer = server;
            this.isConnected = true;
            this.connectionType = 'bluetooth';
            this.setStatus(`Terhubung: ${deviceName}`, 'success');
            this.logTerminal(`Koneksi Bluetooth ke ${deviceName} Berhasil!`, 'success');

            if (this.btnConnectSerial) this.btnConnectSerial.classList.add('hidden');
            if (this.btnConnectBt) this.btnConnectBt.classList.add('hidden');
            if (this.btnDisconnect) this.btnDisconnect.classList.remove('hidden');

            // Coba ambil UART characteristic
            const services = await server.getPrimaryServices();
            for (let s of services) {
                const chars = await s.getCharacteristics();
                for (let c of chars) {
                    if (c.properties.write || c.properties.writeWithoutResponse) {
                        this.txCharacteristic = c;
                    }
                    if (c.properties.notify || c.properties.read) {
                        this.rxCharacteristic = c;
                        await c.startNotifications();
                        c.addEventListener('characteristicvaluechanged', (e) => {
                            const val = new TextDecoder().decode(e.target.value);
                            this.handleIncomingData(val);
                        });
                    }
                }
            }

            this.sendRaw('status\r\n');

        } catch (err) {
            console.error('Bluetooth Error:', err);
            this.setStatus('Gagal Terhubung', 'warning');
            this.logTerminal(`Gagal Bluetooth: ${err.message}`, 'error');
        }
    }

    // ==========================================================================
    // C. PENGIRIMAN DATA KE HARDWARE (SERIAL / BLUETOOTH / WIFI AP)
    // ==========================================================================
    async sendRaw(data) {
        if (!this.isConnected) {
            this.logTerminal('Hardware belum tersambung! Silakan pilih salah satu tombol koneksi di panel kiri terlebih dahulu.', 'warn');
            return;
        }

        try {
            if (this.connectionType === 'http_ap') {
                // Kirim lewat endpoint HTTP Access Point ESP32 (Support HP & Laptop)
                const clean = data.replace(/[\r\n]+$/, '');
                await fetch('http://192.168.4.1/api/cmd?q=' + encodeURIComponent(clean), {
                    mode: 'cors',
                    cache: 'no-cache'
                });
            } else if (this.connectionType === 'serial' && this.serialWriter) {
                await this.serialWriter.write(data);
            } else if (this.connectionType === 'bluetooth' && this.txCharacteristic) {
                await this.txCharacteristic.writeValue(new TextEncoder().encode(data));
            }
        } catch (err) {
            console.error('Send Error:', err);
            this.logTerminal(`Gagal mengirim data: ${err.message}`, 'error');
        }
    }

    sendCommand(cmd) {
        const clean = cmd.trim();
        if (!clean) return;
        this.logTerminal(clean, 'tx');
        this.sendRaw(clean + '\r\n');
    }

    handleTerminalInput() {
        if (!this.cmdInput) return;
        const val = this.cmdInput.value.trim();
        if (!val) return;
        this.sendCommand(val);
        this.cmdInput.value = '';
    }

    sendWifiCredentials() {
        const ssid = this.inputSsid ? this.inputSsid.value.trim() : '';
        const pass = this.inputPass ? this.inputPass.value.trim() : '';

        if (!ssid) {
            alert('Silakan masukkan Nama WiFi (SSID)!');
            if (this.inputSsid) this.inputSsid.focus();
            return;
        }

        const cmd = `set:${ssid},${pass}`;
        this.logTerminal(`Mengirim konfigurasi WiFi: SSID="${ssid}"...`, 'info');
        this.sendCommand(cmd);

        if (this.btnSendWifi) {
            this.btnSendWifi.disabled = true;
            this.btnSendWifi.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i> Mengirim & Menyimpan...';
            setTimeout(() => {
                this.btnSendWifi.disabled = false;
                this.btnSendWifi.innerHTML = '<i class="fa-solid fa-paper-plane"></i> Kirim ke ESP32 & Sambungkan';
            }, 4000);
        }
    }

    // ==========================================================================
    // D. PENANGANAN DATA MASUK DARI ESP32 & BUKA KUNCI DASHBOARD
    // ==========================================================================
    handleIncomingData(raw) {
        this.incomingBuffer += raw;
        const lines = this.incomingBuffer.split(/\r?\n/);
        this.incomingBuffer = lines.pop(); // simpan sisa karakter tak lengkap

        for (let line of lines) {
            const clean = line.trim();
            if (clean.length > 0) {
                this.logTerminal(clean, 'rx');
                this.checkConnectionSuccess(clean);
            }
        }
    }

    checkConnectionSuccess(text) {
        const lower = text.toLowerCase();
        
        // Deteksi jika ESP32 berhasil terhubung ke WiFi
        const isConnected = lower.includes('wifi terhubung') ||
                            lower.includes('terhubung realtime') ||
                            lower.includes('ip address :') ||
                            lower.includes('wifi connected') ||
                            lower.includes('berhasil disimpan');

        if (isConnected) {
            this.setStatus('ESP32 Terhubung ke WiFi! 🟢', 'success');
            
            if (this.btnEnterDashboard) {
                this.btnEnterDashboard.classList.remove('hidden');
                this.btnEnterDashboard.classList.add('pulse-glow');
            }
            if (this.lockNotice) {
                this.lockNotice.innerHTML = '<span style="color:#10B981;font-weight:700;">✅ ESP32 Terhubung! Silakan klik tombol hijau di atas untuk membuka dashboard.</span>';
            }
        }
    }

    disconnectAll() {
        if (this.httpPollInterval) {
            clearInterval(this.httpPollInterval);
            this.httpPollInterval = null;
        }

        try {
            if (this.serialReader) {
                this.serialReader.cancel();
                this.serialReader = null;
            }
            if (this.serialWriter) {
                this.serialWriter.close();
                this.serialWriter = null;
            }
            if (this.serialPort) {
                this.serialPort.close();
                this.serialPort = null;
            }
            if (this.bluetoothDevice && this.bluetoothDevice.gatt.connected) {
                this.bluetoothDevice.gatt.disconnect();
            }
        } catch (e) {
            console.log('Disconnect cleanup:', e);
        }

        this.isConnected = false;
        this.connectionType = null;
        this.setStatus('Belum Terhubung', 'warning');
        this.logTerminal('Koneksi telah diputus.', 'warn');

        if (this.btnConnectWifiAp) this.btnConnectWifiAp.classList.remove('hidden');
        if (this.btnConnectSerial) this.btnConnectSerial.classList.remove('hidden');
        if (this.btnConnectBt) this.btnConnectBt.classList.remove('hidden');
        if (this.btnDisconnect) this.btnDisconnect.classList.add('hidden');
    }
}

// Inisialisasi Gateway saat dokumen siap
document.addEventListener('DOMContentLoaded', () => {
    window.provisioningManager = new ESPProvisioningManager();
});
