const bcrypt = require('bcrypt');
const jwt = require('jsonwebtoken');
const { getDb } = require('./db');

const JWT_SECRET = process.env.JWT_SECRET || 'axle-measurement-dev-secret-change-in-prod';
const TOKEN_EXPIRY = '12h';
const SALT_ROUNDS = 10;

function createUser(username, password, role) {
    const db = getDb();
    const hash = bcrypt.hashSync(password, SALT_ROUNDS);
    const stmt = db.prepare('INSERT INTO users (username, password_hash, role) VALUES (?, ?, ?)');
    return stmt.run(username, hash, role);
}

function authenticateUser(username, password) {
    const db = getDb();
    const user = db.prepare('SELECT * FROM users WHERE username = ?').get(username);
    if (!user) return null;
    if (!bcrypt.compareSync(password, user.password_hash)) return null;

    const token = jwt.sign(
        { id: user.id, username: user.username, role: user.role },
        JWT_SECRET,
        { expiresIn: TOKEN_EXPIRY }
    );

    return { token, user: { id: user.id, username: user.username, role: user.role } };
}

function verifyToken(token) {
    try {
        return jwt.verify(token, JWT_SECRET);
    } catch {
        return null;
    }
}

// Express middleware
function requireAuth(req, res, next) {
    const token = req.cookies?.token;
    if (!token) return res.status(401).json({ error: 'Not authenticated' });

    const decoded = verifyToken(token);
    if (!decoded) return res.status(401).json({ error: 'Invalid or expired token' });

    req.user = decoded;
    next();
}

function requireAdmin(req, res, next) {
    requireAuth(req, res, () => {
        if (req.user.role !== 'admin') {
            return res.status(403).json({ error: 'Admin access required' });
        }
        next();
    });
}

function getUsers() {
    const db = getDb();
    return db.prepare('SELECT id, username, role, created_at FROM users').all();
}

function deleteUser(id) {
    const db = getDb();
    return db.prepare('DELETE FROM users WHERE id = ?').run(id);
}

function updateUser(id, updates) {
    const db = getDb();
    if (updates.password) {
        updates.password_hash = bcrypt.hashSync(updates.password, SALT_ROUNDS);
        delete updates.password;
    }
    const fields = Object.keys(updates).filter(k => ['username', 'role', 'password_hash'].includes(k));
    if (fields.length === 0) return null;
    const sets = fields.map(f => `${f} = ?`).join(', ');
    const values = fields.map(f => updates[f]);
    return db.prepare(`UPDATE users SET ${sets} WHERE id = ?`).run(...values, id);
}

module.exports = {
    createUser, authenticateUser, verifyToken,
    requireAuth, requireAdmin,
    getUsers, deleteUser, updateUser,
    JWT_SECRET
};
