const express = require('express');
const { createUser, authenticateUser, requireAuth, requireAdmin, getUsers, deleteUser, updateUser } = require('./auth');
const { saveCalibration, getActiveCalibration, getCalibrationHistory, measureDistance } = require('./calibration');
const { saveMeasurement, getMeasurements, getMeasurement, deleteMeasurement, SCREENSHOT_DIR } = require('./measurements');
const { analyzeWeight, formatWeight } = require('./bridge-formula');
const { getControllerSettings, saveControllerSettings } = require('./settings');
const path = require('path');

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
    const { ref1_norm, ref2_norm, known_distance_inches, inches_per_norm, camera_name } = req.body;
    if (ref1_norm == null || ref2_norm == null || !known_distance_inches) {
        return res.status(400).json({ error: 'ref1_norm, ref2_norm, and known_distance_inches required' });
    }

    try {
        const cal = saveCalibration({
            ref1_norm: parseFloat(ref1_norm),
            ref2_norm: parseFloat(ref2_norm),
            known_distance_inches: parseFloat(known_distance_inches),
            inches_per_norm: inches_per_norm ? parseFloat(inches_per_norm) : undefined,
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
    const { line1_norm, line2_norm, notes, axle_count, max_weight_lbs, is_special_vehicle, screenshot } = req.body;
    if (line1_norm == null || line2_norm == null) {
        return res.status(400).json({ error: 'line1_norm and line2_norm required' });
    }

    const cal = getActiveCalibration();
    if (!cal) return res.status(400).json({ error: 'No active calibration. Calibrate first.' });

    const result = measureDistance(parseFloat(line1_norm), parseFloat(line2_norm), cal);

    const saved = saveMeasurement({
        line1_px: Math.round(parseFloat(line1_norm) * 10000),
        line2_px: Math.round(parseFloat(line2_norm) * 10000),
        pixel_distance: Math.round(result.norm_distance * 10000),
        distance_inches: result.distance_inches,
        distance_display: result.distance_display,
        calibration_id: cal.id,
        measured_by: req.user.username,
        notes,
        axle_count: axle_count ? parseInt(axle_count) : null,
        max_weight_lbs: max_weight_lbs ? parseFloat(max_weight_lbs) : null,
        is_special_vehicle: !!is_special_vehicle,
        screenshot
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

// --- Screenshots ---

router.get('/screenshots/:filename', requireAuth, (req, res) => {
    const filePath = path.join(SCREENSHOT_DIR, req.params.filename);
    res.sendFile(filePath, (err) => {
        if (err) res.status(404).json({ error: 'Screenshot not found' });
    });
});

// --- Controller Settings ---

router.get('/settings/controller', requireAuth, (req, res) => {
    res.json({ settings: getControllerSettings() });
});

router.put('/settings/controller', requireAdmin, (req, res) => {
    const result = saveControllerSettings(req.body);
    if (!result.ok) return res.status(400).json({ errors: result.errors });
    res.json({ settings: result.settings });
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

// --- Dividia Cloud API Proxy (avoids CORS for browser → cloud auth) ---

router.post('/nvr/auth', async (req, res) => {
    const { apiUrl, username, password } = req.body;
    if (!username || !password) {
        return res.status(400).json({ error: 'Username and password required' });
    }

    const base = (apiUrl || 'https://api.cloud.dividia.net').replace(/\/+$/, '');
    const url = `${base}/ve/auth`;

    try {
        const https = require('https');
        const http = require('http');
        const data = await new Promise((resolve, reject) => {
            const payload = JSON.stringify({ username, password });
            const parsed = new URL(url);
            const isSecure = parsed.protocol === 'https:';
            const opts = {
                hostname: parsed.hostname,
                port: parsed.port || (isSecure ? 443 : 80),
                path: parsed.pathname,
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Content-Length': Buffer.byteLength(payload)
                },
                rejectUnauthorized: false
            };
            const transport = isSecure ? https : http;
            const apiReq = transport.request(opts, (apiRes) => {
                let body = '';
                apiRes.on('data', chunk => body += chunk);
                apiRes.on('end', () => {
                    try {
                        const json = JSON.parse(body);
                        if (apiRes.statusCode >= 400) {
                            reject({ status: apiRes.statusCode, data: json });
                        } else {
                            resolve(json);
                        }
                    } catch {
                        reject(new Error('Unexpected response from API (not JSON)'));
                    }
                });
            });
            apiReq.on('error', reject);
            apiReq.write(payload);
            apiReq.end();
        });
        res.json(data);
    } catch (err) {
        if (err.status && err.data) {
            return res.status(err.status).json(err.data);
        }
        res.status(502).json({ error: 'Cannot reach API: ' + (err.message || err) });
    }
});

// --- Local NVR JSON-RPC auth proxy ---

router.post('/nvr/auth-local', async (req, res) => {
    const { nvrUrl, username, password } = req.body;
    if (!nvrUrl || !username || !password) {
        return res.status(400).json({ error: 'nvrUrl, username, and password required' });
    }

    const base = nvrUrl.replace(/\/+$/, '');
    const url = `${base}/api`;

    try {
        const https = require('https');
        const http = require('http');

        const rpc = (method, params, id) => new Promise((resolve, reject) => {
            const payload = JSON.stringify({ jsonrpc: 2, method, params, id });
            const parsed = new URL(url);
            const isSecure = parsed.protocol === 'https:';
            const opts = {
                hostname: parsed.hostname,
                port: parsed.port || (isSecure ? 443 : 80),
                path: parsed.pathname,
                method: 'POST',
                headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(payload) },
                rejectUnauthorized: false
            };
            const transport = isSecure ? https : http;
            const r = transport.request(opts, (resp) => {
                let body = '';
                resp.on('data', chunk => body += chunk);
                resp.on('end', () => {
                    try { resolve(JSON.parse(body)); }
                    catch { reject(new Error('Invalid JSON from NVR')); }
                });
            });
            r.on('error', reject);
            r.write(payload);
            r.end();
        });

        // Login
        const loginRes = await rpc('auth.loginUser', [username, password, true, true], 2);
        if (loginRes.error) {
            return res.status(401).json({ error: loginRes.error.message || 'Login failed' });
        }
        const session = loginRes.result?.[1]?.[0] || loginRes.result;
        if (!session || typeof session !== 'string') {
            return res.status(401).json({ error: 'No session returned' });
        }

        // Get cameras
        const camsRes = await rpc('config.camera.getAllCameras', [session], 11);
        const cameras = camsRes.result?.[1] || camsRes.result || [];

        // Get serial
        const serialRes = await rpc('info.getSerial', [], 3);
        const serial = serialRes.result?.[1]?.[0] || serialRes.result || '';

        res.json({ session, serial, cameras });
    } catch (err) {
        res.status(502).json({ error: 'Cannot reach NVR: ' + (err.message || err) });
    }
});

module.exports = router;
