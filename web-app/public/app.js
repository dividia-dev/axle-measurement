/**
 * Axle Measurement — Frontend Application
 *
 * Handles:
 * - Login/logout
 * - Video display (test video in dev, WebRTC via go2rtc in production)
 * - Canvas overlay with two movable vertical measurement lines
 * - Keyboard input from USB HID controller (or regular keyboard)
 * - Distance calculation and display
 * - Calibration workflow
 * - Measurement history
 */

// === State ===
const state = {
    user: null,
    calibration: null,
    line1_x: 0.3,  // Normalized position (0-1) relative to video
    line2_x: 0.7,
    videoRect: null, // Actual video display rect within the container
    isCalibrating: false,
    axleCount: 5,
    isSpecialVehicle: false,
    scaleWeight: null,  // From scale integration (future)
    weightResult: null,
    fineMode: false,    // Toggle: false = coarse, true = fine
};

// Movement speeds (in normalized units per keystroke)
const COARSE_STEP = 0.005;  // ~5 pixels at 1080p
const FINE_STEP = 0.001;    // ~1 pixel at 1080p

// === DOM Elements ===
const $ = (id) => document.getElementById(id);
const loginScreen = $('login-screen');
const appScreen = $('app-screen');
const loginForm = $('login-form');
const loginError = $('login-error');
const video = $('camera-feed');
const overlay = $('overlay');
const measurementDisplay = $('measurement-display');
const measurementValue = $('measurement-value');
const calStatus = $('cal-indicator');
const btnSave = $('btn-save-measurement');
const btnReset = $('btn-reset-lines');
const btnCalibrate = $('btn-calibrate');
const btnSettings = $('btn-settings');
const btnHistory = $('btn-history');
const btnLogout = $('btn-logout');
const calModal = $('calibration-modal');
const historyModal = $('history-modal');
const weightPanel = $('weight-panel');
const axleCountDisplay = $('axle-count-display');
const weightMax = $('weight-max');
const weightScale = $('weight-scale');
const weightStatus = $('weight-status');
const chkSpecial = $('chk-special');

// === API Helpers ===
async function api(method, path, body) {
    const opts = {
        method,
        headers: { 'Content-Type': 'application/json' },
        credentials: 'same-origin'
    };
    if (body) opts.body = JSON.stringify(body);
    const res = await fetch('/api' + path, opts);
    const data = await res.json();
    if (!res.ok) throw new Error(data.error || `HTTP ${res.status}`);
    return data;
}

// === Auth ===
async function login(username, password) {
    const data = await api('POST', '/auth/login', { username, password });
    state.user = data.user;
    showApp();
}

async function logout() {
    await api('POST', '/auth/logout');
    state.user = null;
    showLogin();
}

async function checkAuth() {
    try {
        const data = await api('GET', '/auth/me');
        state.user = data.user;
        showApp();
    } catch {
        showLogin();
    }
}

function showLogin() {
    loginScreen.hidden = false;
    appScreen.hidden = true;
}

function showApp() {
    loginScreen.hidden = true;
    appScreen.hidden = false;
    $('user-info').textContent = `${state.user.username} (${state.user.role})`;

    // Show admin controls
    const isAdmin = state.user.role === 'admin';
    btnCalibrate.hidden = !isAdmin;
    if (btnSettings) btnSettings.hidden = !isAdmin;

    loadCalibration();
    initVideo();
    resizeOverlay();
}

// === Calibration ===
async function loadCalibration() {
    try {
        const data = await api('GET', '/calibration');
        state.calibration = data.calibration;
        updateCalStatus();
    } catch {
        state.calibration = null;
        updateCalStatus();
    }
}

function updateCalStatus() {
    if (state.calibration) {
        calStatus.textContent = `CALIBRATED (${state.calibration.pixels_per_inch.toFixed(2)} px/in)`;
        calStatus.className = 'calibrated';
        btnSave.disabled = false;
        measurementDisplay.hidden = false;
        weightPanel.hidden = false;
        updateWeightCheck();
    } else {
        calStatus.textContent = 'NOT CALIBRATED';
        calStatus.className = 'not-calibrated';
        btnSave.disabled = true;
        measurementDisplay.hidden = true;
        weightPanel.hidden = true;
    }
}

function openCalibration() {
    state.isCalibrating = true;
    calModal.hidden = false;
    updateCalModal();
}

