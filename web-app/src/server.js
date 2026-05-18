const express = require('express');
const cookieParser = require('cookie-parser');
const path = require('path');
const { closeDb } = require('./db');
const routes = require('./routes');
const { createUser } = require('./auth');
const { getDb } = require('./db');

const PORT = process.env.PORT || 3000;

function createApp() {
    const app = express();

    app.use(express.json({ limit: '5mb' }));
    app.use(cookieParser());
    app.use(express.static(path.join(__dirname, '..', 'public')));

    // Serve the demo at /demo (so it can use the /api/nvr/auth proxy)
    app.use('/demo', express.static(path.join(__dirname, '..', '..', 'docs', 'demo')));

    app.use('/api', routes);

    // Serve index.html for all non-API routes (SPA-style)
    app.get('*', (req, res) => {
        res.sendFile(path.join(__dirname, '..', 'public', 'index.html'));
    });

    return app;
}

function ensureDefaultAdmin() {
    const db = getDb();
    const userCount = db.prepare('SELECT COUNT(*) as count FROM users').get();
    if (userCount.count === 0) {
        createUser('admin', 'admin', 'admin');
        console.log('Created default admin user (admin/admin) — change this password!');
    }
}

if (require.main === module) {
    const app = createApp();
    ensureDefaultAdmin();

    const server = app.listen(PORT, () => {
        console.log(`Axle Measurement app running at http://localhost:${PORT}`);
        console.log(`Default login: admin / admin`);
    });

    process.on('SIGINT', () => {
        console.log('\nShutting down...');
        server.close();
        closeDb();
        process.exit(0);
    });

    process.on('SIGTERM', () => {
        server.close();
        closeDb();
        process.exit(0);
    });
}

module.exports = { createApp };
