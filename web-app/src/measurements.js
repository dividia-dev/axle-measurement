const { getDb } = require('./db');
const path = require('path');
const fs = require('fs');

const SCREENSHOT_DIR = path.join(__dirname, '..', 'data', 'screenshots');

function saveMeasurement({ line1_px, line2_px, pixel_distance, distance_inches, distance_display, calibration_id, measured_by, notes, axle_count, max_weight_lbs, is_special_vehicle, screenshot }) {
    const db = getDb();

    // Save screenshot to disk if provided
    let screenshotPath = null;
    if (screenshot) {
        if (!fs.existsSync(SCREENSHOT_DIR)) fs.mkdirSync(SCREENSHOT_DIR, { recursive: true });
        const filename = `measurement-${Date.now()}.jpg`;
        screenshotPath = filename;
        const base64Data = screenshot.replace(/^data:image\/\w+;base64,/, '');
        fs.writeFileSync(path.join(SCREENSHOT_DIR, filename), base64Data, 'base64');
    }

    const stmt = db.prepare(`
        INSERT INTO measurements (line1_px, line2_px, pixel_distance, distance_inches, distance_display, calibration_id, measured_by, notes, axle_count, max_weight_lbs, is_special_vehicle, screenshot)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `);
    const result = stmt.run(
        line1_px, line2_px, pixel_distance, distance_inches, distance_display,
        calibration_id, measured_by, notes || null,
        axle_count || null, max_weight_lbs || null, is_special_vehicle ? 1 : 0,
        screenshotPath
    );
    return { id: result.lastInsertRowid };
}

function getMeasurements({ limit = 50, offset = 0 } = {}) {
    const db = getDb();
    const rows = db.prepare('SELECT * FROM measurements ORDER BY id DESC LIMIT ? OFFSET ?').all(limit, offset);
    const count = db.prepare('SELECT COUNT(*) as total FROM measurements').get();
    return { measurements: rows, total: count.total };
}

function getMeasurement(id) {
    const db = getDb();
    return db.prepare('SELECT * FROM measurements WHERE id = ?').get(id) || null;
}

function deleteMeasurement(id) {
    const db = getDb();
    return db.prepare('DELETE FROM measurements WHERE id = ?').run(id);
}

module.exports = { saveMeasurement, getMeasurements, getMeasurement, deleteMeasurement, SCREENSHOT_DIR };