function closeCalibration() {
    state.isCalibrating = false;
    calModal.hidden = true;
}

function updateCalModal() {
    if (!calModal.hidden) {
        const rect = getVideoRect();
        const l1px = Math.round(state.line1_x * rect.width);
        const l2px = Math.round(state.line2_x * rect.width);
        $('cal-pixel-dist').textContent = Math.abs(l2px - l1px);
    }
}

async function saveCalibration() {
    const feetVal = parseInt($('cal-feet').value) || 0;
    const inchesVal = parseInt($('cal-inches').value) || 0;
    const totalInches = feetVal * 12 + inchesVal;

    if (totalInches <= 0) {
        $('cal-error').textContent = 'Enter a valid distance (feet and/or inches)';
        $('cal-error').hidden = false;
        return;
    }

    const rect = getVideoRect();
    const ref1_px = Math.round(state.line1_x * rect.width);
    const ref2_px = Math.round(state.line2_x * rect.width);

    try {
        const data = await api('POST', '/calibration', {
            ref1_px, ref2_px,
            known_distance_inches: totalInches
        });
        state.calibration = data.calibration;
        updateCalStatus();
        closeCalibration();
    } catch (err) {
        $('cal-error').textContent = err.message;
        $('cal-error').hidden = false;
    }
}

// === Mode Indicator ===
function updateModeIndicator() {
    const indicator = $('mode-indicator');
    if (state.fineMode) {
        indicator.textContent = 'FINE';
        indicator.className = 'mode-fine';
    } else {
        indicator.textContent = 'COARSE';
        indicator.className = 'mode-coarse';
    }
}

// === Weight Check (Bridge Formula) ===
function updateAxleCount(delta) {
    state.axleCount = Math.max(2, Math.min(9, state.axleCount + delta));
    axleCountDisplay.textContent = state.axleCount;
    updateWeightCheck();
}

async function updateWeightCheck() {
    if (!state.calibration) return;

    const rect = getVideoRect();
    const pixelDist = Math.abs(state.line2_x - state.line1_x) * rect.width;
    const distInches = pixelDist / state.calibration.pixels_per_inch;
    const distFeet = distInches / 12;

    try {
        const data = await api('POST', '/weight-check', {
            distanceFeet: distFeet,
            axleCount: state.axleCount,
            scaleWeight: state.scaleWeight,
            isSpecialVehicle: state.isSpecialVehicle,
        });

        state.weightResult = data.analysis;
        renderWeightResult(data.analysis);
    } catch (err) {
        weightMax.textContent = '--';
    }
}

function renderWeightResult(analysis) {
    weightMax.textContent = analysis.maxAllowable.toLocaleString() + ' lbs';

    if (analysis.scaleWeight != null) {
        $('weight-scale-line').hidden = false;
        weightScale.textContent = analysis.scaleWeight.toLocaleString() + ' lbs';
        weightStatus.hidden = false;

        if (analysis.isLegal) {
            weightStatus.className = 'legal';
            weightStatus.textContent = `LEGAL (${analysis.remainingCapacity.toLocaleString()} lbs remaining)`;
        } else {
            weightStatus.className = 'overweight';
            weightStatus.textContent = `OVERWEIGHT by ${analysis.overweightBy.toLocaleString()} lbs`;
        }
    } else {
        $('weight-scale-line').hidden = true;
        weightStatus.hidden = true;
    }
}

// Debounce weight checks during line movement
let weightCheckTimeout = null;
function debouncedWeightCheck() {
    if (weightCheckTimeout) clearTimeout(weightCheckTimeout);
    weightCheckTimeout = setTimeout(updateWeightCheck, 300);
}

// === Video ===
function initVideo() {
    // In dev mode, load the test video
    // In production, this would connect to go2rtc via WebRTC
    if (!video.src && !video.srcObject) {
        video.src = 'test-video.mp4';
        video.onerror = () => {
            console.log('No test video found. Run: npm run download-test-video');
        };
    }
}

