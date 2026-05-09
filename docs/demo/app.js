/**
 * Axle Measurement — Demo Application (Static / SessionStorage)
 *
 * Self-contained demo that runs entirely in the browser.
 * All data stored in sessionStorage (clears when tab closes).
 * Bridge Formula B calculated client-side.
 */

// === Bridge Formula B (inlined from server) ===
const WEIGHT_LIMITS = {
    SINGLE_AXLE: 20000,
    TANDEM_AXLE: 34000,
    MAX_GVW: 80000,
    SPECIAL_VEHICLE_GVW: 70000,
};

function analyzeWeight(distanceFeet, axleCount, options = {}) {
    const { scaleWeight = null, isSpecialVehicle = false } = options;
    if (axleCount < 1) return null;

    if (axleCount === 1) {
        const analysis = {
            distanceFeet, axleCount,
            formulaWeight: WEIGHT_LIMITS.SINGLE_AXLE,
            maxAllowable: WEIGHT_LIMITS.SINGLE_AXLE,
            cap: 'SINGLE_AXLE',
            maxSingleAxle: WEIGHT_LIMITS.SINGLE_AXLE,
            maxTandemAxle: WEIGHT_LIMITS.TANDEM_AXLE,
            isSpecialVehicle,
        };
        return analysis;
    }

    const L = distanceFeet;
    const N = axleCount;
    const rawWeight = 500 * ((L * N) / (N - 1) + 12 * N + 36);
    const formulaWeight = Math.round(rawWeight / 500) * 500;

    let maxAllowable = formulaWeight;
    let cap = 'FORMULA';

    if (N === 2 && formulaWeight > WEIGHT_LIMITS.TANDEM_AXLE) {
        maxAllowable = WEIGHT_LIMITS.TANDEM_AXLE;
        cap = 'TANDEM_CAP';
    }
    if (maxAllowable > WEIGHT_LIMITS.MAX_GVW) {
        maxAllowable = WEIGHT_LIMITS.MAX_GVW;
        cap = 'GVW_CAP';
    }
    if (isSpecialVehicle && maxAllowable > WEIGHT_LIMITS.SPECIAL_VEHICLE_GVW) {
        maxAllowable = WEIGHT_LIMITS.SPECIAL_VEHICLE_GVW;
        cap = 'SPECIAL_VEHICLE_CAP';
    }

    const analysis = {
        distanceFeet, axleCount, formulaWeight, maxAllowable, cap,
        maxSingleAxle: WEIGHT_LIMITS.SINGLE_AXLE,
        maxTandemAxle: WEIGHT_LIMITS.TANDEM_AXLE,
        isSpecialVehicle,
    };

    if (scaleWeight !== null) {
        const overBy = scaleWeight - maxAllowable;
        analysis.scaleWeight = scaleWeight;
        analysis.isLegal = scaleWeight <= maxAllowable;
        analysis.overweightBy = overBy > 0 ? overBy : 0;
        analysis.remainingCapacity = overBy < 0 ? Math.abs(overBy) : 0;
        analysis.needsPermit = scaleWeight > maxAllowable;
    }

    return analysis;
}

// === SessionStorage Helpers ===
function storageGet(key) {
    try {
        const val = sessionStorage.getItem('axle_' + key);
        return val ? JSON.parse(val) : null;
    } catch { return null; }
}

function storageSet(key, value) {
    sessionStorage.setItem('axle_' + key, JSON.stringify(value));
    updateStorageMeter();
}

function updateStorageMeter() {
    let total = 0;
    for (let i = 0; i < sessionStorage.length; i++) {
        const key = sessionStorage.key(i);
        if (key.startsWith('axle_')) {
            total += sessionStorage.getItem(key).length * 2; // UTF-16
        }
    }
    const kb = (total / 1024).toFixed(1);
    const pct = Math.min(100, (total / (5 * 1024 * 1024)) * 100);
    const el = document.getElementById('storage-used');
    const fill = document.getElementById('storage-fill');
    if (el) el.textContent = kb + ' KB';
    if (fill) fill.style.width = pct + '%';
}

