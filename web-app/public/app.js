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

// Controller settings (loaded from API, these are defaults)
let controllerSettings = {
    keymap: {
        line1_left: 'a', line1_right: 'd',
        line2_left: 'j', line2_right: 'l',
        toggle_mode: 'f', save: ' ', reset: 'r'
    },
    sensitivity: { coarse_step: 0.005, fine_step: 0.001 }
};

// Reverse map: key → action (rebuilt when settings change)
let keyActionMap = new Map();

// Live video (WSPlayer) state
let wsPlayer = null;
let nvrAuth = null; // { token, devices, cameras, host, port, secure }

function buildKeyActionMap() {
    keyActionMap.clear();
    for (const [action, key] of Object.entries(controllerSettings.keymap)) {
        keyActionMap.set(key.toLowerCase(), action);
    }
}

// Key capture state for settings modal
let captureTarget = null; // action name being rebound, or null

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

// === In-app Dialog (replaces confirm/alert to stay in fullscreen) ===
function appConfirm(msg) {
    return new Promise(resolve => {
        const dialog = $('app-dialog');
        $('app-dialog-msg').textContent = msg;
        $('app-dialog-cancel').hidden = false;
        dialog.hidden = false;
        const cleanup = (result) => {
            dialog.hidden = true;
            $('app-dialog-ok').removeEventListener('click', onOk);
            $('app-dialog-cancel').removeEventListener('click', onCancel);
            resolve(result);
        };
        const onOk = () => cleanup(true);
        const onCancel = () => cleanup(false);
        $('app-dialog-ok').addEventListener('click', onOk);
        $('app-dialog-cancel').addEventListener('click', onCancel);
        $('app-dialog-ok').focus();
    });
}

function appAlert(msg) {
    return new Promise(resolve => {
        const dialog = $('app-dialog');
        $('app-dialog-msg').textContent = msg;
        $('app-dialog-cancel').hidden = true;
        dialog.hidden = false;
        const onOk = () => {
            dialog.hidden = true;
            $('app-dialog-ok').removeEventListener('click', onOk);
            resolve();
        };
        $('app-dialog-ok').addEventListener('click', onOk);
        $('app-dialog-ok').focus();
    });
}

// === Mobile Keyboard for Key Capture ===
const mobileKeyInput = $('mobile-key-input');
const hasTouchScreen = ('ontouchstart' in window) || (navigator.maxTouchPoints > 0);

mobileKeyInput.addEventListener('keydown', (e) => {
    if (captureTarget) {
        e.preventDefault();
        const key = e.key === ' ' ? ' ' : e.key;
        if (key.length === 1 || ['Tab', 'Enter', 'Backspace', 'Delete', 'Escape'].includes(key)) {
            completeKeyCapture(key);
        }
        mobileKeyInput.value = '';
        mobileKeyInput.blur();
    }
});

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
    $('btn-settings').hidden = !isAdmin;

    loadControllerSettings();
    loadCalibration();
    initVideo();
    resizeOverlay();
    startClock();
}

// === Clock ===
function startClock() {
    const clockEl = $('panel-clock');
    function update() {
        const now = new Date();
        const date = now.toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: 'numeric' });
        const time = now.toLocaleTimeString(undefined, { hour: '2-digit', minute: '2-digit', second: '2-digit' });
        clockEl.innerHTML = `<div class="clock-date">${date}</div><div class="clock-time">${time}</div>`;
    }
    update();
    setInterval(update, 1000);
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
    if (state.calibration && state.calibration.inches_per_norm) {
        calStatus.textContent = `CALIBRATED`;
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
    // Save current line positions so we can restore on cancel
    state._preCalLine1 = state.line1_x;
    state._preCalLine2 = state.line2_x;

    // Restore calibration reference positions and pre-fill distance
    if (state.calibration && state.calibration.ref1_norm != null) {
        state.line1_x = state.calibration.ref1_norm;
        state.line2_x = state.calibration.ref2_norm;
        const totalInches = state.calibration.known_distance_inches;
        if (totalInches > 0) {
            const ft = Math.floor(totalInches / 12);
            const inches = totalInches % 12;
            $('cal-feet').value = ft > 0 ? ft : '';
            $('cal-inches').value = inches > 0 ? inches : '';
        }
    }

    state.isCalibrating = true;
    calModal.hidden = false;
    $('cal-error').hidden = true;
    updateCalModal();
    drawOverlay();
}

function closeCalibration() {
    // Restore line positions from before calibration
    if (state._preCalLine1 != null) {
        state.line1_x = state._preCalLine1;
        state.line2_x = state._preCalLine2;
        state._preCalLine1 = null;
        state._preCalLine2 = null;
    }
    state.isCalibrating = false;
    calModal.hidden = true;
    drawOverlay();
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
    const feetInput = $('cal-feet').value.trim();
    const inchesInput = $('cal-inches').value.trim();
    const feetVal = feetInput !== '' ? parseInt(feetInput) : 0;
    const inchesVal = inchesInput !== '' ? parseInt(inchesInput) : 0;
    const totalInches = feetVal * 12 + inchesVal;

    if (totalInches <= 0) {
        $('cal-error').textContent = 'Enter a distance (feet and/or inches)';
        $('cal-error').hidden = false;
        return;
    }

    // Store normalized positions (0-1), not pixel positions
    // This makes calibration independent of window/video size
    const ref1_norm = state.line1_x;
    const ref2_norm = state.line2_x;
    const normDist = Math.abs(ref2_norm - ref1_norm);

    if (normDist < 0.01) {
        $('cal-error').textContent = 'Lines are too close together. Spread them apart.';
        $('cal-error').hidden = false;
        return;
    }

    // inches_per_norm = how many inches one normalized unit represents
    const inches_per_norm = totalInches / normDist;

    try {
        const data = await api('POST', '/calibration', {
            ref1_norm, ref2_norm,
            known_distance_inches: totalInches,
            inches_per_norm
        });
        state.calibration = data.calibration;
        updateCalStatus();
        closeCalibration();
    } catch (err) {
        $('cal-error').textContent = err.message;
        $('cal-error').hidden = false;
    }
}