function connectWebRTC(streamName) {
    // Production: connect to go2rtc
    const pc = new RTCPeerConnection({ iceServers: [] });
    pc.addTransceiver('video', { direction: 'recvonly' });
    pc.ontrack = (event) => {
        video.srcObject = event.streams[0];
    };

    pc.createOffer().then(offer => {
        pc.setLocalDescription(offer);
        return fetch(`/go2rtc/api/webrtc?src=${streamName}`, {
            method: 'POST',
            body: offer.sdp
        });
    }).then(res => res.text()).then(answer => {
        pc.setRemoteDescription({ type: 'answer', sdp: answer });
    }).catch(err => {
        console.error('WebRTC connection failed:', err);
    });
}

// === Canvas Overlay ===
function getVideoRect() {
    // Calculate the actual displayed video rectangle within the container
    const container = $('video-container');
    const cw = container.clientWidth;
    const ch = container.clientHeight;

    // If video has natural dimensions, calculate letterbox/pillarbox
    const vw = video.videoWidth || cw;
    const vh = video.videoHeight || ch;

    const containerRatio = cw / ch;
    const videoRatio = vw / vh;

    let displayW, displayH, offsetX, offsetY;

    if (videoRatio > containerRatio) {
        // Video is wider — pillarbox (black bars top/bottom)
        displayW = cw;
        displayH = cw / videoRatio;
        offsetX = 0;
        offsetY = (ch - displayH) / 2;
    } else {
        // Video is taller — letterbox (black bars left/right)
        displayH = ch;
        displayW = ch * videoRatio;
        offsetX = (cw - displayW) / 2;
        offsetY = 0;
    }

    return { x: offsetX, y: offsetY, width: displayW, height: displayH };
}

function resizeOverlay() {
    const container = $('video-container');
    overlay.width = container.clientWidth;
    overlay.height = container.clientHeight;
    drawOverlay();
}

function drawLine(ctx, x, y1, y2, color) {
    // Black border for contrast on both light and dark backgrounds
    ctx.strokeStyle = '#000000';
    ctx.lineWidth = 4;
    ctx.setLineDash([]);
    ctx.beginPath();
    ctx.moveTo(x, y1);
    ctx.lineTo(x, y2);
    ctx.stroke();

    // Colored line on top
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(x, y1);
    ctx.lineTo(x, y2);
    ctx.stroke();
}

function drawOverlay() {
    const ctx = overlay.getContext('2d');
    ctx.clearRect(0, 0, overlay.width, overlay.height);

    const rect = getVideoRect();
    const inCalMode = state.isCalibrating;
    const lineColor = inCalMode ? 'tomato' : '#0088FF';

    // Convert normalized positions to pixel positions within the video rect
    const l1x = rect.x + state.line1_x * rect.width;
    const l2x = rect.x + state.line2_x * rect.width;

    // Draw lines with black outline for visibility
    drawLine(ctx, l1x, rect.y, rect.y + rect.height, lineColor);
    drawLine(ctx, l2x, rect.y, rect.y + rect.height, lineColor);

    // Line labels
    const label1 = $('line1-label');
    const label2 = $('line2-label');
    label1.style.left = (l1x + 4) + 'px';
    label1.hidden = false;
    label2.style.left = (l2x + 4) + 'px';
    label2.hidden = false;
    label1.classList.toggle('cal-mode', inCalMode);
    label2.classList.toggle('cal-mode', inCalMode);

    // Calibration mode border on video container
    $('video-container').classList.toggle('cal-mode', inCalMode);

    // Calculate and display measurement
    if (state.calibration) {
        const pixelDist = Math.abs(state.line2_x - state.line1_x) * rect.width;
        const inches = pixelDist / state.calibration.pixels_per_inch;
        const feet = Math.floor(inches / 12);
        const remainInches = Math.round(inches % 12);

        let displayText;
        if (remainInches === 0) {
            displayText = `${feet} ft`;
        } else {
            displayText = `${feet} ft ${remainInches} in`;
        }
        measurementValue.textContent = displayText;
        measurementDisplay.hidden = false;

        // Draw connecting line between measurement lines (dashed)
        const midY = rect.y + 60;
        ctx.strokeStyle = inCalMode ? 'rgba(255, 99, 71, 0.4)' : 'rgba(0, 136, 255, 0.4)';
        ctx.lineWidth = 1;
        ctx.setLineDash([4, 4]);
        ctx.beginPath();
        ctx.moveTo(l1x, midY);
        ctx.lineTo(l2x, midY);
        ctx.stroke();
        ctx.setLineDash([]);
    }

    updateCalModal();
}

