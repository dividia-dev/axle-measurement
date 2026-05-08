const { describe, it, before, after } = require('node:test');
const assert = require('node:assert');
const path = require('path');
const fs = require('fs');

// Use a temp DB for tests
process.env.DB_PATH = path.join(__dirname, 'fixtures', 'test-api.db');

const { createApp } = require('../src/server');
const { createUser } = require('../src/auth');
const { closeDb } = require('../src/db');

// Simple HTTP test helper (no supertest dependency needed)
async function request(app, method, path, { body, cookie } = {}) {
    return new Promise((resolve, reject) => {
        const server = app.listen(0, '127.0.0.1', () => {
            const port = server.address().port;
            const url = `http://127.0.0.1:${port}${path}`;
            const opts = {
                method,
                headers: { 'Content-Type': 'application/json' },
            };
            if (cookie) opts.headers['Cookie'] = cookie;
            if (body) opts.body = JSON.stringify(body);

            fetch(url, opts)
                .then(async (res) => {
                    const data = await res.json().catch(() => null);
                    server.close();
                    resolve({ status: res.status, body: data, headers: res.headers });
                })
                .catch((err) => {
                    server.close();
                    reject(err);
                });
        });
    });
}

function extractCookie(headers) {
    const setCookie = headers.get('set-cookie');
    if (!setCookie) return null;
    return setCookie.split(';')[0]; // "token=xyz"
}

describe('API', () => {
    let app;
    let adminCookie;
    let operatorCookie;

    before(() => {
        const dbPath = process.env.DB_PATH;
        if (fs.existsSync(dbPath)) fs.unlinkSync(dbPath);
        if (fs.existsSync(dbPath + '-wal')) fs.unlinkSync(dbPath + '-wal');
        if (fs.existsSync(dbPath + '-shm')) fs.unlinkSync(dbPath + '-shm');

        app = createApp();
        createUser('admin', 'admin123', 'admin');
        createUser('operator', 'op123', 'operator');
    });

    after(() => {
        closeDb();
        const dbPath = process.env.DB_PATH;
        if (fs.existsSync(dbPath)) fs.unlinkSync(dbPath);
        if (fs.existsSync(dbPath + '-wal')) fs.unlinkSync(dbPath + '-wal');
        if (fs.existsSync(dbPath + '-shm')) fs.unlinkSync(dbPath + '-shm');
    });

    describe('POST /api/auth/login', () => {
        it('logs in admin and returns cookie', async () => {
            const res = await request(app, 'POST', '/api/auth/login', {
                body: { username: 'admin', password: 'admin123' }
            });
            assert.strictEqual(res.status, 200);
            assert.strictEqual(res.body.user.username, 'admin');
            assert.strictEqual(res.body.user.role, 'admin');
            adminCookie = extractCookie(res.headers);
            assert.ok(adminCookie);
        });

        it('logs in operator', async () => {
            const res = await request(app, 'POST', '/api/auth/login', {
                body: { username: 'operator', password: 'op123' }
            });
            assert.strictEqual(res.status, 200);
            operatorCookie = extractCookie(res.headers);
            assert.ok(operatorCookie);
        });

        it('rejects invalid credentials', async () => {
            const res = await request(app, 'POST', '/api/auth/login', {
                body: { username: 'admin', password: 'wrong' }
            });
            assert.strictEqual(res.status, 401);
        });
    });

    describe('GET /api/auth/me', () => {
        it('returns user info with valid cookie', async () => {
            const res = await request(app, 'GET', '/api/auth/me', { cookie: adminCookie });
            assert.strictEqual(res.status, 200);
            assert.strictEqual(res.body.user.username, 'admin');
        });

        it('rejects without cookie', async () => {
            const res = await request(app, 'GET', '/api/auth/me');
            assert.strictEqual(res.status, 401);
        });
    });

    describe('POST /api/calibration', () => {
        it('saves calibration as admin', async () => {
            const res = await request(app, 'POST', '/api/calibration', {
                cookie: adminCookie,
                body: { ref1_px: 100, ref2_px: 1100, known_distance_inches: 240 }
            });
            assert.strictEqual(res.status, 201);
            assert.ok(res.body.calibration.pixels_per_inch);
            // 1000 pixels / 240 inches = 4.1667
            assert.ok(Math.abs(res.body.calibration.pixels_per_inch - (1000 / 240)) < 0.001);
        });

        it('rejects calibration from operator', async () => {
            const res = await request(app, 'POST', '/api/calibration', {
                cookie: operatorCookie,
                body: { ref1_px: 100, ref2_px: 1100, known_distance_inches: 240 }
            });
            assert.strictEqual(res.status, 403);
        });
    });

    describe('GET /api/calibration', () => {
        it('returns active calibration', async () => {
            const res = await request(app, 'GET', '/api/calibration', { cookie: operatorCookie });
            assert.strictEqual(res.status, 200);
            assert.ok(res.body.calibration);
            assert.ok(res.body.calibration.pixels_per_inch > 0);
        });
    });

    describe('POST /api/measurements', () => {
        it('saves a measurement as operator', async () => {
            const res = await request(app, 'POST', '/api/measurements', {
                cookie: operatorCookie,
                body: { line1_px: 200, line2_px: 900 }
            });
            assert.strictEqual(res.status, 201);
            assert.ok(res.body.distance_inches > 0);
            assert.ok(res.body.distance_display);
            assert.strictEqual(res.body.pixel_distance, 700);
        });

        it('rejects without auth', async () => {
            const res = await request(app, 'POST', '/api/measurements', {
                body: { line1_px: 200, line2_px: 900 }
            });
            assert.strictEqual(res.status, 401);
        });
    });

    describe('GET /api/measurements', () => {
        it('returns measurement history', async () => {
            const res = await request(app, 'GET', '/api/measurements', { cookie: operatorCookie });
            assert.strictEqual(res.status, 200);
            assert.ok(Array.isArray(res.body.measurements));
            assert.ok(res.body.measurements.length > 0);
            assert.ok(res.body.total > 0);
        });
    });

    describe('GET /api/users', () => {
        it('lists users as admin', async () => {
            const res = await request(app, 'GET', '/api/users', { cookie: adminCookie });
            assert.strictEqual(res.status, 200);
            assert.ok(res.body.users.length >= 2);
        });

        it('rejects listing users as operator', async () => {
            const res = await request(app, 'GET', '/api/users', { cookie: operatorCookie });
            assert.strictEqual(res.status, 403);
        });
    });
});