// === Mode Toggle ===
function toggleMode() {
    state.fineMode = !state.fineMode;
    updateModeIndicator();
}

function updateModeIndicator() {
    const indicator = $('mode-indicator');
    if (state.fineMode) {
        indicator.textContent = 'FINE';
        indicator.className = 'panel-btn mode-fine';
    } else {
        indicator.textContent = 'COARSE';
        indicator.className = 'panel-btn mode-coarse';
    }
}

// === Pointer Drag for Lines (unified mouse + touch + pen) ===
const DRAG_HIT_ZONE = 15; // pixels from line center to grab (mouse/pen)
const TOUCH_HIT_ZONE = 30; // wider zone for touch (fingers are bigger)
const FINE_DRAG_RATIO = 0.25; // In fine mode, line moves 1/4 of pointer distance

let dragState = {
    active: false,
    line: null,       // 1 or 2
    startMouseX: 0,   // pointer X at drag start (pixels)
    startLineNorm: 0, // line normalized position at drag start
    pointerId: null,  // for pointer capture
};

function getLinePixelX(lineNorm) {
    const rect = getVideoRect();
    return rect.x + lineNorm * rect.width;
}

function pixelToNorm(pixelX) {
    const rect = getVideoRect();
    return Math.max(0, Math.min(1, (pixelX - rect.x) / rect.width));
}

function getHoveredLine(mouseX, isTouch) {
    const hitZone = isTouch ? TOUCH_HIT_ZONE : DRAG_HIT_ZONE;
    const l1x = getLinePixelX(state.line1_x);
    const l2x = getLinePixelX(state.line2_x);
    const d1 = Math.abs(mouseX - l1x);
    const d2 = Math.abs(mouseX - l2x);

    if (d1 <= hitZone && d1 <= d2) return 1;
    if (d2 <= hitZone) return 2;
    return null;
}

function onOverlayPointerDown(e) {
    const isTouch = (e.pointerType === 'touch');
    const line = getHoveredLine(e.offsetX, isTouch);
    if (line) {
        dragState.active = true;
        dragState.line = line;
        dragState.startMouseX = e.offsetX;
        dragState.startLineNorm = line === 1 ? state.line1_x : state.line2_x;
        dragState.pointerId = e.pointerId;
        overlay.setPointerCapture(e.pointerId);
        $('video-container').classList.add('dragging');
        e.preventDefault();
    }
}

function onOverlayPointerMove(e) {
    if (dragState.active) {
        const rect = getVideoRect();
        const deltaPx = e.offsetX - dragState.startMouseX;
        const deltaNorm = deltaPx / rect.width;
        const ratio = state.fineMode ? FINE_DRAG_RATIO : 1.0;
        const newNorm = Math.max(0, Math.min(1, dragState.startLineNorm + deltaNorm * ratio));

        if (dragState.line === 1) {
            state.line1_x = newNorm;
        } else {
            state.line2_x = newNorm;
        }
        drawOverlay();
        debouncedWeightCheck();
        e.preventDefault();
    } else {
        // Show grab cursor when hovering near a line (mouse only)
        if (e.pointerType === 'mouse') {
            const line = getHoveredLine(e.offsetX, false);
            const container = $('video-container');
            if (line) {
                container.classList.add('drag-hover');
            } else {
                container.classList.remove('drag-hover');
            }
        }
    }
}

function onOverlayPointerUp(e) {
    if (dragState.active) {
        if (dragState.pointerId != null) {
            overlay.releasePointerCapture(dragState.pointerId);
        }
        dragState.active = false;
        dragState.line = null;
        dragState.pointerId = null;
        $('video-container').classList.remove('dragging');
    }
}

function showWeightInfo() {
    if (!state.calibration || !state.calibration.inches_per_norm) {
        appAlert('Calibrate first to see weight calculations.');
        return;
    }
    const normDist = Math.abs(state.line2_x - state.line1_x);
    const distInches = normDist * state.calibration.inches_per_norm;
    const L = distInches / 12;
    const N = state.axleCount;

    let formulaWeight, maxAllowable, capReason;
    if (N === 1) {
        formulaWeight = 20000; maxAllowable = 20000; capReason = 'Single axle limit';
    } else {
        const raw = 500 * ((L * N) / (N - 1) + 12 * N + 36);
        formulaWeight = Math.round(raw / 500) * 500;
        maxAllowable = formulaWeight; capReason = 'Bridge Formula B';
        if (N === 2 && formulaWeight > 34000) { maxAllowable = 34000; capReason = 'Tandem axle cap (34,000 lbs)'; }
        if (maxAllowable > 80000) { maxAllowable = 80000; capReason = 'Federal GVW cap (80,000 lbs)'; }
        if (state.isSpecialVehicle && maxAllowable > 70000) { maxAllowable = 70000; capReason = 'FL special vehicle cap (70,000 lbs)'; }
    }
    appAlert(
        'Federal Bridge Formula B (23 USC 127)\n══════════════════════════════════\n\n' +
        'W = 500 x ((L x N) / (N - 1) + 12N + 36)\n\n' +
        'Where:\n' +
        `  L = ${L.toFixed(1)} ft (measured axle spacing)\n` +
        `  N = ${N} (axle count)\n\n` +
        'Calculation:\n' +
        `  W = 500 x ((${L.toFixed(1)} x ${N}) / (${N} - 1) + 12 x ${N} + 36)\n` +
        `  W = ${formulaWeight.toLocaleString()} lbs (formula result)\n\n` +
        `Max Allowable: ${maxAllowable.toLocaleString()} lbs\n` +
        `Applied Cap: ${capReason}\n\n` +
        'Caps (in order of precedence):\n' +
        '  - Single axle: 20,000 lbs\n' +
        '  - Tandem axle: 34,000 lbs\n' +
        '  - Federal GVW: 80,000 lbs\n' +
        '  - FL Dump/Mix:  70,000 lbs'
    );
}

