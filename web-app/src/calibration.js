const { getDb } = require('./db');

function saveCalibration({ ref1_norm, ref2_norm, known_distance_inches, inches_per_norm, camera_name, calibrated_by }) {
    const db = getDb();
    const normDist = Math.abs(ref2_norm - ref1_norm);
    if (normDist < 0.001) throw new Error('Reference points are too close together');
    if (known_distance_inches <= 0) throw new Error('Distance must be positive');

    // inches_per_norm: how many real-world inches one normalized unit (0-1) represents
    // This is window-size independent
    const computed_ipn = inches_per_norm || (known_distance_inches / normDist);

    // Deactivate previous calibrations
    db.prepare('UPDATE calibration SET is_active = 0 WHERE is_active = 1').run();

    const stmt = db.prepare(`
        INSERT INTO calibration (ref1_px, ref2_px, known_distance_inches, pixels_per_inch, camera_name, calibrated_by)
        VALUES (?, ?, ?, ?, ?, ?)
    `);

    // Store ref positions as normalized * 10000 (integer column, preserve precision)
    // Store inches_per_norm in the pixels_per_inch column (repurposed)
    const result = stmt.run(
        Math.round(ref1_norm * 10000),
        Math.round(ref2_norm * 10000),
        known_distance_inches,
        computed_ipn,
        camera_name || 'default',
        calibrated_by
    );

    return {
        id: result.lastInsertRowid,
        ref1_norm, ref2_norm,
        known_distance_inches,
        inches_per_norm: computed_ipn,
        camera_name, calibrated_by
    };
}

function getActiveCalibration() {
    const db = getDb();
    const row = db.prepare('SELECT * FROM calibration WHERE is_active = 1 ORDER BY id DESC LIMIT 1').get();
    if (!row) return null;

    // Convert stored values back to normalized format
    return {
        ...row,
        ref1_norm: row.ref1_px / 10000,
        ref2_norm: row.ref2_px / 10000,
        inches_per_norm: row.pixels_per_inch,  // repurposed column
    };
}

function getCalibrationHistory() {
    const db = getDb();
    return db.prepare('SELECT * FROM calibration ORDER BY id DESC LIMIT 50').all();
}

function measureDistance(line1_norm, line2_norm, calibration) {
    if (!calibration || !calibration.inches_per_norm) return null;
    const normDist = Math.abs(line2_norm - line1_norm);
    const distanceInches = normDist * calibration.inches_per_norm;
    return {
        norm_distance: normDist,
        distance_inches: distanceInches,
        distance_display: formatDistance(distanceInches)
    };
}

function formatDistance(inches) {
    const feet = Math.floor(inches / 12);
    const remainingInches = Math.round(inches % 12);
    if (remainingInches === 0) return `${feet} ft`;
    return `${feet} ft ${remainingInches} in`;
}

module.exports = { saveCalibration, getActiveCalibration, getCalibrationHistory, measureDistance, formatDistance };
