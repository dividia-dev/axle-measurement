const { describe, it, before, after } = require('node:test');
const assert = require('node:assert');
const path = require('path');
const fs = require('fs');

// Use a temp DB for tests
process.env.DB_PATH = path.join(__dirname, 'fixtures', 'test-settings.db');

const { createApp } = require('../src/server');
const { createUser } = require('../src/auth');
const { closeDb } = require('../src/db');

async function request(app, method, url, { body, cookie } = {}) {
    return new Promise((resolve, reject) => {
        const server = app.listen(0, '127.0.0.1', () => {
            const port = server.address().port;
            const fullUrl = `http://127.0.0.1:${port}${url}`;
            const opts = {
                method,
                headers: { 'Content-Type': 'application/json' },
            };
            if (cookie) opts.headers['Cookie'] = cookie;
            if (body) opts.body = JSON.stringify(body);

            fetch(fullUrl, opts)
                .then(async (res) => {
                    const data = await res.json().catch(() => null);
                    server.close();
                    resolve({ status: res.status, body: data, headers: res.headers });
                })
                .catch((err) => { server.close(); reject(err); });
        });
    });
}

function extractCookie(headers) {
    const setCookie = headers.get('set-cookie');
    if (!setCookie) return null;
    return setCookie.split(';')[0];
}

describe('Controller Settings', () => {
    let app;
    let adminCookie;
    let operatorCookie;

    before(async () => {
        const dbPath = process.env.DB_PATH;
        if (fs.existsSync(dbPath)) fs.unlinkSync(dbPath);
        if (fs.existsSync(dbPath + '-wal')) fs.unlinkSync(dbPath + '-wal');
        if (fs.existsSync(dbPath + '-shm')) fs.unlinkSync(dbPath + '-shm');

        app = createApp();
        createUser('admin', 'admin123', 'admin');
        createUser('operator', 'op123', 'operator');

        // Login admin
        const adminRes = await request(app, 'POST', '/api/auth/login', {
            body: { username: 'admin', password: 'admin123' }
        });
        adminCookie = extractCookie(adminRes.headers);

        // Login operator
        const opRes = await request(app, 'POST', '/api/auth/login', {
            body: { username: 'operator', password: 'op123' }
        });
        operatorCookie = extractCookie(opRes.headers);
    });

    after(() => {
        closeDb();
        const dbPath = process.env.DB_PATH;
        if (fs.existsSync(dbPath)) fs.unlinkSync(dbPath);
        if (fs.existsSync(dbPath + '-wal')) fs.unlinkSync(dbPath + '-wal');
        if (fs.existsSync(dbPath + '-shm')) fs.unlinkSync(dbPath + '-shm');
    });

    describe('GET /api/settings/controller', () => {
        it('returns defaults when no settings saved', async () => {
            const res = await request(app, 'GET', '/api/settings/controller', { cookie: adminCookie });
            assert.strictEqual(res.status, 200);
            assert.strictEqual(res.body.settings.keymap.line1_left, 'a');
            assert.strictEqual(res.body.settings.keymap.line1_right, 'd');
            assert.strictEqual(res.body.settings.keymap.save, ' ');
            assert.strictEqual(res.body.settings.sensitivity.coarse_step, 0.005);
            assert.strictEqual(res.body.settings.sensitivity.fine_step, 0.001);
        });

        it('rejects unauthenticated requests', async () => {
            const res = await request(app, 'GET', '/api/settings/controller');
            assert.strictEqual(res.status, 401);
        });

        it('allows operator access', async () => {
            const res = await request(app, 'GET', '/api/settings/controller', { cookie: operatorCookie });
            assert.strictEqual(res.status, 200);
            assert.ok(res.body.settings.keymap);
        });
    });

    describe('PUT /api/settings/controller', () => {
        it('saves settings as admin', async () => {
            const settings = {
                keymap: {
                    line1_left: 'q', line1_right: 'e',
                    line2_left: 'u', line2_right: 'o',
                    toggle_mode: 'g', save: ' ', reset: 't'
                },
                sensitivity: { coarse_step: 0.01, fine_step: 0.002 }
            };
            const res = await request(app, 'PUT', '/api/settings/controller', {
                body: settings, cookie: adminCookie
            });
            assert.strictEqual(res.status, 200);
            assert.strictEqual(res.body.settings.keymap.line1_left, 'q');
            assert.strictEqual(res.body.settings.sensitivity.coarse_step, 0.01);
        });

        it('returns saved settings on subsequent GET', async () => {
            const res = await request(app, 'GET', '/api/settings/controller', { cookie: adminCookie });
            assert.strictEqual(res.status, 200);
            assert.strictEqual(res.body.settings.keymap.line1_left, 'q');
            assert.strictEqual(res.body.settings.sensitivity.coarse_step, 0.01);
        });

        it('rejects non-admin (operator)', async () => {
            const settings = {
                keymap: {
                    line1_left: 'x', line1_right: 'y',
                    line2_left: 'z', line2_right: 'w',
                    toggle_mode: 'v', save: ' ', reset: 'b'
                },
                sensitivity: { coarse_step: 0.005, fine_step: 0.001 }
            };
            const res = await request(app, 'PUT', '/api/settings/controller', {
                body: settings, cookie: operatorCookie
            });
            assert.strictEqual(res.status, 403);
        });

        it('rejects duplicate keys', async () => {
            const settings = {
                keymap: {
                    line1_left: 'a', line1_right: 'a', // duplicate
                    line2_left: 'j', line2_right: 'l',
                    toggle_mode: 'f', save: ' ', reset: 'r'
                },
                sensitivity: { coarse_step: 0.005, fine_step: 0.001 }
            };
            const res = await request(app, 'PUT', '/api/settings/controller', {
                body: settings, cookie: adminCookie
            });
            assert.strictEqual(res.status, 400);
            assert.ok(res.body.errors.some(e => e.includes('Duplicate')));
        });

        it('rejects out-of-range sensitivity', async () => {
            const settings = {
                keymap: {
                    line1_left: 'a', line1_right: 'd',
                    line2_left: 'j', line2_right: 'l',
                    toggle_mode: 'f', save: ' ', reset: 'r'
                },
                sensitivity: { coarse_step: 0.5, fine_step: 0.001 } // coarse way too high
            };
            const res = await request(app, 'PUT', '/api/settings/controller', {
                body: settings, cookie: adminCookie
            });
            assert.strictEqual(res.status, 400);
            assert.ok(res.body.errors.some(e => e.includes('coarse_step')));
        });

        it('rejects missing keymap fields', async () => {
            const settings = {
                keymap: { line1_left: 'a' }, // missing most keys
                sensitivity: { coarse_step: 0.005, fine_step: 0.001 }
            };
            const res = await request(app, 'PUT', '/api/settings/controller', {
                body: settings, cookie: adminCookie
            });
            assert.strictEqual(res.status, 400);
            assert.ok(res.body.errors.some(e => e.includes('Missing')));
        });
    });
});
