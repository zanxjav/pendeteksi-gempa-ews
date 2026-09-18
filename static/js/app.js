/**
 * GeoShield EWS - Main Application Controller
 * Handles Dashboard telemetry gauges, clock, PWA installation, and event binding
 */

document.addEventListener('DOMContentLoaded', () => {
    // 1. Live Date & Clock Formatter (Matching Screenshot: "18 Sep 2025 08:21" style)
    const liveTimeDisplay = document.getElementById('live-time-display');
    const specDateTime = document.getElementById('spec-val-datetime');

    const updateClock = () => {
        const now = new Date();
        const months = ['Jan', 'Feb', 'Mar', 'Apr', 'Mei', 'Jun', 'Jul', 'Agu', 'Sep', 'Okt', 'Nov', 'Des'];
        const day = String(now.getDate()).padStart(2, '0');
        const month = months[now.getMonth()];
        const year = now.getFullYear();
        const hour = String(now.getHours()).padStart(2, '0');
        const min = String(now.getMinutes()).padStart(2, '0');

        const formatted = `${day} ${month} ${year} ${hour}:${min}`;
        if (liveTimeDisplay) liveTimeDisplay.textContent = formatted;
        if (specDateTime) specDateTime.textContent = formatted;
    };
    setInterval(updateClock, 1000);
    updateClock();

    // 2. Speedometer Gauge Controller
    const speedArc = document.getElementById('speed-arc-progress');
    const speedDisplay = document.getElementById('val-speed-display');
    const specSpeed = document.getElementById('spec-val-speed');

    // Total length of semicircle arc path with radius 75: PI * 75 ≈ 235.6
    const TOTAL_ARC_LENGTH = 235.6;

    const setSpeedometer = (speedValue, maxSpeed = 100) => {
        const clamped = Math.max(0, Math.min(speedValue, maxSpeed));
        const fraction = clamped / maxSpeed;
        const offset = TOTAL_ARC_LENGTH * (1 - fraction);

        if (speedArc) {
            speedArc.style.strokeDashoffset = offset;
            // Highlight red if exceeding danger limit (60)
            if (clamped > 60) {
                speedArc.style.stroke = '#EF4444';
            } else {
                speedArc.style.stroke = '#1A68FF';
            }
        }
        if (speedDisplay) speedDisplay.textContent = Math.round(clamped);
        if (specSpeed) specSpeed.textContent = `${Math.round(clamped)} km/h`;

        if (window.geoMap && window.geoMap.setSpeed) {
            window.geoMap.setSpeed(Math.round(clamped));
        }
    };

    // Initial speed set to 48 (as in screenshot)
    setSpeedometer(48);

    // Dynamic subtle fluctuation to simulate real sensor tracking
    setInterval(() => {
        // Fluctuate gently between 45 and 52 km/h
        const jitter = 48 + Math.round((Math.random() - 0.5) * 6);
        setSpeedometer(jitter);
    }, 4000);

    // 3. PWA (Progressive Web App) Install Prompt Handler
    let deferredPrompt = null;
    const btnInstallPwa = document.getElementById('btn-install-pwa');

    window.addEventListener('beforeinstallprompt', (e) => {
        e.preventDefault();
        deferredPrompt = e;
        if (btnInstallPwa) {
            btnInstallPwa.classList.remove('hidden');
        }
    });

    if (btnInstallPwa) {
        btnInstallPwa.addEventListener('click', async () => {
            if (deferredPrompt) {
                deferredPrompt.prompt();
                const { outcome } = await deferredPrompt.userChoice;
                console.log(`[PWA] User response to install prompt: ${outcome}`);
                deferredPrompt = null;
                btnInstallPwa.classList.add('hidden');
            } else {
                alert('Aplikasi sudah siap diinstall melalui menu titik tiga (Chrome/Edge) -> "Install Aplikasi".');
            }
        });
    }

    // 4. Siren Audio & Alert Toggle
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
            } else {
                soundIcon.className = 'fa-solid fa-volume-xmark';
                btnToggleSound.title = 'Sirine Dibisukan';
                if (alarmAudio) {
                    alarmAudio.pause();
                    alarmAudio.currentTime = 0;
                }
            }
        });
    }

    // Dismiss Alert button
    const btnDismissAlert = document.getElementById('btn-dismiss-alert');
    const btnSilenceAlarm = document.getElementById('btn-silence-alarm');
    const alertOverlay = document.getElementById('ews-alert-overlay');

    if (btnDismissAlert && alertOverlay) {
        btnDismissAlert.addEventListener('click', () => {
            alertOverlay.classList.add('hidden');
            if (alarmAudio) alarmAudio.pause();
        });
    }
    if (btnSilenceAlarm && alarmAudio) {
        btnSilenceAlarm.addEventListener('click', () => {
            alarmAudio.pause();
            alarmAudio.currentTime = 0;
        });
    }

    // Helper Toast Notification
    window.showToast = (msg, type = 'info') => {
        const toast = document.createElement('div');
        toast.className = `custom-toast toast-${type}`;
        toast.style.cssText = `
            position: fixed;
            bottom: 24px;
            right: 24px;
            background: #0F172A;
            color: #FFFFFF;
            padding: 12px 20px;
            border-radius: 12px;
            box-shadow: 0 10px 25px rgba(0,0,0,0.25);
            font-size: 13px;
            font-weight: 600;
            display: flex;
            align-items: center;
            gap: 10px;
            z-index: 99999;
            animation: slideUp 0.3s ease-out;
            border-left: 4px solid #10B981;
        `;
        toast.innerHTML = `<i class="fa-solid fa-circle-check text-success"></i> ${msg}`;
        document.body.appendChild(toast);
        setTimeout(() => {
            toast.style.opacity = '0';
            toast.style.transition = 'opacity 0.5s ease';
            setTimeout(() => toast.remove(), 500);
        }, 3500);
    };

    // 5. Sidebar Nav Links Smooth Handling
    const sidebarLinks = document.querySelectorAll('.sidebar-menu .menu-link');
    sidebarLinks.forEach(link => {
        link.addEventListener('click', (e) => {
            if (link.id === 'sidebar-nav-bluetooth') return; // Handled by provisioning
            sidebarLinks.forEach(l => l.classList.remove('active'));
            link.classList.add('active');
        });
    });
});
