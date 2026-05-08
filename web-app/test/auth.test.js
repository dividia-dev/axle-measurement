const { describe, it, before, after } = require('node:test');
const assert = require('node:assert');
const path = require('path');

// Use a temp DB for tests
process.env.DB_PATH = path.join(__dirname, 'fixtures', 'test-auth.db');

const { createUser, authenticateUser, verifyToken, JWT_SECRET } = require('../src/auth');
const { closeDb } = require('../src/db');
const fs = require('fs');

describe('Auth', () => {
    before(() => {
        const dbPath = process.env.DB_PATH;
        if (fs.existsSync(dbPath)) fs.unlinkSync(dbPath);
        if (fs.existsSync(dbPath + '-wal')) fs.unlinkSync(dbPath + '-wal');
        if (fs.existsSync(dbPath + '-shm')) fs.unlinkSync(dbPath + '-shm');
    });

    after(() => {
        closeDb();
        const dbPath = process.env.DB_PATH;
        if (fs.existsSync(dbPath)) fs.unlinkSync(dbPath);
        if (fs.existsSync(dbPath + '-wal')) fs.unlinkSync(dbPath + '-wal');
        if (fs.existsSync(dbPath + '-shm')) fs.unlinkSync(dbPath + '-shm');
    });

    describe('createUser', () => {
        it('creates a user', () => {
            const result = createUser('testadmin', 'password123', 'admin');
            assert.ok(result.lastInsertRowid > 0);
        });

        it('creates an operator user', () => {
            const result = createUser('testop', 'oppass', 'operator');
            assert.ok(result.lastInsertRowid > 0);
        });

        it('rejects duplicate username', () => {
            assert.throws(() => {
                createUser('testadmin', 'differentpass', 'admin');
            });
        });
    });

    describe('authenticateUser', () => {
        it('authenticates with correct credentials', () => {
            const result = authenticateUser('testadmin', 'password123');
            assert.ok(result);
            assert.ok(result.token);
            assert.strictEqual(result.user.username, 'testadmin');
            assert.strictEqual(result.user.role, 'admin');
        });

        it('rejects wrong password', () => {
            const result = authenticateUser('testadmin', 'wrongpass');
            assert.strictEqual(result, null);
        });

        it('rejects nonexistent user', () => {
            const result = authenticateUser('nosuchuser', 'password');
            assert.strictEqual(result, null);
        });
    });

    describe('verifyToken', () => {
        it('verifies a valid token', () => {
            const auth = authenticateUser('testadmin', 'password123');
            const decoded = verifyToken(auth.token);
            assert.ok(decoded);
            assert.strictEqual(decoded.username, 'testadmin');
            assert.strictEqual(decoded.role, 'admin');
        });

        it('rejects an invalid token', () => {
            const decoded = verifyToken('not.a.valid.token');
            assert.strictEqual(decoded, null);
        });

        it('rejects an empty token', () => {
            const decoded = verifyToken('');
            assert.strictEqual(decoded, null);
        });
    });
});
