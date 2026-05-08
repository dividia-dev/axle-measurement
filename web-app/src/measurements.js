const { getDb } = require('./db');

function saveMeasurement({ line1_px, line2_px, pixel_distance, distance_inches, distance_display, calibration_id, measured_by, notes }) {
    const db = getDb();
    const stmt = db.prepare(`
        INSERT INTO measurements (line1_px, line2_px, pixel_distance, distance_inches, distance_display, calibration_id, measured_by, notes)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    `);
    const result = stmt.run(line1_px, line2_px, pixel_distance, distance_inches, distance_display, calibration_id, measured_by, notes || null);
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

module.exports = { saveMeasurement, getMeasurements, getMeasurement, deleteMeasurement };
