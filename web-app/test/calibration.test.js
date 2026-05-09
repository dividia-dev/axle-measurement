const { describe, it, before, after } = require('node:test');
const assert = require('node:assert');
const path = require('path');

// Use a temp DB for tests
process.env.DB_PATH = path.join(__dirname, 'fixtures', 'test-cal.db');

const { saveCalibration, getActiveCalibration, getCalibrationHistory, measureDistance, formatDistance } = require('../src/calibration');
const { closeDb } = require('../src/db');
const fs = require('fs');

describe('Calibration', () => {
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

    describe('saveCalibration', () => {
        it('computes inches_per_norm correctly', () => {
            // ref1 at 0.2, ref2 at 0.8 = 0.6 normalized distance
            // known distance = 240 inches
            // inches_per_norm = 240 / 0.6 = 400
            const cal = saveCalibration({
                ref1_norm: 0.2,
                ref2_norm: 0.8,
                known_distance_inches: 240,
                calibrated_by: 'test'
            });

            assert.ok(Math.abs(cal.inches_per_norm - 400) < 0.01);
            assert.strictEqual(cal.known_distance_inches, 240);
        });

        it('handles reversed reference points', () => {
            const cal = saveCalibration({
                ref1_norm: 0.8,
                ref2_norm: 0.2,
                known_distance_inches: 240,
                calibrated_by: 'test'
            });

            assert.ok(Math.abs(cal.inches_per_norm - 400) < 0.01);
        });

        it('throws on same positions', () => {
            assert.throws(() => {
                saveCalibration({
                    ref1_norm: 0.5,
                    ref2_norm: 0.5,
                    known_distance_inches: 100,
                    calibrated_by: 'test'
                });
            }, /too close/);
        });

        it('throws on zero or negative distance', () => {
            assert.throws(() => {
                saveCalibration({
                    ref1_norm: 0.2,
                    ref2_norm: 0.8,
                    known_distance_inches: 0,
                    calibrated_by: 'test'
                });
            }, /positive/);
        });

        it('deactivates previous calibrations', () => {
            saveCalibration({
                ref1_norm: 0.1,
                ref2_norm: 0.9,
                known_distance_inches: 480,
                calibrated_by: 'test'
            });

            saveCalibration({
                ref1_norm: 0.2,
                ref2_norm: 0.7,
                known_distance_inches: 300,
                calibrated_by: 'test'
            });

            const active = getActiveCalibration();
            assert.ok(Math.abs(active.ref1_norm - 0.2) < 0.001);
            assert.ok(Math.abs(active.ref2_norm - 0.7) < 0.001);

            const history = getCalibrationHistory();
            const activeCount = history.filter(c => c.is_active).length;
            assert.strictEqual(activeCount, 1);
        });

        it('stores and retrieves normalized positions accurately', () => {
            const cal = saveCalibration({
                ref1_norm: 0.3456,
                ref2_norm: 0.7891,
                known_distance_inches: 200,
                calibrated_by: 'test'
            });

            const active = getActiveCalibration();
            // Stored as *10000 integers, so precision to 4 decimal places
            assert.ok(Math.abs(active.ref1_norm - 0.3456) < 0.001);
            assert.ok(Math.abs(active.ref2_norm - 0.7891) < 0.001);
            assert.ok(active.inches_per_norm > 0);
        });
    });

    describe('getActiveCalibration', () => {
        it('returns null when no calibration exists', () => {
            closeDb();
            const dbPath = process.env.DB_PATH;
            if (fs.existsSync(dbPath)) fs.unlinkSync(dbPath);
            if (fs.existsSync(dbPath + '-wal')) fs.unlinkSync(dbPath + '-wal');
            if (fs.existsSync(dbPath + '-shm')) fs.unlinkSync(dbPath + '-shm');

            const cal = getActiveCalibration();
            assert.strictEqual(cal, null);
        });
    });

    describe('measureDistance', () => {
        it('calculates distance correctly', () => {
            // inches_per_norm = 400, distance from 0.2 to 0.7 = 0.5
            // expected: 0.5 * 400 = 200 inches = 16 ft 8 in
            const cal = { inches_per_norm: 400 };
            const result = measureDistance(0.2, 0.7, cal);

            assert.ok(Math.abs(result.norm_distance - 0.5) < 0.0001);
            assert.ok(Math.abs(result.distance_inches - 200) < 0.01);
            assert.strictEqual(result.distance_display, '16 ft 8 in');
        });

        it('handles reversed line positions', () => {
            const cal = { inches_per_norm: 400 };
            const result = measureDistance(0.7, 0.2, cal);

            assert.ok(Math.abs(result.norm_distance - 0.5) < 0.0001);
            assert.ok(Math.abs(result.distance_inches - 200) < 0.01);
        });

        it('returns null without calibration', () => {
            const result = measureDistance(0.2, 0.7, null);
            assert.strictEqual(result, null);
        });

        it('calculates zero distance for same position', () => {
            const cal = { inches_per_norm: 400 };
            const result = measureDistance(0.5, 0.5, cal);

            assert.strictEqual(result.norm_distance, 0);
            assert.strictEqual(result.distance_inches, 0);
            assert.strictEqual(result.distance_display, '0 ft');
        });

        it('gives same result regardless of window size', () => {
            // This is the key test — normalized measurement is window-independent
            // Simulating: ref points at 0.25 and 0.75, 20 feet apart
            // inches_per_norm = 240 / 0.5 = 480
            const cal = { inches_per_norm: 480 };

            // Lines at 0.3 and 0.6 — always the same normalized distance
            const result = measureDistance(0.3, 0.6, cal);

            // 0.3 norm dist * 480 = 144 inches = 12 ft
            assert.strictEqual(result.distance_inches, 144);
            assert.strictEqual(result.distance_display, '12 ft');

            // A different "window size" doesn't matter — same normalized positions,
            // same inches_per_norm, same result. This was the bug before.
        });
    });

    describe('formatDistance', () => {
        it('formats whole feet', () => {
            assert.strictEqual(formatDistance(120), '10 ft');
            assert.strictEqual(formatDistance(180), '15 ft');
        });

        it('formats feet and inches', () => {
            assert.strictEqual(formatDistance(125), '10 ft 5 in');
            assert.strictEqual(formatDistance(41 * 12 + 3), '41 ft 3 in');
        });

        it('formats small distances', () => {
            assert.strictEqual(formatDistance(6), '0 ft 6 in');
        });
    });
});
