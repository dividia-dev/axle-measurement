const { getDb } = require('./db');

function saveCalibration({ ref1_px, ref2_px, known_distance_inches, camera_name, calibrated_by }) {
    const db = getDb();
    const pixelDist = Math.abs(ref2_px - ref1_px);
    if (pixelDist === 0) throw new Error('Reference points cannot be at the same pixel');
    if (known_distance_inches <= 0) throw new Error('Distance must be positive');

    const pixels_per_inch = pixelDist / known_distance_inches;

    // Deactivate previous calibrations
    db.prepare('UPDATE calibration SET is_active = 0 WHERE is_active = 1').run();

    const stmt = db.prepare(`
        INSERT INTO calibration (ref1_px, ref2_px, known_distance_inches, pixels_per_inch, camera_name, calibrated_by)
        VALUES (?, ?, ?, ?, ?, ?)
    `);

    const result = stmt.run(ref1_px, ref2_px, known_distance_inches, pixels_per_inch, camera_name || 'default', calibrated_by);
    return {
        id: result.lastInsertRowid,
        ref1_px, ref2_px, known_distance_inches, pixels_per_inch, camera_name, calibrated_by
    };
}

function getActiveCalibration() {
    const db = getDb();
    return db.prepare('SELECT * FROM calibration WHERE is_active = 1 ORDER BY id DESC LIMIT 1').get() || null;
}

function getCalibrationHistory() {
    const db = getDb();
    return db.prepare('SELECT * FROM calibration ORDER BY id DESC LIMIT 50').all();
}

function measureDistance(line1_px, line2_px, calibration) {
    if (!calibration) return null;
    const pixelDist = Math.abs(line2_px - line1_px);
    const distanceInches = pixelDist / calibration.pixels_per_inch;
    return {
        pixel_distance: pixelDist,
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