// === Keyboard Input ===
function handleKeyDown(e) {
    // Don't capture keys when typing in inputs
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
    // Don't capture keys when modals are open (except calibration — lines still need to move)
    if (!historyModal.hidden) return;

    let handled = true;
    const step = state.fineMode ? FINE_STEP : COARSE_STEP;

    switch (e.key.toLowerCase()) {
        // Joystick 1 — Line 1 (coarse or fine based on toggle)
        case 'a': state.line1_x = Math.max(0, state.line1_x - step); break;
        case 'd': state.line1_x = Math.min(1, state.line1_x + step); break;

        // Joystick 2 — Line 2 (coarse or fine based on toggle)
        case 'j': state.line2_x = Math.max(0, state.line2_x - step); break;
        case 'l': state.line2_x = Math.min(1, state.line2_x + step); break;

        // Fine/Coarse toggle
        case 'f':
            state.fineMode = !state.fineMode;
            updateModeIndicator();
            break;

        // Actions
        case ' ':
            e.preventDefault();
            saveMeasurementAction();
            break;
        case 'r':
            resetLines();
            break;
        case 'escape':
            closeCalibration();
            historyModal.hidden = true;
            break;
        default:
            handled = false;
    }

    if (handled) {
        drawOverlay();
        debouncedWeightCheck();
    }
}

// === Actions ===
function resetLines() {
    state.line1_x = 0.3;
    state.line2_x = 0.7;
    drawOverlay();
    debouncedWeightCheck();
}

async function saveMeasurementAction() {
    if (!state.calibration) return;

    const rect = getVideoRect();
    const l1px = Math.round(state.line1_x * rect.width);
    const l2px = Math.round(state.line2_x * rect.width);

    try {
        const data = await api('POST', '/measurements', {
            line1_px: l1px,
            line2_px: l2px,
            notes: ''
        });

        // Flash the measurement display to confirm save
        measurementDisplay.style.borderColor = '#00ff00';
        setTimeout(() => { measurementDisplay.style.borderColor = '#0088ff'; }, 500);
    } catch (err) {
        console.error('Failed to save measurement:', err);
    }
}

async function showHistory() {
    historyModal.hidden = false;
    try {
        const data = await api('GET', '/measurements?limit=50');
        const tbody = $('history-body');
        tbody.innerHTML = '';

        if (data.measurements.length === 0) {
            tbody.innerHTML = '<tr><td colspan="4" style="text-align:center;color:#666">No measurements yet</td></tr>';
            return;
        }

        for (const m of data.measurements) {
            const tr = document.createElement('tr');
            const date = new Date(m.created_at).toLocaleString();
            tr.innerHTML = `
                <td>${date}</td>
                <td><strong>${m.distance_display}</strong></td>
                <td>${m.measured_by || '-'}</td>
                <td>${m.notes || '-'}</td>
            `;
            tbody.appendChild(tr);
        }
    } catch (err) {
        console.error('Failed to load history:', err);
    }
}

// === Event Listeners ===
loginForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    loginError.hidden = true;
    try {
        await login($('login-username').value, $('login-password').value);
    } catch (err) {
        loginError.textContent = err.message;
        loginError.hidden = false;
    }
});

btnLogout.addEventListener('click', logout);
btnReset.addEventListener('click', resetLines);
btnSave.addEventListener('click', saveMeasurementAction);
btnCalibrate.addEventListener('click', openCalibration);
btnHistory.addEventListener('click', showHistory);
$('btn-history-close').addEventListener('click', () => { historyModal.hidden = true; });
$('btn-cal-save').addEventListener('click', saveCalibration);
$('btn-cal-cancel').addEventListener('click', closeCalibration);

// Weight panel controls
$('btn-axle-minus').addEventListener('click', () => updateAxleCount(-1));
$('btn-axle-plus').addEventListener('click', () => updateAxleCount(1));
chkSpecial.addEventListener('change', () => {
    state.isSpecialVehicle = chkSpecial.checked;
    updateWeightCheck();
});

document.addEventListener('keydown', handleKeyDown);
window.addEventListener('resize', resizeOverlay);

// Video metadata loaded — we know dimensions, resize overlay
video.addEventListener('loadedmetadata', resizeOverlay);
video.addEventListener('playing', resizeOverlay);

// === Init ===
checkAuth();