// === Weight Check (Bridge Formula) ===
function updateAxleCount(delta) {
    state.axleCount = Math.max(2, Math.min(9, state.axleCount + delta));
    axleCountDisplay.textContent = state.axleCount;
    updateWeightCheck();
}

async function updateWeightCheck() {
    if (!state.calibration || !state.calibration.inches_per_norm) return;

    const normDist = Math.abs(state.line2_x - state.line1_x);
    const distInches = normDist * state.calibration.inches_per_norm;
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
function getVideoSourceConfig() {
    try {
        return JSON.parse(localStorage.getItem('axle_video_source')) || { type: 'test' };
    } catch { return { type: 'test' }; }
}

function saveVideoSourceConfig(cfg) {
    localStorage.setItem('axle_video_source', JSON.stringify(cfg));
}

function initVideo() {
    // Always start with test video — NVR requires login each session
    if (!video.src && !video.srcObject) {
        video.src = 'test-video.mp4';
        video.onerror = () => {
            console.log('No test video found. Run: npm run download-test-video');
        };
    }
}

async function loginNVR() {
    const connType = $('nvr-conn-type').value;
    const apiUrl = $('nvr-api-url').value.trim();
    const username = $('nvr-username').value.trim();
    const password = $('nvr-password').value;
    const statusEl = $('nvr-status');

    if (!apiUrl || !username || !password) {
        statusEl.innerHTML = '<span style="color:#c62828">URL, username, and password required</span>';
        return;
    }

    statusEl.textContent = 'Authenticating...';
    $('btn-nvr-login').disabled = true;

    try {
        if (connType === 'cloud') {
            await loginCloud(apiUrl, username, password, statusEl);
        } else {
            await loginLocal(apiUrl, username, password, statusEl);
        }
    } catch (err) {
        statusEl.innerHTML = '<span style="color:#c62828">' + err.message + '</span>';
    } finally {
        $('btn-nvr-login').disabled = false;
    }
}

async function loginCloud(apiUrl, username, password, statusEl) {
    const res = await fetch('/api/nvr/auth', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ apiUrl, username, password })
    });
    const data = await res.json();
    if (!res.ok || data.error) throw new Error(data.error || `Auth failed (${res.status})`);

    const apiHost = apiUrl.replace(/^https?:\/\//, '');
    const wsProto = apiUrl.startsWith('https') ? 'wss' : 'ws';
    nvrAuth = {
        mode: 'cloud', token: data.token,
        devices: data.devices || [], cameras: data.cameras || [],
        apiUrl, apiHost, wsProto
    };

    saveVideoSourceConfig({ type: 'nvr', connType: 'cloud', apiUrl, username });
    populateCameraDropdown();
}

async function loginLocal(nvrUrl, username, password, statusEl) {
    const res = await fetch('/api/nvr/auth-local', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ nvrUrl, username, password })
    });
    const data = await res.json();
    if (!res.ok || data.error) throw new Error(data.error || `Auth failed (${res.status})`);

    const nvrHost = nvrUrl.replace(/^https?:\/\//, '');
    const wsProto = nvrUrl.startsWith('https') ? 'wss' : 'ws';
    nvrAuth = {
        mode: 'local', session: data.session, serial: data.serial,
        cameras: data.cameras || [],
        nvrUrl, nvrHost, wsProto
    };

    saveVideoSourceConfig({ type: 'nvr', connType: 'local', apiUrl: nvrUrl, username });
    populateCameraDropdown();
}

function populateCameraDropdown() {
    const camSelect = $('nvr-camera-select');
    camSelect.innerHTML = '';

    if (nvrAuth.mode === 'cloud') {
        const uniqueCams = [];
        const seen = new Set();
        for (const cam of nvrAuth.cameras) {
            const key = `${cam.sDeviceID}-${cam.bCamera}`;
            if (!seen.has(key)) { seen.add(key); uniqueCams.push(cam); }
        }
        for (const cam of uniqueCams) {
            const dev = (nvrAuth.devices || []).find(d => d.sDeviceID === cam.sDeviceID);
            const devName = dev ? dev.sName : cam.sDeviceID;
            const label = `${devName} — Cam ${cam.bCamera}${cam.sName ? ' (' + cam.sName + ')' : ''}`;
            const opt = document.createElement('option');
            opt.value = JSON.stringify({ deviceId: cam.sDeviceID, camera: cam.bCamera });
            opt.textContent = label;
            camSelect.appendChild(opt);
        }
    } else {
        const cams = Array.isArray(nvrAuth.cameras) ? nvrAuth.cameras : [];
        for (let i = 0; i < cams.length; i++) {
            const cam = cams[i];
            const camNum = cam.bCamera || cam.bCameraNum || (i + 1);
            const label = cam.sName || `Camera ${camNum}`;
            const opt = document.createElement('option');
            opt.value = JSON.stringify({ camera: camNum });
            opt.textContent = label;
            camSelect.appendChild(opt);
        }
    }

    const statusEl = $('nvr-status');
    const count = camSelect.options.length;
    $('nvr-camera-select-group').hidden = false;
    $('btn-nvr-login').hidden = true;
    $('btn-nvr-disconnect').hidden = false;
    statusEl.innerHTML = `<span style="color:#4caf50">Logged in — ${count} camera(s)</span>`;

    if (count > 0) connectNVRCamera();
    camSelect.onchange = connectNVRCamera;
    $('nvr-profile').onchange = connectNVRCamera;
}