// === State ===
const state = {
    user: { username: 'demo', role: 'admin' },
    calibration: null,
    line1_x: 0.3,
    line2_x: 0.7,
    videoRect: null,
    isCalibrating: false,
    axleCount: 5,
    isSpecialVehicle: false,
    scaleWeight: null,
    weightResult: null,
    fineMode: false,
};

const COARSE_STEP = 0.005;
const FINE_STEP = 0.001;

// === DOM Elements ===
const $ = (id) => document.getElementById(id);
const video = $('camera-feed');
const overlay = $('overlay');
const measurementDisplay = $('measurement-display');
const measurementValue = $('measurement-value');
const calStatus = $('cal-indicator');
const btnSave = $('btn-save-measurement');
const btnReset = $('btn-reset-lines');
const btnCalibrate = $('btn-calibrate');
const btnHistory = $('btn-history');
const calModal = $('calibration-modal');
const historyModal = $('history-modal');
const weightPanel = $('weight-panel');
const axleCountDisplay = $('axle-count-display');
const weightMax = $('weight-max');
const weightScale = $('weight-scale');
const weightStatus = $('weight-status');
const chkSpecial = $('chk-special');

// === Reference Markers (normalized positions for calibration) ===
// Two fixed markers on the test pattern, positioned near the first and last axle.
// These are drawn on the OVERLAY so they don't appear in saved screenshots.
const REF_MARKER_1 = 0.37;  // normalized X position (tight, centered on truck mid-section)
const REF_MARKER_2 = 0.49;  // normalized X position (span = 0.12, ~1/4 of original)

function drawReferenceMarkers(ctx, rect) {
    const m1x = rect.x + REF_MARKER_1 * rect.width;
    const m2x = rect.x + REF_MARKER_2 * rect.width;
    const markerTop = rect.y + rect.height - 40;
    const markerBot = rect.y + rect.height - 8;

    // Draw small triangular markers at bottom of frame
    for (const mx of [m1x, m2x]) {
        // Upward-pointing triangle
        ctx.fillStyle = '#ff6600';
        ctx.beginPath();
        ctx.moveTo(mx, markerTop);
        ctx.lineTo(mx - 6, markerBot);
        ctx.lineTo(mx + 6, markerBot);
        ctx.closePath();
        ctx.fill();

        // Short tick line above triangle
        ctx.strokeStyle = '#ff6600';
        ctx.lineWidth = 2;
        ctx.setLineDash([]);
        ctx.beginPath();
        ctx.moveTo(mx, markerTop - 12);
        ctx.lineTo(mx, markerTop);
        ctx.stroke();
    }

    // Connecting line between markers
    ctx.strokeStyle = 'rgba(255, 102, 0, 0.3)';
    ctx.lineWidth = 1;
    ctx.setLineDash([4, 4]);
    ctx.beginPath();
    ctx.moveTo(m1x, markerBot - 16);
    ctx.lineTo(m2x, markerBot - 16);
    ctx.stroke();
    ctx.setLineDash([]);

    // Label
    const midX = (m1x + m2x) / 2;
    ctx.fillStyle = '#ff6600';
    ctx.font = '11px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText('REF', midX, markerTop - 16);
}

