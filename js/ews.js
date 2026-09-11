/**
 * GeoShield EWS - Early Warning System Engine
 * Synthesizes warning sirens using Web Audio API and Indonesian Voice Alerts via Web Speech API
 */

class EarlyWarningSystem {
    constructor() {
        this.audioCtx = null;
        this.isSoundEnabled = true;
        this.isSirenActive = false;
        this.sirenOscillator = null;
        this.sirenGainNode = null;
        this.sirenLfo = null;
        this.lastSpokenTime = 0;
        this.currentAlertLevel = 'NORMAL'; // NORMAL, WASPADA, SIAGA, AWAS
        
        // Bind UI Elements
        this.alertOverlay = document.getElementById('ews-alert-overlay');
        this.alertTitle = document.getElementById('alert-modal-title');
        this.alertDesc = document.getElementById('alert-modal-desc');
        this.alertMagVal = document.getElementById('alert-mag-val');
        this.alertMmiVal = document.getElementById('alert-mmi-val');
        this.alertFloodVal = document.getElementById('alert-flood-val');
        this.btnSilence = document.getElementById('btn-silence-alarm');
        this.btnDismiss = document.getElementById('btn-dismiss-alert');
        this.btnToggleSound = document.getElementById('btn-toggle-sound');
        this.soundIcon = document.getElementById('sound-icon');
        this.systemStatusBadge = document.getElementById('system-status-badge');

        this.initEvents();
    }

    initAudioContext() {
        if (!this.audioCtx) {
            const AudioContext = window.AudioContext || window.webkitAudioContext;
            this.audioCtx = new AudioContext();
        }
        if (this.audioCtx.state === 'suspended') {
            this.audioCtx.resume();
        }
    }

    initEvents() {
        if (this.btnSilence) {
            this.btnSilence.addEventListener('click', () => this.stopSiren());
        }
        if (this.btnDismiss) {
            this.btnDismiss.addEventListener('click', () => this.dismissAlertModal());
        }
        if (this.btnToggleSound) {
            this.btnToggleSound.addEventListener('click', () => this.toggleSound());
        }

        // Initialize audio context on first user click anywhere
        document.addEventListener('click', () => {
            if (!this.audioCtx) this.initAudioContext();
        }, { once: true });
    }

    toggleSound() {
        this.isSoundEnabled = !this.isSoundEnabled;
        if (!this.isSoundEnabled) {
            this.stopSiren();
            this.soundIcon.className = 'fa-solid fa-volume-xmark';
            this.btnToggleSound.classList.add('muted');
        } else {
            this.soundIcon.className = 'fa-solid fa-volume-high';
            this.btnToggleSound.classList.remove('muted');
        }
    }

    startSiren(type = 'danger') {
        if (!this.isSoundEnabled) return;
        this.initAudioContext();
        if (this.isSirenActive) return;

        try {
            this.isSirenActive = true;
            const now = this.audioCtx.currentTime;

            // Main Oscillator (Dual frequency sweeping siren)
            this.sirenOscillator = this.audioCtx.createOscillator();
            this.sirenGainNode = this.audioCtx.createGain();

            this.sirenOscillator.type = type === 'danger' ? 'sawtooth' : 'sine';
            
            // Frequency modulation for police/disaster emergency wail (600Hz to 1200Hz)
            this.sirenOscillator.frequency.setValueAtTime(600, now);
            this.sirenLfo = this.audioCtx.createOscillator();
            this.sirenLfo.frequency.setValueAtTime(type === 'danger' ? 1.5 : 0.8, now); // LFO Speed
            
            const lfoGain = this.audioCtx.createGain();
            lfoGain.gain.setValueAtTime(type === 'danger' ? 400 : 200, now);

            this.sirenLfo.connect(lfoGain);
            lfoGain.connect(this.sirenOscillator.frequency);

            this.sirenGainNode.gain.setValueAtTime(0.01, now);
            this.sirenGainNode.gain.exponentialRampToValueAtTime(0.2, now + 0.3);

            this.sirenOscillator.connect(this.sirenGainNode);
            this.sirenGainNode.connect(this.audioCtx.destination);

            this.sirenOscillator.start();
            this.sirenLfo.start();
        } catch (e) {
            console.warn('Audio siren start issue:', e);
        }
    }