function connectNVRCamera() {
    if (!nvrAuth) return;
    disconnectNVRStream();

    const selected = JSON.parse($('nvr-camera-select').value);
    const profile = parseInt($('nvr-profile').value) || 2;
    const statusEl = $('nvr-status');

    video.style.display = 'none';

    let wrap = $('ws-player-wrap');
    if (!wrap) {
        wrap = document.createElement('div');
        wrap.id = 'ws-player-wrap';
        $('video-container').insertBefore(wrap, overlay);
    }
    wrap.innerHTML = '';

    let wsUrl;
    if (nvrAuth.mode === 'cloud') {
        wsUrl = `${nvrAuth.wsProto}://${nvrAuth.apiHost}/${selected.deviceId}-cam${selected.camera}-pro${profile}?token=${encodeURIComponent(nvrAuth.token)}`;
    } else {
        wsUrl = `${nvrAuth.wsProto}://${nvrAuth.nvrHost}/ws/cam${selected.camera}-pro${profile}?sess=${nvrAuth.session}`;
    }

    wsPlayer = WSPlayer.create(wrap, {
        url: wsUrl,
        camera: [selected.camera, `Camera ${selected.camera}`],
        noOverlay: ['cam_name'],
        autoReconnect: true,
        onConnect: () => {
            if (statusEl) statusEl.textContent = 'Connected, buffering...';
        },
        onFirstFrame: () => {
            if (statusEl) statusEl.innerHTML = '<span style="color:#4caf50">Streaming</span>';
            resizeOverlay();
        },
        onDisconnect: () => {
            if (statusEl) statusEl.innerHTML = '<span style="color:#c62828">Disconnected</span>';
        },
        onError: (err) => {
            if (statusEl) statusEl.innerHTML = '<span style="color:#c62828">Error: ' + (err.message || err) + '</span>';
        },
        onReconnecting: () => {
            if (statusEl) statusEl.textContent = 'Reconnecting...';
        }
    });
}

function disconnectNVRStream() {
    if (wsPlayer) {
        wsPlayer.stop();
        wsPlayer = null;
    }
    const wrap = $('ws-player-wrap');
    if (wrap) wrap.innerHTML = '';
}