// === Test Pattern Video ===
function generateTestPattern() {
    const canvas = $('test-pattern');
    canvas.width = 1280;
    canvas.height = 720;
    const ctx = canvas.getContext('2d');

    // Clean background
    ctx.fillStyle = '#3a3a3a';
    ctx.fillRect(0, 0, 1280, 720);

    // Ground plane
    ctx.fillStyle = '#4a4a3a';
    ctx.fillRect(0, 500, 1280, 220);

    // Draw truck (clean, no labels)
    drawTruck(ctx);

    // Title (minimal)
    ctx.fillStyle = 'rgba(0,0,0,0.6)';
    ctx.fillRect(10, 10, 240, 32);
    ctx.fillStyle = '#0088ff';
    ctx.font = 'bold 14px sans-serif';
    ctx.textAlign = 'left';
    ctx.fillText('DEMO MODE', 20, 32);

    // Convert to video-like source
    try {
        const stream = canvas.captureStream(0);
        video.srcObject = stream;
        stream.getVideoTracks()[0].requestFrame();
    } catch {
        video.poster = canvas.toDataURL('image/png');
        video.style.objectFit = 'contain';
    }
}

function drawTruck(ctx) {
    const truckY = 300;
    const truckH = 180;
    const cabX = 800;
    const trailerX = 200;

    // Trailer body
    ctx.fillStyle = '#5a6a7a';
    ctx.fillRect(trailerX, truckY, cabX - trailerX - 20, truckH - 40);
    ctx.strokeStyle = '#4a5a6a';
    ctx.lineWidth = 2;
    ctx.strokeRect(trailerX, truckY, cabX - trailerX - 20, truckH - 40);

    // Cab
    ctx.fillStyle = '#cc4444';
    ctx.fillRect(cabX, truckY + 20, 120, truckH - 60);
    // Cab window
    ctx.fillStyle = '#8ab4d8';
    ctx.fillRect(cabX + 10, truckY + 30, 50, 40);
    // Cab front
    ctx.fillStyle = '#aa3333';
    ctx.fillRect(cabX + 100, truckY + 40, 40, truckH - 80);

    // Wheels (no labels, no arrows)
    const wheelY = truckY + truckH - 15;
    const wheelR = 25;
    const axlePositions = [280, 380, 720, 820, 900];

    for (const ax of axlePositions) {
        // Tire
        ctx.fillStyle = '#222';
        ctx.beginPath();
        ctx.arc(ax, wheelY, wheelR, 0, Math.PI * 2);
        ctx.fill();
        // Hub
        ctx.fillStyle = '#444';
        ctx.beginPath();
        ctx.arc(ax, wheelY, 8, 0, Math.PI * 2);
        ctx.fill();
    }
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
// Default calibration: 10ft across the reference marker span (0.37-0.49 = 0.12 norm)
const DEFAULT_CALIBRATION = {
    ref1_norm: REF_MARKER_1,
    ref2_norm: REF_MARKER_2,
    known_distance_inches: 240,
    inches_per_norm: 240 / (REF_MARKER_2 - REF_MARKER_1),
};

function loadCalibration() {
    state.calibration = storageGet('calibration') || DEFAULT_CALIBRATION;
    updateCalStatus();
}

function updateCalStatus() {
    if (state.calibration && state.calibration.inches_per_norm) {
        calStatus.textContent = 'CALIBRATED';
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
    state._preCalLine1 = state.line1_x;
    state._preCalLine2 = state.line2_x;

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

function saveCalibration() {
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

    const ref1_norm = state.line1_x;
    const ref2_norm = state.line2_x;
    const normDist = Math.abs(ref2_norm - ref1_norm);

    if (normDist < 0.01) {
        $('cal-error').textContent = 'Lines are too close together. Spread them apart.';
        $('cal-error').hidden = false;
        return;
    }

    const inches_per_norm = totalInches / normDist;
    const calibration = { ref1_norm, ref2_norm, known_distance_inches: totalInches, inches_per_norm };

    storageSet('calibration', calibration);
    state.calibration = calibration;
    updateCalStatus();

    // Don't restore old positions after successful save
    state._preCalLine1 = null;
    state._preCalLine2 = null;
    state.isCalibrating = false;
    calModal.hidden = true;
    drawOverlay();
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

// === Mouse Drag for Lines ===
const DRAG_HIT_ZONE = 15;
const FINE_DRAG_RATIO = 0.25;

let dragState = {
    active: false,
    line: null,
    startMouseX: 0,
    startLineNorm: 0,
};

function getLinePixelX(lineNorm) {
    const rect = getVideoRect();
    return rect.x + lineNorm * rect.width;
}

function pixelToNorm(pixelX) {
    const rect = getVideoRect();
    return Math.max(0, Math.min(1, (pixelX - rect.x) / rect.width));
}

function getHoveredLine(mouseX) {
    const l1x = getLinePixelX(state.line1_x);
    const l2x = getLinePixelX(state.line2_x);
    const d1 = Math.abs(mouseX - l1x);
    const d2 = Math.abs(mouseX - l2x);
    if (d1 <= DRAG_HIT_ZONE && d1 <= d2) return 1;
    if (d2 <= DRAG_HIT_ZONE) return 2;
    return null;
}

function onOverlayMouseDown(e) {
    const line = getHoveredLine(e.offsetX);
    if (line) {
        dragState.active = true;
        dragState.line = line;
        dragState.startMouseX = e.offsetX;
        dragState.startLineNorm = line === 1 ? state.line1_x : state.line2_x;
        $('video-container').classList.add('dragging');
        e.preventDefault();
    }
}

function onOverlayMouseMove(e) {
    if (dragState.active) {
        const rect = getVideoRect();
        const mouseDeltaPx = e.offsetX - dragState.startMouseX;
        const mouseDeltaNorm = mouseDeltaPx / rect.width;
        const ratio = state.fineMode ? FINE_DRAG_RATIO : 1.0;
        const newNorm = Math.max(0, Math.min(1, dragState.startLineNorm + mouseDeltaNorm * ratio));
        if (dragState.line === 1) state.line1_x = newNorm;
        else state.line2_x = newNorm;
        drawOverlay();
        debouncedWeightCheck();
    } else {
        const line = getHoveredLine(e.offsetX);
        const container = $('video-container');
        if (line) container.classList.add('drag-hover');
        else container.classList.remove('drag-hover');
    }
}

function onOverlayMouseUp() {
    if (dragState.active) {
        dragState.active = false;
        dragState.line = null;
        $('video-container').classList.remove('dragging');
    }
}

// === Weight Check (client-side Bridge Formula) ===
function updateAxleCount(delta) {
    state.axleCount = Math.max(2, Math.min(9, state.axleCount + delta));
    axleCountDisplay.textContent = state.axleCount;
    updateWeightCheck();
}

function updateWeightCheck() {
    if (!state.calibration || !state.calibration.inches_per_norm) return;

    const normDist = Math.abs(state.line2_x - state.line1_x);
    const distInches = normDist * state.calibration.inches_per_norm;
    const distFeet = distInches / 12;

    const analysis = analyzeWeight(distFeet, state.axleCount, {
        scaleWeight: state.scaleWeight,
        isSpecialVehicle: state.isSpecialVehicle,
    });

    if (analysis) {
        state.weightResult = analysis;
        renderWeightResult(analysis);
    } else {
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

let weightCheckTimeout = null;
function debouncedWeightCheck() {
    if (weightCheckTimeout) clearTimeout(weightCheckTimeout);
    weightCheckTimeout = setTimeout(updateWeightCheck, 300);
}

// === Video / Image Loading ===
function initVideo() {
    // Generate test pattern as default
    generateTestPattern();
}

function loadUserMedia(file) {
    const url = URL.createObjectURL(file);
    state._userMediaLoaded = true;

    if (file.type.startsWith('video/')) {
        // Load as video
        video.style.display = '';
        video.srcObject = null;
        video.poster = '';
        video.src = url;
        video.play().catch(() => {});
    } else if (file.type.startsWith('image/')) {
        // Load image into a canvas, then feed to video as a static frame
        const img = new Image();
        img.onload = () => {
            const canvas = $('test-pattern');
            canvas.width = img.naturalWidth;
            canvas.height = img.naturalHeight;
            const ctx = canvas.getContext('2d');
            ctx.drawImage(img, 0, 0);

            try {
                const stream = canvas.captureStream(0);
                video.srcObject = stream;
                stream.getVideoTracks()[0].requestFrame();
            } catch {
                video.poster = canvas.toDataURL('image/png');
            }
            setTimeout(resizeOverlay, 100);
            URL.revokeObjectURL(url);
        };
        img.src = url;
    }
}

// === Canvas Overlay ===
function getVideoRect() {
    const container = $('video-container');
    const cw = container.clientWidth;
    const ch = container.clientHeight;

    const vw = video.videoWidth || 1280;
    const vh = video.videoHeight || 720;

    const containerRatio = cw / ch;
    const videoRatio = vw / vh;

    let displayW, displayH, offsetX, offsetY;
    if (videoRatio > containerRatio) {
        displayW = cw;
        displayH = cw / videoRatio;
        offsetX = 0;
        offsetY = (ch - displayH) / 2;
    } else {
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
    ctx.strokeStyle = '#000000';
    ctx.lineWidth = 4;
    ctx.setLineDash([]);
    ctx.beginPath();
    ctx.moveTo(x, y1);
    ctx.lineTo(x, y2);
    ctx.stroke();

    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(x, y1);
    ctx.lineTo(x, y2);
    ctx.stroke();
}

function drawDragHandle(ctx, x, y, color) {
    const size = 8;
    ctx.fillStyle = '#000000';
    ctx.beginPath();
    ctx.moveTo(x, y - size - 1);
    ctx.lineTo(x + size + 1, y);
    ctx.lineTo(x, y + size + 1);
    ctx.lineTo(x - size - 1, y);
    ctx.closePath();
    ctx.fill();

    ctx.fillStyle = color;
    ctx.beginPath();
    ctx.moveTo(x, y - size);
    ctx.lineTo(x + size, y);
    ctx.lineTo(x, y + size);
    ctx.lineTo(x - size, y);
    ctx.closePath();
    ctx.fill();

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

    const l1x = rect.x + state.line1_x * rect.width;
    const l2x = rect.x + state.line2_x * rect.width;

    // Draw reference markers (overlay-only, won't appear in screenshots)
    if (!state._userMediaLoaded) {
        drawReferenceMarkers(ctx, rect);
    }

    drawLine(ctx, l1x, rect.y, rect.y + rect.height, lineColor);
    drawLine(ctx, l2x, rect.y, rect.y + rect.height, lineColor);

    const handleY = rect.y + rect.height / 2;
    drawDragHandle(ctx, l1x, handleY, lineColor);
    drawDragHandle(ctx, l2x, handleY, lineColor);

    const label1 = $('line1-label');
    const label2 = $('line2-label');
    label1.style.left = (l1x + 4) + 'px';
    label1.hidden = false;
    label2.style.left = (l2x + 4) + 'px';
    label2.hidden = false;
    label1.classList.toggle('cal-mode', inCalMode);
    label2.classList.toggle('cal-mode', inCalMode);

    $('video-container').classList.toggle('cal-mode', inCalMode);

    if (state.calibration && state.calibration.inches_per_norm) {
        const normDist = Math.abs(state.line2_x - state.line1_x);
        const inches = normDist * state.calibration.inches_per_norm;
        const totalRounded = Math.round(inches);
        const feet = Math.floor(totalRounded / 12);
        const remainInches = totalRounded % 12;

        let displayText;
        if (remainInches === 0) displayText = `${feet} ft`;
        else displayText = `${feet} ft ${remainInches} in`;
        measurementValue.textContent = displayText;
        measurementDisplay.hidden = false;

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
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
    if (!historyModal.hidden) return;

    let handled = true;
    const step = state.fineMode ? FINE_STEP : COARSE_STEP;

    switch (e.key.toLowerCase()) {
        case 'a': state.line1_x = Math.max(0, state.line1_x - step); break;
        case 'd': state.line1_x = Math.min(1, state.line1_x + step); break;
        case 'j': state.line2_x = Math.max(0, state.line2_x - step); break;
        case 'l': state.line2_x = Math.min(1, state.line2_x + step); break;
        case 'f': toggleMode(); break;
        case ' ':
            e.preventDefault();
            saveMeasurementAction();
            break;
        case 'r': resetLines(); break;
        case 'escape':
            closeCalibration();
            historyModal.hidden = true;
            break;
        default: handled = false;
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

function captureScreenshot() {
    const rect = getVideoRect();
    const composite = document.createElement('canvas');
    const sourceCanvas = $('test-pattern');
    const useTestPattern = !video.videoWidth && sourceCanvas.width > 0;

    composite.width = useTestPattern ? sourceCanvas.width : (video.videoWidth || rect.width);
    composite.height = useTestPattern ? sourceCanvas.height : (video.videoHeight || rect.height);
    const ctx = composite.getContext('2d');

    if (useTestPattern) {
        ctx.drawImage(sourceCanvas, 0, 0);
    } else {
        ctx.drawImage(video, 0, 0, composite.width, composite.height);
    }

    const l1x = state.line1_x * composite.width;
    const l2x = state.line2_x * composite.width;

    ctx.strokeStyle = '#000000';
    ctx.lineWidth = 4;
    ctx.beginPath(); ctx.moveTo(l1x, 0); ctx.lineTo(l1x, composite.height); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(l2x, 0); ctx.lineTo(l2x, composite.height); ctx.stroke();

    ctx.strokeStyle = '#0088FF';
    ctx.lineWidth = 2;
    ctx.beginPath(); ctx.moveTo(l1x, 0); ctx.lineTo(l1x, composite.height); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(l2x, 0); ctx.lineTo(l2x, composite.height); ctx.stroke();

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

    ctx.fillStyle = 'rgba(0, 0, 0, 0.7)';
    ctx.fillRect(composite.width - textWidth - padding * 2, padding, textWidth + padding, lines.length * lineHeight + padding);

    ctx.fillStyle = '#ffffff';
    ctx.font = `bold ${fontSize}px monospace`;
    ctx.textAlign = 'right';
    for (let i = 0; i < lines.length; i++) {
        ctx.fillText(lines[i], composite.width - padding * 2, padding + fontSize + i * lineHeight);
    }

    return composite.toDataURL('image/jpeg', 0.85);
}

function saveMeasurementAction() {
    if (!state.calibration) return;

    const normDist = Math.abs(state.line2_x - state.line1_x);
    const inches = normDist * state.calibration.inches_per_norm;
    const totalRounded = Math.round(inches);
    const ft = Math.floor(totalRounded / 12);
    const remainIn = totalRounded % 12;
    const distStr = remainIn === 0 ? `${ft} ft` : `${ft} ft ${remainIn} in`;
    const maxWt = state.weightResult ? state.weightResult.maxAllowable.toLocaleString() + ' lbs' : '--';

    const msg = `Save this measurement?\n\nDistance: ${distStr}\nAxles: ${state.axleCount}\nMax Weight: ${maxWt}`;
    if (!confirm(msg)) return;

    const screenshot = captureScreenshot();
    const maxWeight = state.weightResult ? state.weightResult.maxAllowable : null;

    const measurement = {
        id: Date.now(),
        created_at: new Date().toISOString(),
        line1_norm: state.line1_x,
        line2_norm: state.line2_x,
        distance_display: distStr,
        axle_count: state.axleCount,
        max_weight_lbs: maxWeight,
        is_special_vehicle: state.isSpecialVehicle,
        measured_by: 'demo',
        screenshot: screenshot,
        notes: '',
    };

    const measurements = storageGet('measurements') || [];
    measurements.unshift(measurement);

    // Keep max 50 measurements
    if (measurements.length > 50) measurements.length = 50;

    try {
        storageSet('measurements', measurements);
        measurementDisplay.style.borderColor = '#00ff00';
        setTimeout(() => { measurementDisplay.style.borderColor = '#0088ff'; }, 500);
    } catch (e) {
        if (e.name === 'QuotaExceededError') {
            alert('Session storage is full. Clear some data to save more measurements.');
        }
    }
}

function showHistory() {
    historyModal.hidden = false;
    const measurements = storageGet('measurements') || [];
    const tbody = $('history-body');
    tbody.innerHTML = '';

    if (measurements.length === 0) {
        tbody.innerHTML = '<tr><td colspan="6" style="text-align:center;color:#666">No measurements yet</td></tr>';
        return;
    }

    for (const m of measurements) {
        const tr = document.createElement('tr');
        const date = new Date(m.created_at).toLocaleString();
        const maxWt = m.max_weight_lbs ? m.max_weight_lbs.toLocaleString() + ' lbs' : '-';
        const axles = m.axle_count ? m.axle_count + (m.is_special_vehicle ? ' (D/M)' : '') : '-';

        let imgHtml = '-';
        if (m.screenshot) {
            imgHtml = `<a href="${m.screenshot}" target="_blank" class="img-link img-link-view">View</a><a href="${m.screenshot}" download="measurement-${m.id}.jpg" class="img-link img-link-download">Download</a>`;
        }

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
}

function clearDemoData() {
    if (!confirm('Clear all demo data? This will remove calibration and all saved measurements.')) return;
    const keysToRemove = [];
    for (let i = 0; i < sessionStorage.length; i++) {
        const key = sessionStorage.key(i);
        if (key.startsWith('axle_')) keysToRemove.push(key);
    }
    keysToRemove.forEach(k => sessionStorage.removeItem(k));
    state.calibration = null;
    updateCalStatus();
    updateStorageMeter();
}

// === Event Listeners ===
btnReset.addEventListener('click', resetLines);
btnSave.addEventListener('click', saveMeasurementAction);
btnCalibrate.addEventListener('click', openCalibration);
btnHistory.addEventListener('click', showHistory);
$('btn-history-close').addEventListener('click', () => { historyModal.hidden = true; });
$('btn-cal-save').addEventListener('click', saveCalibration);
$('btn-cal-cancel').addEventListener('click', closeCalibration);
$('btn-clear-data').addEventListener('click', clearDemoData);
$('btn-special-info').addEventListener('click', () => {
    alert(
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
$('btn-load-media').addEventListener('click', () => $('file-input').click());
$('file-input').addEventListener('change', (e) => {
    if (e.target.files[0]) loadUserMedia(e.target.files[0]);
});

$('btn-axle-minus').addEventListener('click', () => updateAxleCount(-1));
$('btn-axle-plus').addEventListener('click', () => updateAxleCount(1));
chkSpecial.addEventListener('change', () => {
    state.isSpecialVehicle = chkSpecial.checked;
    updateWeightCheck();
});

$('mode-indicator').addEventListener('click', toggleMode);

overlay.addEventListener('mousedown', onOverlayMouseDown);
overlay.addEventListener('mousemove', onOverlayMouseMove);
overlay.addEventListener('mouseup', onOverlayMouseUp);
overlay.addEventListener('mouseleave', onOverlayMouseUp);

document.addEventListener('keydown', handleKeyDown);
window.addEventListener('resize', resizeOverlay);

video.addEventListener('loadedmetadata', resizeOverlay);
video.addEventListener('playing', resizeOverlay);

// === Init ===
loadCalibration();
initVideo();
startClock();
resizeOverlay();
updateStorageMeter();
