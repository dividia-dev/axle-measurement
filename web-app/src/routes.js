const express = require('express');
const { createUser, authenticateUser, requireAuth, requireAdmin, getUsers, deleteUser, updateUser } = require('./auth');
const { saveCalibration, getActiveCalibration, getCalibrationHistory, measureDistance } = require('./calibration');
const { saveMeasurement, getMeasurements, getMeasurement, deleteMeasurement } = require('./measurements');
const { analyzeWeight, formatWeight } = require('./bridge-formula');

const router = express.Router();

// --- Auth ---

router.post('/auth/login', (req, res) => {
    const { username, password } = req.body;
    if (!username || !password) return res.status(400).json({ error: 'Username and password required' });

    const result = authenticateUser(username, password);
    if (!result) return res.status(401).json({ error: 'Invalid credentials' });

    res.cookie('token', result.token, {
        httpOnly: true,
        sameSite: 'strict',
        maxAge: 12 * 60 * 60 * 1000
    });

    res.json({ user: result.user });
});

router.post('/auth/logout', (req, res) => {
    res.clearCookie('token');
    res.json({ ok: true });
});

router.get('/auth/me', requireAuth, (req, res) => {
    res.json({ user: req.user });
});

// --- Users (admin only) ---

router.get('/users', requireAdmin, (req, res) => {
    res.json({ users: getUsers() });
});

router.post('/users', requireAdmin, (req, res) => {
    const { username, password, role } = req.body;
    if (!username || !password || !role) return res.status(400).json({ error: 'Username, password, and role required' });
    if (!['operator', 'admin'].includes(role)) return res.status(400).json({ error: 'Role must be operator or admin' });

    try {
        createUser(username, password, role);
        res.status(201).json({ ok: true });
    } catch (err) {
        if (err.message.includes('UNIQUE')) return res.status(409).json({ error: 'Username already exists' });
        res.status(500).json({ error: err.message });
    }
});

router.put('/users/:id', requireAdmin, (req, res) => {
    const result = updateUser(parseInt(req.params.id), req.body);
    if (!result) return res.status(400).json({ error: 'No valid fields to update' });
    res.json({ ok: true });
});

router.delete('/users/:id', requireAdmin, (req, res) => {
    deleteUser(parseInt(req.params.id));
    res.json({ ok: true });
});

// --- Calibration ---

router.get('/calibration', requireAuth, (req, res) => {
    const cal = getActiveCalibration();
    res.json({ calibration: cal });
});

router.get('/calibration/history', requireAuth, (req, res) => {
    res.json({ history: getCalibrationHistory() });
});

router.post('/calibration', requireAdmin, (req, res) => {
    const { ref1_px, ref2_px, known_distance_inches, camera_name } = req.body;
    if (ref1_px == null || ref2_px == null || !known_distance_inches) {
        return res.status(400).json({ error: 'ref1_px, ref2_px, and known_distance_inches required' });
    }

    try {
        const cal = saveCalibration({
            ref1_px: parseInt(ref1_px),
            ref2_px: parseInt(ref2_px),
            known_distance_inches: parseFloat(known_distance_inches),
            camera_name,
            calibrated_by: req.user.username
        });
        res.status(201).json({ calibration: cal });
    } catch (err) {
        res.status(400).json({ error: err.message });
    }
});

// --- Measurements ---

router.get('/measurements', requireAuth, (req, res) => {
    const limit = parseInt(req.query.limit) || 50;
    const offset = parseInt(req.query.offset) || 0;
    res.json(getMeasurements({ limit, offset }));
});

router.get('/measurements/:id', requireAuth, (req, res) => {
    const m = getMeasurement(parseInt(req.params.id));
    if (!m) return res.status(404).json({ error: 'Measurement not found' });
    res.json({ measurement: m });
});

router.post('/measurements', requireAuth, (req, res) => {
    const { line1_px, line2_px, notes } = req.body;
    if (line1_px == null || line2_px == null) {
        return res.status(400).json({ error: 'line1_px and line2_px required' });
    }

    const cal = getActiveCalibration();
    if (!cal) return res.status(400).json({ error: 'No active calibration. Calibrate first.' });

    const result = measureDistance(parseInt(line1_px), parseInt(line2_px), cal);

    const saved = saveMeasurement({
        line1_px: parseInt(line1_px),
        line2_px: parseInt(line2_px),
        pixel_distance: result.pixel_distance,
        distance_inches: result.distance_inches,
        distance_display: result.distance_display,
        calibration_id: cal.id,
        measured_by: req.user.username,
        notes
    });

    res.status(201).json({
        id: saved.id,
        ...result
    });
});

router.delete('/measurements/:id', requireAdmin, (req, res) => {
    deleteMeasurement(parseInt(req.params.id));
    res.json({ ok: true });
});

// --- Bridge Formula Weight Calculator ---

router.post('/weight-check', requireAuth, (req, res) => {
    const { distanceFeet, axleCount, scaleWeight, isSpecialVehicle } = req.body;
    if (!distanceFeet || !axleCount) {
        return res.status(400).json({ error: 'distanceFeet and axleCount required' });
    }

    const result = analyzeWeight(
        parseFloat(distanceFeet),
        parseInt(axleCount),
        {
            scaleWeight: scaleWeight != null ? parseFloat(scaleWeight) : null,
            isSpecialVehicle: !!isSpecialVehicle,
        }
    );

    if (!result) return res.status(400).json({ error: 'Invalid inputs' });
    res.json({ analysis: result });
});

module.exports = router;
