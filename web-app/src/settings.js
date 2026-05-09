const { getDb } = require('./db');

// Default controller settings
const DEFAULTS = {
    keymap: {
        line1_left: 'a',
        line1_right: 'd',
        line2_left: 'j',
        line2_right: 'l',
        toggle_mode: 'f',
        save: ' ',
        reset: 'r'
    },
    sensitivity: {
        coarse_step: 0.005,
        fine_step: 0.001
    }
};

const SENSITIVITY_LIMITS = {
    coarse_step: { min: 0.001, max: 0.02 },
    fine_step: { min: 0.0002, max: 0.005 }
};

function getControllerSettings() {
    const db = getDb();
    const rows = db.prepare("SELECT key, value FROM settings WHERE key LIKE 'controller.%'").all();

    // Start with defaults
    const settings = JSON.parse(JSON.stringify(DEFAULTS));

    // Override with saved values
    for (const row of rows) {
        const parts = row.key.replace('controller.', '').split('.');
        if (parts[0] === 'keymap' && parts[1] in settings.keymap) {
            settings.keymap[parts[1]] = row.value;
        } else if (parts[0] === 'sensitivity' && parts[1] in settings.sensitivity) {
            settings.sensitivity[parts[1]] = parseFloat(row.value);
        }
    }

    return settings;
}

function saveControllerSettings(settings) {
    const errors = validateControllerSettings(settings);
    if (errors.length > 0) {
        return { ok: false, errors };
    }

    const db = getDb();
    const upsert = db.prepare(
        "INSERT INTO settings (key, value, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP) " +
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = CURRENT_TIMESTAMP"
    );

    const saveAll = db.transaction(() => {
        for (const [action, key] of Object.entries(settings.keymap)) {
            upsert.run(`controller.keymap.${action}`, key);
        }
        for (const [param, value] of Object.entries(settings.sensitivity)) {
            upsert.run(`controller.sensitivity.${param}`, String(value));
        }
    });

    saveAll();
    return { ok: true, settings: getControllerSettings() };
}

function validateControllerSettings(settings) {
    const errors = [];

    if (!settings || !settings.keymap || !settings.sensitivity) {
        return ['Missing required fields: keymap, sensitivity'];
    }

    // Validate all keymap actions exist
    for (const action of Object.keys(DEFAULTS.keymap)) {
        if (!(action in settings.keymap)) {
            errors.push(`Missing key mapping: ${action}`);
        }
    }

    // Validate key values are single characters or known specials
    const validSpecials = [' ', 'escape', 'enter', 'tab', 'backspace', 'delete',
        'arrowup', 'arrowdown', 'arrowleft', 'arrowright',
        'f1', 'f2', 'f3', 'f4', 'f5', 'f6', 'f7', 'f8', 'f9', 'f10', 'f11', 'f12',
        'f13', 'f14', 'f15', 'f16', 'f17', 'f18', 'f19', 'f20', 'f21', 'f22', 'f23', 'f24'];

    const usedKeys = new Map();
    for (const [action, key] of Object.entries(settings.keymap || {})) {
        if (typeof key !== 'string' || key.length === 0) {
            errors.push(`Invalid key for ${action}: must be a non-empty string`);
            continue;
        }
        const normalizedKey = key.toLowerCase();
        if (normalizedKey.length > 1 && !validSpecials.includes(normalizedKey)) {
            errors.push(`Invalid key for ${action}: "${key}" is not a recognized key`);
        }
        if (usedKeys.has(normalizedKey)) {
            errors.push(`Duplicate key "${key}": already used for ${usedKeys.get(normalizedKey)}`);
        }
        usedKeys.set(normalizedKey, action);
    }

    // Validate sensitivity values
    for (const [param, limits] of Object.entries(SENSITIVITY_LIMITS)) {
        const value = settings.sensitivity?.[param];
        if (value === undefined || value === null) {
            errors.push(`Missing sensitivity value: ${param}`);
        } else if (typeof value !== 'number' || isNaN(value)) {
            errors.push(`Invalid sensitivity value for ${param}: must be a number`);
        } else if (value < limits.min || value > limits.max) {
            errors.push(`${param} must be between ${limits.min} and ${limits.max}`);
        }
    }

    return errors;
}

module.exports = { getControllerSettings, saveControllerSettings, DEFAULTS };