function disconnectNVR() {
    disconnectNVRStream();
    nvrAuth = null;

    video.style.display = '';

    $('btn-nvr-login').hidden = false;
    $('btn-nvr-login').disabled = false;
    $('btn-nvr-disconnect').hidden = true;
    $('nvr-camera-select-group').hidden = true;

    const statusEl = $('nvr-status');
    if (statusEl) statusEl.textContent = '';
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

    // Get native video dimensions from either <video> or WSPlayer canvas
    let vw, vh;
    if (wsPlayer) {
        const wsCanvas = $('ws-player-wrap')?.querySelector('canvas');
        vw = (wsCanvas && wsCanvas.width) || cw;
        vh = (wsCanvas && wsCanvas.height) || ch;
    } else {
        vw = video.videoWidth || cw;
        vh = video.videoHeight || ch;
    }

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

function drawDragHandle(ctx, x, y, color) {
    const size = 8;
    // Black outline
    ctx.fillStyle = '#000000';
    ctx.beginPath();
    ctx.moveTo(x, y - size - 1);
    ctx.lineTo(x + size + 1, y);
    ctx.lineTo(x, y + size + 1);
    ctx.lineTo(x - size - 1, y);
    ctx.closePath();
    ctx.fill();
    // Colored fill
    ctx.fillStyle = color;
    ctx.beginPath();
    ctx.moveTo(x, y - size);
    ctx.lineTo(x + size, y);
    ctx.lineTo(x, y + size);
    ctx.lineTo(x - size, y);
    ctx.closePath();
    ctx.fill();
    // Left/right arrows inside
    ctx.fillStyle = 'rgba(0,0,0,0.5)';
    ctx.font = '10px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText('\u25C0\u25B6', x, y);
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

    // Draw drag handles (small diamond shapes at center of each line)
    const handleY = rect.y + rect.height / 2;
    drawDragHandle(ctx, l1x, handleY, lineColor);
    drawDragHandle(ctx, l2x, handleY, lineColor);

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

    // Calculate and display measurement using normalized coordinates
    // This is window-size independent — same measurement regardless of resize
    if (state.calibration && state.calibration.inches_per_norm) {
        const normDist = Math.abs(state.line2_x - state.line1_x);
        const inches = normDist * state.calibration.inches_per_norm;
        const totalRounded = Math.round(inches);
        const feet = Math.floor(totalRounded / 12);
        const remainInches = totalRounded % 12;

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

    const settingsModal = $('settings-modal');

    // Key capture mode for settings
    if (captureTarget) {
        e.preventDefault();
        if (e.key === 'Escape') {
            cancelKeyCapture();
            return;
        }
        completeKeyCapture(e.key);
        return;
    }

    // Controller test: light up indicators when test tab is visible
    if (settingsModal && !settingsModal.hidden) {
        handleControllerTest(e.key, true);
        return; // Don't process normal actions while settings is open
    }

    // Don't capture keys when modals are open (except calibration — lines still need to move)
    if (!historyModal.hidden) return;

    const key = e.key.toLowerCase();
    const action = keyActionMap.get(key);

    // Escape always works regardless of bindings
    if (key === 'escape') {
        closeCalibration();
        historyModal.hidden = true;
        return;
    }

    if (!action) return;

    const step = state.fineMode
        ? controllerSettings.sensitivity.fine_step
        : controllerSettings.sensitivity.coarse_step;

    switch (action) {
        case 'line1_left':  state.line1_x = Math.max(0, state.line1_x - step); break;
        case 'line1_right': state.line1_x = Math.min(1, state.line1_x + step); break;
        case 'line2_left':  state.line2_x = Math.max(0, state.line2_x - step); break;
        case 'line2_right': state.line2_x = Math.min(1, state.line2_x + step); break;
        case 'toggle_mode': toggleMode(); break;
        case 'save':        e.preventDefault(); saveMeasurementAction(); break;
        case 'reset':       resetLines(); break;
        default: return;
    }

    drawOverlay();
    debouncedWeightCheck();
}

function handleKeyUp(e) {
    const settingsModal = $('settings-modal');
    if (settingsModal && !settingsModal.hidden) {
        handleControllerTest(e.key, false);
    }
}

// === Actions ===
function resetLines() {
    state.line1_x = 0.3;
    state.line2_x = 0.7;
    drawOverlay();
    debouncedWeightCheck();
}

function captureScreenshot() {
    // Capture the video frame cropped to just the video area (no black bars)
    // with measurement lines drawn on top (no handles)
    const rect = getVideoRect();
    const composite = document.createElement('canvas');

    // Get source dimensions and drawable element from either <video> or WSPlayer
    let srcEl, srcW, srcH;
    if (wsPlayer) {
        srcEl = $('ws-player-wrap')?.querySelector('canvas');
        srcW = (srcEl && srcEl.width) || rect.width;
        srcH = (srcEl && srcEl.height) || rect.height;
    } else {
        srcEl = video;
        srcW = video.videoWidth || rect.width;
        srcH = video.videoHeight || rect.height;
    }

    composite.width = srcW;
    composite.height = srcH;
    const ctx = composite.getContext('2d');

    // Draw the video frame
    if (srcEl) ctx.drawImage(srcEl, 0, 0, composite.width, composite.height);

    // Draw lines (no handles) — scale normalized positions to video resolution
    const l1x = state.line1_x * composite.width;
    const l2x = state.line2_x * composite.width;

    // Black outline
    ctx.strokeStyle = '#000000';
    ctx.lineWidth = 4;
    ctx.beginPath();
    ctx.moveTo(l1x, 0); ctx.lineTo(l1x, composite.height);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(l2x, 0); ctx.lineTo(l2x, composite.height);
    ctx.stroke();

    // Blue lines
    ctx.strokeStyle = '#0088FF';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(l1x, 0); ctx.lineTo(l1x, composite.height);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(l2x, 0); ctx.lineTo(l2x, composite.height);
    ctx.stroke();

    // Overlay measurement data in upper-right
    const now = new Date();
    const dateStr = now.toLocaleDateString() + ' ' + now.toLocaleTimeString();
    const normDist = Math.abs(state.line2_x - state.line1_x);
    const inches = normDist * state.calibration.inches_per_norm;
    const totalRounded = Math.round(inches);
    const ft = Math.floor(totalRounded / 12);
    const remainIn = totalRounded % 12;
    const distStr = remainIn === 0 ? `${ft} ft` : `${ft} ft ${remainIn} in`;
    const maxWt = state.weightResult ? state.weightResult.maxAllowable.toLocaleString() + ' lbs' : '--';
    const axleStr = state.axleCount + ' axles' + (state.isSpecialVehicle ? ' (Dump/Mix)' : '');

    const lines = [dateStr, distStr, axleStr, 'Max: ' + maxWt];
    const fontSize = Math.max(14, Math.round(composite.height / 30));
    const lineHeight = fontSize * 1.4;
    const padding = 10;
    const textWidth = fontSize * 14;

    // Background box
    ctx.fillStyle = 'rgba(0, 0, 0, 0.7)';
    ctx.fillRect(
        composite.width - textWidth - padding * 2,
        padding,
        textWidth + padding,
        lines.length * lineHeight + padding
    );

    // Text
    ctx.fillStyle = '#ffffff';
    ctx.font = `bold ${fontSize}px monospace`;
    ctx.textAlign = 'right';
    for (let i = 0; i < lines.length; i++) {
        ctx.fillText(lines[i], composite.width - padding * 2, padding + fontSize + i * lineHeight);
    }

    return composite.toDataURL('image/jpeg', 0.85);
}

async function saveMeasurementAction() {
    if (!state.calibration) return;

    // Build confirmation message
    const normDist = Math.abs(state.line2_x - state.line1_x);
    const inches = normDist * state.calibration.inches_per_norm;
    const totalRounded = Math.round(inches);
    const ft = Math.floor(totalRounded / 12);
    const remainIn = totalRounded % 12;
    const distStr = remainIn === 0 ? `${ft} ft` : `${ft} ft ${remainIn} in`;
    const maxWt = state.weightResult ? state.weightResult.maxAllowable.toLocaleString() + ' lbs' : '--';

    const msg = `Save this measurement?\n\nDistance: ${distStr}\nAxles: ${state.axleCount}\nMax Weight: ${maxWt}`;
    if (!(await appConfirm(msg))) return;

    const screenshot = captureScreenshot();
    const maxWeight = state.weightResult ? state.weightResult.maxAllowable : null;

    try {
        const data = await api('POST', '/measurements', {
            line1_norm: state.line1_x,
            line2_norm: state.line2_x,
            axle_count: state.axleCount,
            max_weight_lbs: maxWeight,
            is_special_vehicle: state.isSpecialVehicle,
            screenshot,
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
            tbody.innerHTML = '<tr><td colspan="6" style="text-align:center;color:#666">No measurements yet</td></tr>';
            return;
        }

        for (const m of data.measurements) {
            const tr = document.createElement('tr');
            const date = new Date(m.created_at + 'Z').toLocaleString();
            const maxWt = m.max_weight_lbs ? m.max_weight_lbs.toLocaleString() + ' lbs' : '-';
            const axles = m.axle_count ? m.axle_count + (m.is_special_vehicle ? ' (D/M)' : '') : '-';
            const imgHtml = m.screenshot
                ? `<a href="/api/screenshots/${m.screenshot}" target="_blank" class="img-link img-link-view">View</a><a href="/api/screenshots/${m.screenshot}" download="${m.screenshot}" class="img-link img-link-download">Download</a>`
                : '-';
            tr.innerHTML = `
                <td>${date}</td>
                <td><strong>${m.distance_display}</strong></td>
                <td>${axles}</td>
                <td>${maxWt}</td>
                <td>${m.measured_by || '-'}</td>
                <td>${imgHtml}</td>
            `;
            tbody.appendChild(tr);
        }
    } catch (err) {
        console.error('Failed to load history:', err);
    }
}

// === Controller Settings ===
const SETTINGS_DEFAULTS = {
    keymap: {
        line1_left: 'a', line1_right: 'd',
        line2_left: 'j', line2_right: 'l',
        toggle_mode: 'f', save: ' ', reset: 'r'
    },
    sensitivity: { coarse_step: 0.005, fine_step: 0.001 }
};

// Pending settings (edited but not yet saved)
let pendingSettings = null;

async function loadControllerSettings() {
    try {
        const data = await api('GET', '/settings/controller');
        controllerSettings = data.settings;
    } catch {
        // Use defaults on error
    }
    buildKeyActionMap();
    updateKeyHints();
}

function openSettings() {
    pendingSettings = JSON.parse(JSON.stringify(controllerSettings));
    const modal = $('settings-modal');
    modal.hidden = false;

    // Populate key mapping buttons
    for (const [action, key] of Object.entries(pendingSettings.keymap)) {
        const btn = modal.querySelector(`.key-capture-btn[data-action="${action}"]`);
        if (btn) {
            btn.textContent = formatKeyDisplay(key);
            btn.classList.remove('listening', 'conflict');
        }
    }

    // Populate sensitivity sliders
    $('slider-coarse').value = pendingSettings.sensitivity.coarse_step;
    $('val-coarse').textContent = pendingSettings.sensitivity.coarse_step;
    $('slider-fine').value = pendingSettings.sensitivity.fine_step;
    $('val-fine').textContent = pendingSettings.sensitivity.fine_step;

    // Show first tab
    switchSettingsTab('keymaps');
    $('keymap-error').hidden = true;

    // Update test labels
    updateTestLabels();
}

function closeSettings() {
    $('settings-modal').hidden = true;
    captureTarget = null;
    pendingSettings = null;
}

async function saveSettings() {
    if (!pendingSettings) return;

    // Read current slider values
    pendingSettings.sensitivity.coarse_step = parseFloat($('slider-coarse').value);
    pendingSettings.sensitivity.fine_step = parseFloat($('slider-fine').value);

    try {
        const data = await api('PUT', '/settings/controller', pendingSettings);
        controllerSettings = data.settings;
        pendingSettings = JSON.parse(JSON.stringify(controllerSettings));
        buildKeyActionMap();
        updateKeyHints();
        const confirm = $('settings-save-confirm');
        confirm.style.opacity = '1';
        setTimeout(() => { confirm.style.opacity = '0'; }, 2000);
    } catch (err) {
        $('keymap-error').textContent = err.message;
        $('keymap-error').hidden = false;
    }
}

function switchSettingsTab(tabName) {
    // Update tab buttons
    document.querySelectorAll('.settings-tab').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.tab === tabName);
    });
    // Update tab content
    document.querySelectorAll('.settings-tab-content').forEach(el => {
        el.hidden = true;
    });
    $('settings-tab-' + tabName).hidden = false;
}

