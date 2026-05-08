/**
 * Federal Bridge Formula B / Florida Weight Calculator
 *
 * Based on 23 USC 127 and Florida Statute 316.535
 *
 * Formula: W = 500 * ((L * N) / (N - 1) + 12 * N + 36)
 *
 * Where:
 *   W = maximum weight in pounds (rounded to nearest 500)
 *   L = distance in feet between outer axles of the group
 *   N = number of axles in the group
 */

const WEIGHT_LIMITS = {
    SINGLE_AXLE: 20000,
    TANDEM_AXLE: 34000,
    MAX_GVW: 80000,
    SPECIAL_VEHICLE_GVW: 70000,
};

/**
 * Calculate max allowable weight using the Federal Bridge Formula B.
 * Florida uses this same formula (FL Statute 316.535).
 */
function calculateBridgeFormula(distanceFeet, axleCount) {
    if (axleCount < 1) return null;

    if (axleCount === 1) {
        return {
            formulaWeight: WEIGHT_LIMITS.SINGLE_AXLE,
            maxAllowable: WEIGHT_LIMITS.SINGLE_AXLE,
            cap: 'SINGLE_AXLE',
        };
    }

    const L = distanceFeet;
    const N = axleCount;

    // Bridge Formula B
    const rawWeight = 500 * ((L * N) / (N - 1) + 12 * N + 36);
    const formulaWeight = Math.round(rawWeight / 500) * 500;

    let maxAllowable = formulaWeight;
    let cap = 'FORMULA';

    // Tandem cap
    if (N === 2 && formulaWeight > WEIGHT_LIMITS.TANDEM_AXLE) {
        maxAllowable = WEIGHT_LIMITS.TANDEM_AXLE;
        cap = 'TANDEM_CAP';
    }

    // GVW cap
    if (maxAllowable > WEIGHT_LIMITS.MAX_GVW) {
        maxAllowable = WEIGHT_LIMITS.MAX_GVW;
        cap = 'GVW_CAP';
    }

    return { formulaWeight, maxAllowable, cap };
}

/**
 * Full weight analysis for a measured vehicle.
 *
 * @param {number} distanceFeet - Outer bridge (first to last axle) in feet
 * @param {number} axleCount - Number of axles
 * @param {object} options
 * @param {number} [options.scaleWeight] - Actual weight from scale (lbs)
 * @param {boolean} [options.isSpecialVehicle] - Dump, concrete, waste, fuel truck
 * @returns {object} Analysis result
 */
function analyzeWeight(distanceFeet, axleCount, options = {}) {
    const { scaleWeight = null, isSpecialVehicle = false } = options;

    const result = calculateBridgeFormula(distanceFeet, axleCount);
    if (!result) return null;

    let maxAllowable = result.maxAllowable;
    let cap = result.cap;

    // FL special vehicle cap (dump, concrete, waste, fuel)
    if (isSpecialVehicle && maxAllowable > WEIGHT_LIMITS.SPECIAL_VEHICLE_GVW) {
        maxAllowable = WEIGHT_LIMITS.SPECIAL_VEHICLE_GVW;
        cap = 'SPECIAL_VEHICLE_CAP';
    }

    const analysis = {
        distanceFeet,
        axleCount,
        formulaWeight: result.formulaWeight,
        maxAllowable,
        cap,
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

/**
 * Format weight for display.
 */
function formatWeight(lbs) {
    return lbs.toLocaleString() + ' lbs';
}

module.exports = { WEIGHT_LIMITS, calculateBridgeFormula, analyzeWeight, formatWeight };
