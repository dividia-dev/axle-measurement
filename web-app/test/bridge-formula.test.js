const { describe, it } = require('node:test');
const assert = require('node:assert');

const { calculateBridgeFormula, analyzeWeight, WEIGHT_LIMITS } = require('../src/bridge-formula');

describe('Bridge Formula', () => {
    describe('calculateBridgeFormula', () => {
        it('returns single axle limit for 1 axle', () => {
            const result = calculateBridgeFormula(0, 1);
            assert.strictEqual(result.maxAllowable, 20000);
            assert.strictEqual(result.cap, 'SINGLE_AXLE');
        });

        it('calculates correctly for 5 axles at 41 feet', () => {
            // W = 500 * ((41*5)/(5-1) + 12*5 + 36)
            // W = 500 * (51.25 + 60 + 36) = 500 * 147.25 = 73,625 → rounds to 73,500
            const result = calculateBridgeFormula(41, 5);
            assert.strictEqual(result.formulaWeight, 73500);
            assert.strictEqual(result.maxAllowable, 73500);
            assert.strictEqual(result.cap, 'FORMULA');
        });

        it('calculates correctly for 2 axles at 4 feet', () => {
            // W = 500 * ((4*2)/(2-1) + 12*2 + 36) = 500 * (8 + 24 + 36) = 500 * 68 = 34,000
            const result = calculateBridgeFormula(4, 2);
            assert.strictEqual(result.formulaWeight, 34000);
            assert.strictEqual(result.maxAllowable, 34000);
        });

        it('caps tandem axle at 34,000 lbs', () => {
            // Large spacing on 2 axles would exceed 34,000 from formula
            const result = calculateBridgeFormula(20, 2);
            assert.ok(result.formulaWeight > 34000);
            assert.strictEqual(result.maxAllowable, 34000);
            assert.strictEqual(result.cap, 'TANDEM_CAP');
        });

        it('caps at 80,000 GVW', () => {
            // Very long truck with many axles could exceed 80,000
            const result = calculateBridgeFormula(60, 7);
            assert.ok(result.formulaWeight > 80000);
            assert.strictEqual(result.maxAllowable, 80000);
            assert.strictEqual(result.cap, 'GVW_CAP');
        });

        it('calculates correctly for 3 axles at 10 feet', () => {
            // W = 500 * ((10*3)/(3-1) + 12*3 + 36) = 500 * (15 + 36 + 36) = 500 * 87 = 43,500
            const result = calculateBridgeFormula(10, 3);
            assert.strictEqual(result.formulaWeight, 43500);
            assert.strictEqual(result.maxAllowable, 43500);
        });

        it('returns null for 0 axles', () => {
            const result = calculateBridgeFormula(10, 0);
            assert.strictEqual(result, null);
        });

        it('handles the standard 5-axle semi at 51 feet', () => {
            const result = calculateBridgeFormula(51, 5);
            // Should be at or near the 80,000 GVW cap
            assert.ok(result.maxAllowable <= 80000);
        });
    });

    describe('analyzeWeight', () => {
        it('analyzes without scale weight', () => {
            const result = analyzeWeight(41, 5);
            assert.strictEqual(result.distanceFeet, 41);
            assert.strictEqual(result.axleCount, 5);
            assert.ok(result.maxAllowable > 0);
            assert.strictEqual(result.scaleWeight, undefined);
        });

        it('detects legal weight', () => {
            const result = analyzeWeight(41, 5, { scaleWeight: 60000 });
            assert.strictEqual(result.isLegal, true);
            assert.strictEqual(result.overweightBy, 0);
            assert.strictEqual(result.needsPermit, false);
            assert.ok(result.remainingCapacity > 0);
        });

        it('detects overweight', () => {
            const result = analyzeWeight(41, 5, { scaleWeight: 80000 });
            // At 41ft/5 axles, max is ~73,500 so 80,000 is over
            assert.strictEqual(result.isLegal, false);
            assert.ok(result.overweightBy > 0);
            assert.strictEqual(result.needsPermit, true);
        });

        it('applies special vehicle cap', () => {
            const normal = analyzeWeight(41, 5);
            const special = analyzeWeight(41, 5, { isSpecialVehicle: true });

            // Special vehicle shouldn't exceed 70,000
            assert.ok(normal.maxAllowable <= 80000);
            if (normal.maxAllowable > 70000) {
                assert.strictEqual(special.maxAllowable, 70000);
            }
        });

        it('matches the AxlEye screenshot scenario', () => {
            // From screenshot: 41 ft, truck on scale weighing 73,271 lbs
            // Permit allows 86,500 lbs
            const result = analyzeWeight(41, 5, { scaleWeight: 73271 });
            // At 41ft/5axles, formula gives ~73,500
            // So 73,271 should be just barely legal
            assert.strictEqual(result.isLegal, true);
            assert.strictEqual(result.overweightBy, 0);
        });

        it('returns null for invalid input', () => {
            const result = analyzeWeight(41, 0);
            assert.strictEqual(result, null);
        });
    });
});