function formatKeyDisplay(key) {
    if (key === ' ') return 'Space';
    if (key === 'escape') return 'Esc';
    if (key === 'arrowleft') return '\u2190';
    if (key === 'arrowright') return '\u2192';
    if (key === 'arrowup') return '\u2191';
    if (key === 'arrowdown') return '\u2193';
    if (key.startsWith('f') && key.length > 1) return key.toUpperCase();
    return key.toUpperCase();
}

// Key capture flow
function startKeyCapture(action) {
    captureTarget = action;
    const btn = $('settings-modal').querySelector(`.key-capture-btn[data-action="${action}"]`);
    if (btn) {
        btn.textContent = hasTouchScreen ? 'Tap to type key...' : 'Press a key...';
        btn.classList.add('listening');
        btn.classList.remove('conflict');
    }
    $('keymap-error').hidden = true;
    // On touch devices, focus hidden input to bring up keyboard
    if (hasTouchScreen) {
        mobileKeyInput.value = '';
        mobileKeyInput.focus();
    }
}

function completeKeyCapture(key) {
    if (!captureTarget || !pendingSettings) return;

    const normalizedKey = key.length === 1 ? key.toLowerCase() : key.toLowerCase();

    // Check for duplicates
    for (const [action, boundKey] of Object.entries(pendingSettings.keymap)) {
        if (action !== captureTarget && boundKey.toLowerCase() === normalizedKey) {
            const btn = $('settings-modal').querySelector(`.key-capture-btn[data-action="${captureTarget}"]`);
            if (btn) {
                btn.textContent = formatKeyDisplay(normalizedKey);
                btn.classList.remove('listening');
                btn.classList.add('conflict');
            }
            const actionLabel = getActionLabel(action);
            $('keymap-error').textContent = `"${formatKeyDisplay(normalizedKey)}" is already used for "${actionLabel}"`;
            $('keymap-error').hidden = false;
            captureTarget = null;
            return;
        }
    }

    // Store the key
    pendingSettings.keymap[captureTarget] = normalizedKey;
    const btn = $('settings-modal').querySelector(`.key-capture-btn[data-action="${captureTarget}"]`);
    if (btn) {
        btn.textContent = formatKeyDisplay(normalizedKey);
        btn.classList.remove('listening', 'conflict');
    }
    captureTarget = null;
    $('keymap-error').hidden = true;
    mobileKeyInput.blur();
    updateTestLabels();
}

function cancelKeyCapture() {
    if (!captureTarget || !pendingSettings) return;
    const btn = $('settings-modal').querySelector(`.key-capture-btn[data-action="${captureTarget}"]`);
    if (btn) {
        btn.textContent = formatKeyDisplay(pendingSettings.keymap[captureTarget]);
        btn.classList.remove('listening');
    }
    captureTarget = null;
    mobileKeyInput.blur();
}