    stopSiren() {
        if (!this.isSirenActive) return;
        try {
            if (this.sirenGainNode && this.audioCtx) {
                this.sirenGainNode.gain.setValueAtTime(this.sirenGainNode.gain.value, this.audioCtx.currentTime);
                this.sirenGainNode.gain.exponentialRampToValueAtTime(0.0001, this.audioCtx.currentTime + 0.2);
            }
            setTimeout(() => {
                if (this.sirenOscillator) {
                    this.sirenOscillator.stop();
                    this.sirenOscillator.disconnect();
                }
                if (this.sirenLfo) {
                    this.sirenLfo.stop();
                    this.sirenLfo.disconnect();
                }
                this.isSirenActive = false;
            }, 250);
        } catch (e) {
            this.isSirenActive = false;
        }
    }

    speakIndonesianAlert(text) {
        if (!this.isSoundEnabled || !('speechSynthesis' in window)) return;
        const now = Date.now();
        // Prevent speech spam (minimum 8 seconds between voice announcements)
        if (now - this.lastSpokenTime < 8000) return;
        this.lastSpokenTime = now;

        window.speechSynthesis.cancel();
        const utterance = new SpeechSynthesisUtterance(text);
        utterance.lang = 'id-ID';
        utterance.rate = 1.05;
        utterance.pitch = 1.0;
        
        // Find Indonesian voice if available
        const voices = window.speechSynthesis.getVoices();
        const idVoice = voices.find(v => v.lang.includes('id') || v.lang.includes('ID') || v.name.includes('Indonesian'));
        if (idVoice) utterance.voice = idVoice;

        window.speechSynthesis.speak(utterance);
    }

    triggerEmergencyAlert(data) {
        const { type, title, desc, magnitude, mmi, floodLevel, tdsPpm } = data;
        this.currentAlertLevel = 'AWAS';

        // Update Overlay text
        if (this.alertTitle) this.alertTitle.textContent = title || 'PERINGATAN DINI BENCANA!';
        if (this.alertDesc) this.alertDesc.textContent = desc || 'Terjadi anomali sensor melebihi ambang batas darurat!';
        if (this.alertMagVal) this.alertMagVal.textContent = magnitude ? `${magnitude.toFixed(1)} M` : '-';
        if (this.alertMmiVal) this.alertMmiVal.textContent = mmi || '-';
        if (this.alertFloodVal) this.alertFloodVal.textContent = floodLevel ? `${floodLevel} cm` : 'Normal';

        // Show Modal
        if (this.alertOverlay) this.alertOverlay.classList.remove('hidden');

        // Play Siren & Speak
        this.startSiren('danger');
        
        if (type === 'quake') {
            this.speakIndonesianAlert(`Peringatan Dini! Gempa bumi terdeteksi dengan estimasi magnitudo ${magnitude.toFixed(1)}. Harap waspada dan cari perlindungan.`);
        } else if (type === 'flood') {
            this.speakIndonesianAlert(`Peringatan Dini! Potensi banjir tinggi terdeteksi. Ketinggian air mencapai ${floodLevel} sentimeter.`);
        } else {
            this.speakIndonesianAlert(`Peringatan Dini! Bahaya bencana terdeteksi pada stasiun sensor. Lakukan protokol darurat.`);
        }

        this.updateSystemStatus('red', 'AWAS DARURAT');
    }

    triggerWarningAlert(message, statusText = 'SIAGA') {
        this.currentAlertLevel = statusText;
        this.updateSystemStatus('yellow', statusText);
    }

    resetNormal() {
        this.currentAlertLevel = 'NORMAL';
        this.stopSiren();
        this.dismissAlertModal();
        this.updateSystemStatus('green', 'NORMAL');
    }

    dismissAlertModal() {
        if (this.alertOverlay) this.alertOverlay.classList.add('hidden');
    }

    updateSystemStatus(colorClass, label) {
        if (!this.systemStatusBadge) return;
        this.systemStatusBadge.innerHTML = `
            <span class="status-dot ${colorClass}"></span>
            <span class="status-label">SISTEM: <strong>${label}</strong></span>
        `;
    }
}

// Global EWS Instance
window.ews = new EarlyWarningSystem();
