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
        // Clean up any previous test DB
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
        it('computes pixels_per_inch correctly', () => {
            const cal = saveCalibration({
                ref1_px: 100,
                ref2_px: 1100,
                known_distance_inches: 200,
                calibrated_by: 'test'
            });

            assert.strictEqual(cal.pixels_per_inch, 5.0);
            assert.strictEqual(cal.known_distance_inches, 200);
        });

        it('handles reversed reference points', () => {
            const cal = saveCalibration({
                ref1_px: 1100,
                ref2_px: 100,
                known_distance_inches: 200,
                calibrated_by: 'test'
            });

            assert.strictEqual(cal.pixels_per_inch, 5.0);
        });

        it('throws on same pixel positions', () => {
            assert.throws(() => {
                saveCalibration({
                    ref1_px: 500,
                    ref2_px: 500,
                    known_distance_inches: 100,
                    calibrated_by: 'test'
                });
            }, /same pixel/);
        });

        it('throws on zero or negative distance', () => {
            assert.throws(() => {
                saveCalibration({
                    ref1_px: 100,
                    ref2_px: 500,
                    known_distance_inches: 0,
                    calibrated_by: 'test'
                });
            }, /positive/);

            assert.throws(() => {
                saveCalibration({
                    ref1_px: 100,
                    ref2_px: 500,
                    known_distance_inches: -10,
                    calibrated_by: 'test'
                });
            }, /positive/);
        });

        it('deactivates previous calibrations', () => {
            saveCalibration({
                ref1_px: 100,
                ref2_px: 600,
                known_distance_inches: 100,
                calibrated_by: 'test'
            });

            saveCalibration({
                ref1_px: 200,
                ref2_px: 800,
                known_distance_inches: 120,
                calibrated_by: 'test'
            });

            const active = getActiveCalibration();
            assert.strictEqual(active.ref1_px, 200);
            assert.strictEqual(active.ref2_px, 800);

            const history = getCalibrationHistory();
            const activeCount = history.filter(c => c.is_active).length;
            assert.strictEqual(activeCount, 1);
        });
    });

    describe('getActiveCalibration', () => {
        it('returns null when no calibration exists', () => {
            // Close and recreate with fresh db
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
            const cal = { pixels_per_inch: 4.0 };
            const result = measureDistance(100, 500, cal);

            assert.strictEqual(result.pixel_distance, 400);
            assert.strictEqual(result.distance_inches, 100);
            assert.strictEqual(result.distance_display, '8 ft 4 in');
        });

        it('handles reversed line positions', () => {
            const cal = { pixels_per_inch: 4.0 };
            const result = measureDistance(500, 100, cal);

            assert.strictEqual(result.pixel_distance, 400);
            assert.strictEqual(result.distance_inches, 100);
        });

        it('returns null without calibration', () => {
            const result = measureDistance(100, 500, null);
            assert.strictEqual(result, null);
        });

        it('calculates zero distance for same position', () => {
            const cal = { pixels_per_inch: 4.0 };
            const result = measureDistance(300, 300, cal);

            assert.strictEqual(result.pixel_distance, 0);
            assert.strictEqual(result.distance_inches, 0);
            assert.strictEqual(result.distance_display, '0 ft');
        });

        it('handles real-world scale scenario', () => {
            // 1920px wide frame covering 60 feet (720 inches)
            // Reference stakes at pixel 480 and 1440, 20 feet (240 in) apart
            const cal = { pixels_per_inch: 960 / 240 }; // = 4.0 px/in

            // Axles at pixel 600 and 1320
            const result = measureDistance(600, 1320, cal);

            assert.strictEqual(result.pixel_distance, 720);
            assert.strictEqual(result.distance_inches, 180); // 15 feet
            assert.strictEqual(result.distance_display, '15 ft');
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