function getActionLabel(action) {
    const labels = {
        line1_left: 'Line 1 Left', line1_right: 'Line 1 Right',
        line2_left: 'Line 2 Left', line2_right: 'Line 2 Right',
        toggle_mode: 'Fine/Coarse Toggle', save: 'Save Measurement', reset: 'Reset Lines'
    };
    return labels[action] || action;
}

function resetKeymapDefaults() {
    if (!pendingSettings) return;
    pendingSettings.keymap = JSON.parse(JSON.stringify(SETTINGS_DEFAULTS.keymap));
    const modal = $('settings-modal');
    for (const [action, key] of Object.entries(pendingSettings.keymap)) {
        const btn = modal.querySelector(`.key-capture-btn[data-action="${action}"]`);
        if (btn) {
            btn.textContent = formatKeyDisplay(key);
            btn.classList.remove('listening', 'conflict');
        }
    }
    $('keymap-error').hidden = true;
    updateTestLabels();
}

function resetSensitivityDefaults() {
    if (!pendingSettings) return;
    pendingSettings.sensitivity = JSON.parse(JSON.stringify(SETTINGS_DEFAULTS.sensitivity));
    $('slider-coarse').value = pendingSettings.sensitivity.coarse_step;
    $('val-coarse').textContent = pendingSettings.sensitivity.coarse_step;
    $('slider-fine').value = pendingSettings.sensitivity.fine_step;
    $('val-fine').textContent = pendingSettings.sensitivity.fine_step;
}

// Controller test visualizer
let testStats = { count: 0, times: [], lastAction: '' };
let activeJoyKeys = { joy1: new Set(), joy2: new Set() };

function resetTestStats() {
    testStats = { count: 0, times: [], lastAction: '' };
    activeJoyKeys = { joy1: new Set(), joy2: new Set() };
    updateTestStats();
}

function handleControllerTest(key, isDown) {
    const testTab = $('settings-tab-test');
    if (!testTab || testTab.hidden) return;

    const settings = pendingSettings || controllerSettings;
    const normalizedKey = key.toLowerCase();

    // Map key to test element, joystick group, and action name
    const testMap = {
        [settings.keymap.line1_left]:  { el: 'test-joy1-left', joy: 'joy1', name: 'Joy1 Left' },
        [settings.keymap.line1_right]: { el: 'test-joy1-right', joy: 'joy1', name: 'Joy1 Right' },
        [settings.keymap.line2_left]:  { el: 'test-joy2-left', joy: 'joy2', name: 'Joy2 Left' },
        [settings.keymap.line2_right]: { el: 'test-joy2-right', joy: 'joy2', name: 'Joy2 Right' },
        [settings.keymap.toggle_mode]: { el: 'test-action-toggle', joy: null, name: 'Toggle' },
        [settings.keymap.save]:        { el: 'test-action-save', joy: null, name: 'Save' },
        [settings.keymap.reset]:       { el: 'test-action-reset', joy: null, name: 'Reset' },
    };

    const entry = testMap[normalizedKey];
    if (!entry) return;

    const el = $(entry.el);
    if (el) {
        if (isDown) {
            el.classList.add('active');
            // Track active keys per joystick so center stays lit
            if (entry.joy) {
                activeJoyKeys[entry.joy].add(normalizedKey);
                const centerEl = $('test-' + entry.joy + '-center');
                if (centerEl) centerEl.classList.add('active');
            }
            // Track stats on keydown
            const now = Date.now();
            testStats.count++;
            testStats.times.push(now);
            testStats.lastAction = entry.name;
            // Keep only last 2 seconds of timestamps for rate calculation
            const cutoff = now - 2000;
            testStats.times = testStats.times.filter(t => t > cutoff);
            updateTestStats();
        } else {
            el.classList.remove('active');
            // Only remove center glow when no keys for this joystick are held
            if (entry.joy) {
                activeJoyKeys[entry.joy].delete(normalizedKey);
                if (activeJoyKeys[entry.joy].size === 0) {
                    const centerEl = $('test-' + entry.joy + '-center');
                    if (centerEl) centerEl.classList.remove('active');
                }
            }
        }
    }
}

function updateTestStats() {
    const countEl = $('test-stat-count');
    const rateEl = $('test-stat-rate');
    const lastEl = $('test-stat-last');
    if (!countEl) return;

    countEl.textContent = testStats.count;

    // Calculate rate from timestamps in the last 2 seconds
    const now = Date.now();
    const recent = testStats.times.filter(t => t > now - 2000);
    const rate = recent.length > 1 ? Math.round(recent.length / 2) : 0;
    rateEl.textContent = rate;

    lastEl.textContent = testStats.lastAction || '--';
}

function updateTestLabels() {
    const settings = pendingSettings || controllerSettings;
    const setLabel = (id, key) => {
        const el = $(id);
        if (el) el.textContent = formatKeyDisplay(key);
    };

    setLabel('test-joy1-left-label', settings.keymap.line1_left);
    setLabel('test-joy1-right-label', settings.keymap.line1_right);
    setLabel('test-joy2-left-label', settings.keymap.line2_left);
    setLabel('test-joy2-right-label', settings.keymap.line2_right);

    // Action indicators
    const setActionKey = (id, key) => {
        const el = $(id);
        if (el) {
            const keySpan = el.querySelector('.test-action-key');
            if (keySpan) keySpan.textContent = formatKeyDisplay(key);
        }
    };
    setActionKey('test-action-toggle', settings.keymap.toggle_mode);
    setActionKey('test-action-save', settings.keymap.save);
    setActionKey('test-action-reset', settings.keymap.reset);
}

function updateKeyHints() {
    const hintsEl = document.querySelector('.panel-hints');
    if (!hintsEl) return;
    const km = controllerSettings.keymap;
    hintsEl.innerHTML =
        `<div><kbd>${formatKeyDisplay(km.line1_left)}</kbd><kbd>${formatKeyDisplay(km.line1_right)}</kbd> Line 1</div>` +
        `<div><kbd>${formatKeyDisplay(km.line2_left)}</kbd><kbd>${formatKeyDisplay(km.line2_right)}</kbd> Line 2</div>` +
        `<div><kbd>${formatKeyDisplay(km.toggle_mode)}</kbd> Fine/Coarse</div>` +
        `<div><kbd>${formatKeyDisplay(km.save)}</kbd> Save</div>` +
        `<div><kbd>${formatKeyDisplay(km.reset)}</kbd> Reset</div>`;
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

$('btn-special-info').addEventListener('click', () => {
    appAlert(
        'Dump/Mix — Special Vehicle Cap\n\n' +
        'Florida law (FL Statute 316.535) limits certain vehicle types to 70,000 lbs GVW ' +
        'instead of the standard 80,000 lbs max.\n\n' +
        'This applies to:\n' +
        '  - Dump trucks\n' +
        '  - Concrete mixers\n' +
        '  - Waste haulers\n' +
        '  - Fuel transport vehicles\n\n' +
        'Enable this toggle when measuring one of these vehicle types so the weight ' +
        'calculator uses the correct 70,000 lb cap.'
    );
});

$('btn-weight-info').addEventListener('click', showWeightInfo);

// Settings
$('btn-settings').addEventListener('click', openSettings);
$('btn-settings-save').addEventListener('click', saveSettings);
$('btn-settings-cancel').addEventListener('click', () => {
    if (controllerSettings) {
        pendingSettings = JSON.parse(JSON.stringify(controllerSettings));
        openSettings();
    }
});
$('btn-settings-close').addEventListener('click', closeSettings);
$('btn-keymap-defaults').addEventListener('click', resetKeymapDefaults);
$('btn-sensitivity-defaults').addEventListener('click', resetSensitivityDefaults);
$('btn-test-reset').addEventListener('click', resetTestStats);

// Settings tabs
document.querySelectorAll('.settings-tab').forEach(tab => {
    tab.addEventListener('click', () => switchSettingsTab(tab.dataset.tab));
});

// Key capture buttons
document.querySelectorAll('.key-capture-btn').forEach(btn => {
    btn.addEventListener('click', () => startKeyCapture(btn.dataset.action));
});

// Cancel key capture when clicking anywhere else in the modal
$('settings-modal').addEventListener('click', (e) => {
    if (captureTarget && !e.target.classList.contains('key-capture-btn')) {
        cancelKeyCapture();
    }
});

// Sensitivity sliders
$('slider-coarse').addEventListener('input', (e) => {
    $('val-coarse').textContent = parseFloat(e.target.value).toFixed(3);
});
$('slider-fine').addEventListener('input', (e) => {
    $('val-fine').textContent = parseFloat(e.target.value).toFixed(4);
});

// Video source controls
$('video-source-type').addEventListener('change', (e) => {
    $('nvr-settings').hidden = (e.target.value !== 'nvr');
});

$('nvr-conn-type').addEventListener('change', (e) => {
    const isCloud = e.target.value === 'cloud';
    $('nvr-url-label').textContent = isCloud ? 'Cloud API' : 'NVR Address';
    $('nvr-api-url').value = isCloud ? 'https://api.cloud.dividia.net' : 'https://';
    $('nvr-api-url').placeholder = isCloud ? 'https://api.cloud.dividia.net' : 'https://192.168.0.22';
    $('nvr-username').placeholder = isCloud ? 'email@example.com' : 'admin';
});

$('btn-nvr-login').addEventListener('click', loginNVR);

$('btn-nvr-disconnect').addEventListener('click', () => {
    disconnectNVR();
    saveVideoSourceConfig({ type: 'test' });
    video.src = 'test-video.mp4';
});

// Populate video source fields from saved config on settings open
$('btn-settings').addEventListener('click', () => {
    const cfg = getVideoSourceConfig();
    $('video-source-type').value = cfg.type || 'test';
    $('nvr-settings').hidden = (cfg.type !== 'nvr');
    if (cfg.type === 'nvr') {
        $('nvr-conn-type').value = cfg.connType || 'cloud';
        $('nvr-api-url').value = cfg.apiUrl || 'https://api.cloud.dividia.net';
        $('nvr-username').value = cfg.username || '';
        const isCloud = (cfg.connType || 'cloud') === 'cloud';
        $('nvr-url-label').textContent = isCloud ? 'Cloud API' : 'NVR Address';
    }
    $('btn-nvr-login').hidden = !!nvrAuth;
    $('btn-nvr-disconnect').hidden = !nvrAuth;
    $('nvr-camera-select-group').hidden = !nvrAuth;
});

// Mode toggle button click
$('mode-indicator').addEventListener('click', toggleMode);

// Pointer drag for lines (unified mouse + touch + pen)
overlay.addEventListener('pointerdown', onOverlayPointerDown);
overlay.addEventListener('pointermove', onOverlayPointerMove);
overlay.addEventListener('pointerup', onOverlayPointerUp);
overlay.addEventListener('pointercancel', onOverlayPointerUp);

document.addEventListener('keydown', handleKeyDown);
document.addEventListener('keyup', handleKeyUp);
window.addEventListener('resize', resizeOverlay);

// Fullscreen toggle
$('btn-fullscreen').addEventListener('click', () => {
    if (document.fullscreenElement) {
        document.exitFullscreen();
    } else {
        document.documentElement.requestFullscreen().catch(() => {});
    }
});

// Video metadata loaded — we know dimensions, resize overlay
video.addEventListener('loadedmetadata', resizeOverlay);
video.addEventListener('playing', resizeOverlay);

// === Init ===
buildKeyActionMap();
checkAuth();
