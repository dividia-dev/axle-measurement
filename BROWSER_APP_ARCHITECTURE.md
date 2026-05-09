# Browser-Based Axle Measurement App — Architecture

## 2026-05-08

## Overview

A browser-based application that displays a live camera feed with two movable
vertical measurement lines. Runs on the same Linux device (NUC/Jetson/any x86_64)
that already runs Spot Decoder. Accessible from any browser on the network.

---

## Spot Decoder Device — What We Learned

Spot Decoder runs on a **generic x86_64 Linux device** (CentOS 7). Not tied to any
specific hardware — could be a NUC, mini PC, or any x86 box. Key facts:

- CentOS 7 / RPM-based deployment
- Framebuffer-based display (direct `/dev/fb0` rendering)
- FFmpeg for RTSP decoding (H.264/H.265)
- Apache + PHP serves the web config UI
- Built-in HTTP server on port 88 (control API, screenshots, overlays)
- Already has overlay capabilities (boxes, labels) via HTTP API
- No GPU/CUDA acceleration — pure software decode

**The same device can host the browser app.** It already runs Apache and has
network access to the camera streams.

---

## Recommended Stack

| Layer | Technology | Why |
|-------|-----------|-----|
| RTSP → Browser | **go2rtc** | Sub-500ms latency, zero transcoding for H.264, single binary, proven at scale |
| Backend | **Node.js + Express** | Fast to develop, great WebSocket support, single language with frontend |
| Database | **SQLite** (better-sqlite3) | Zero ops, file-based, stores users + measurements + calibration |
| Frontend | **Vanilla JS + HTML5 Canvas** | No build step, no framework, simple enough to not need React/Vue |
| Auth | **JWT in HttpOnly cookies** | Stateless, role-based (operator/admin), shift-length sessions |
| Controller Input | **Standard keyboard events** | USB HID controller sends keystrokes, browser captures via `keydown` |

---

## Why This Stack

### go2rtc for Video Streaming

- **Single binary** (~15MB), zero dependencies, runs on any Linux
- Converts RTSP → WebRTC (sub-500ms latency) or MSE (~1s latency)
- **No transcoding needed** if camera outputs H.264 — just repackages
- If camera outputs H.265, can transcode via FFmpeg with hardware acceleration
- Built-in web UI and API on port 1984
- Used in production by Home Assistant/Frigate (thousands of deployments)
- Runs on everything from Raspberry Pi Zero to full servers

### Why NOT Other Streaming Approaches

| Approach | Latency | Verdict |
|----------|---------|---------|
| FFmpeg → HLS | 15-30 seconds | **REJECTED** — unusable for real-time line positioning |
| FFmpeg → JSMpeg | ~50ms | Requires MPEG-1 re-encoding, wastes CPU |
| Janus WebRTC | ~500ms | Overkill — designed for multi-party conferencing |
| MediaMTX | <1s | Strong alternative to go2rtc but more complex |
| Direct RTSP in browser | N/A | **Impossible** — browsers cannot play RTSP |

### Why Vanilla JS Over React/Vue

This app has one page with: a video player, a canvas overlay, a measurement display,
a login form, and a settings panel. React/Vue would add: a build system, node_modules,
JSX compilation, state management, and framework update churn. Vanilla JS with modern
DOM APIs is perfectly adequate and results in zero-build-step deployment — copy files
to server and they work.

### Why Node.js Over Go or Python

- **Go** would work but is slower to develop for a CRUD web app with auth
- **Python (Flask)** adds Python dependency management complexity on embedded device
- **Node.js** gives single-language frontend+backend, excellent WebSocket support,
  rich auth middleware ecosystem

---

## System Architecture

```
                    ┌─────────────────────────────────────────────┐
                    │         Linux Device (NUC / mini PC)        │
                    │                                             │
[RTSP Camera] ─────┤  [go2rtc :1984/:8555]                      │
                    │      │                                      │
                    │      │ WebRTC/MSE                           │
                    │      ▼                                      │
                    │  [Node.js/Express :3000] ──► [SQLite DB]   │
                    │      │                        (users,       │
                    │      │                         measurements,│
                    │      │                         calibration) │
                    │      │                                      │
                    │  [Spot Decoder :88]  (existing, unchanged) │
                    │                                             │
                    └────────────┬────────────────────────────────┘
                                 │ HTTP / WebRTC
                                 │
              ┌──────────────────┼──────────────────┐
              │                  │                  │
    [Operator PC/Tablet]  [Admin Browser]  [Office Monitor]
         + USB HID
         Controller
```

### Port Assignments

| Port | Service | Purpose |
|------|---------|---------|
| 88 | Spot Decoder HTTP API | Existing — camera control, overlays, config |
| 1984 | go2rtc API | Stream management, WebRTC signaling |
| 3000 | Node.js app | Measurement web app (UI, API, auth) |
| 8554 | go2rtc RTSP | RTSP re-publish (optional) |
| 8555 | go2rtc WebRTC | WebRTC UDP media |

---

## Frontend Architecture

### Video + Canvas Overlay

```html
<div id="video-container" style="position: relative;">
  <!-- Live video from go2rtc via WebRTC -->
  <video id="camera-feed" autoplay muted playsinline></video>

  <!-- Canvas overlay — drawn on top of video -->
  <canvas id="overlay"
          style="position: absolute; top: 0; left: 0;
                 width: 100%; height: 100%;
                 pointer-events: none;"></canvas>
</div>
```

- `<video>` receives the WebRTC stream from go2rtc
- `<canvas>` is absolutely positioned on top, same dimensions
- Canvas draws: two vertical lines, distance readout, calibration markers
- `pointer-events: none` passes click events through to video if needed

### Keyboard Input from USB HID Controller

The Pro Micro controller sends standard keyboard scancodes via USB HID.
The browser captures these with standard DOM events:

```javascript
document.addEventListener('keydown', (e) => {
    switch(e.key) {
        // Joystick 1 — Line 1 (left axle)
        case 'a': moveLine(1, -COARSE_STEP); break;  // Left coarse
        case 'd': moveLine(1, +COARSE_STEP); break;  // Right coarse
        case 'q': moveLine(1, -FINE_STEP); break;     // Left fine
        case 'e': moveLine(1, +FINE_STEP); break;     // Right fine

        // Joystick 2 — Line 2 (right axle)
        case 'j': moveLine(2, -COARSE_STEP); break;
        case 'l': moveLine(2, +COARSE_STEP); break;
        case 'u': moveLine(2, -FINE_STEP); break;
        case 'o': moveLine(2, +FINE_STEP); break;
    }
    drawOverlay();
});
```

**Important:** The browser window must be focused to receive keyboard events.
Run Chromium in kiosk mode to prevent accidental window switching:
```
chromium --kiosk --app=http://localhost:3000
```

### Canvas Drawing

```javascript
function drawOverlay() {
    const ctx = overlay.getContext('2d');
    ctx.clearRect(0, 0, overlay.width, overlay.height);

    // Line 1 (left axle) — blue
    ctx.strokeStyle = '#0088FF';
    ctx.lineWidth = 2;
    ctx.setLineDash([]);
    ctx.beginPath();
    ctx.moveTo(line1_x, 0);
    ctx.lineTo(line1_x, overlay.height);
    ctx.stroke();

    // Line 2 (right axle) — blue
    ctx.beginPath();
    ctx.moveTo(line2_x, 0);
    ctx.lineTo(line2_x, overlay.height);
    ctx.stroke();

    // Distance measurement
    if (calibration.isCalibrated) {
        const pixelDist = Math.abs(line2_x - line1_x);
        const inches = pixelDist / calibration.pixelsPerInch;
        const feet = Math.floor(inches / 12);
        const remainInches = Math.round(inches % 12);
        const distText = `${feet}' ${remainInches}"`;

        // Display centered between lines, near top
        const textX = (line1_x + line2_x) / 2;
        ctx.fillStyle = 'rgba(0, 0, 0, 0.7)';
        ctx.fillRect(textX - 60, 10, 120, 40);
        ctx.fillStyle = '#FFFFFF';
        ctx.font = 'bold 28px monospace';
        ctx.textAlign = 'center';
        ctx.fillText(distText, textX, 40);
    }
}
```

---

## Backend API

### Endpoints

```
POST   /api/auth/login          { username, password } → { token, role }
POST   /api/auth/logout         Clears cookie

GET    /api/calibration         Get current calibration data
POST   /api/calibration         Save calibration { ref1_px, ref2_px, distance_inches }

POST   /api/measurements        Save a measurement { line1_px, line2_px, distance, timestamp }
GET    /api/measurements        List recent measurements (with pagination)
GET    /api/measurements/:id    Get specific measurement detail

GET    /api/users               (admin only) List users
POST   /api/users               (admin only) Create user
PUT    /api/users/:id           (admin only) Update user
DELETE /api/users/:id           (admin only) Delete user

GET    /api/settings            Get app settings (key mappings, display preferences)
PUT    /api/settings            (admin only) Update settings
```

### User Roles

| Role | Capabilities |
|------|-------------|
| **operator** | View video, move lines, take measurements, view history |
| **admin** | All operator capabilities + manage users, calibration, settings, export |

---

## go2rtc Configuration

```yaml
# /etc/go2rtc/go2rtc.yaml

streams:
  scale_camera:
    - rtsp://camera-ip:554/stream1

api:
  listen: ":1984"

webrtc:
  listen: ":8555"

log:
  level: info
```

### Browser WebRTC Connection

```javascript
async function connectCamera() {
    const pc = new RTCPeerConnection({
        iceServers: []  // No STUN needed on local network
    });

    pc.addTransceiver('video', { direction: 'recvonly' });

    pc.ontrack = (event) => {
        document.getElementById('camera-feed').srcObject = event.streams[0];
    };

    const offer = await pc.createOffer();
    await pc.setLocalDescription(offer);

    const response = await fetch('/go2rtc/api/webrtc?src=scale_camera', {
        method: 'POST',
        body: offer.sdp
    });

    const answer = await response.text();
    await pc.setRemoteDescription({ type: 'answer', sdp: answer });
}
```

The Node.js app can proxy go2rtc's API (at `/go2rtc/...`) so the browser
only needs to talk to one origin — avoids CORS issues.

---

## Deployment

### Systemd Services

**go2rtc:**
```ini
[Unit]
Description=go2rtc RTSP-to-WebRTC gateway
After=network.target

[Service]
ExecStart=/usr/local/bin/go2rtc -config /etc/go2rtc/go2rtc.yaml
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

**Node.js app:**
```ini
[Unit]
Description=Axle Measurement Web App
After=network.target go2rtc.service

[Service]
WorkingDirectory=/opt/axle-measurement
ExecStart=/usr/bin/node server.js
Restart=always
RestartSec=5
Environment=NODE_ENV=production

[Install]
WantedBy=multi-user.target
```

### Installation Steps

1. Install go2rtc binary → `/usr/local/bin/go2rtc`
2. Create go2rtc config → `/etc/go2rtc/go2rtc.yaml`
3. Install Node.js (v18+ LTS)
4. Deploy app to `/opt/axle-measurement/`
5. Run `npm install --production`
6. Initialize SQLite DB (auto-creates on first run)
7. Create systemd services
8. Enable and start services

### Device Requirements

- Any x86_64 Linux box (the Spot Decoder device works)
- 1GB RAM minimum (go2rtc uses ~20MB, Node.js uses ~50MB)
- Network access to camera RTSP stream
- Network access from operator browsers
- Node.js 18+ LTS

---

## Relationship to Spot Decoder

### Coexistence on Same Device

The measurement app runs **alongside** Spot Decoder, not replacing it:

- Spot Decoder continues to do what it does (multi-stream display, PTZ control)
- The measurement app is a separate service on different ports
- Both can access the same camera RTSP stream simultaneously
- If Spot Decoder adds measurement features natively in the future, the web app
  could be retired or kept as a remote-access alternative

### Potential Integration Points

1. **Spot Decoder's HTTP API** could be used instead of go2rtc for video:
   - The `?screen_shot` endpoint returns JPEG snapshots
   - Polling this at ~5fps could work as a low-fidelity video feed
   - But real-time WebRTC via go2rtc is far superior

2. **Spot Decoder's overlay system** could be driven by the web app:
   - Web app computes line positions, calls Spot Decoder API to draw them
   - This would show the lines on the local framebuffer display AND in the browser
   - Useful if operators need to see lines on a wall-mounted monitor

3. **Scale data integration:**
   - If the scale system has a serial/network interface, the Node.js app can read
     weight data and display it alongside the measurement (like AxlEye does)

---

## Data Model

### SQLite Schema

```sql
-- Users
CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    role TEXT NOT NULL CHECK(role IN ('operator', 'admin')),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Calibration (one active record at a time)
CREATE TABLE calibration (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ref1_px INTEGER NOT NULL,
    ref2_px INTEGER NOT NULL,
    known_distance_inches REAL NOT NULL,
    pixels_per_inch REAL NOT NULL,
    camera_name TEXT,
    calibrated_by TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_active INTEGER DEFAULT 1
);

-- Measurements
CREATE TABLE measurements (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    line1_px INTEGER NOT NULL,
    line2_px INTEGER NOT NULL,
    pixel_distance INTEGER NOT NULL,
    distance_inches REAL NOT NULL,
    distance_display TEXT NOT NULL,
    calibration_id INTEGER REFERENCES calibration(id),
    measured_by TEXT,
    notes TEXT,
    screenshot_path TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- App settings (key-value)
CREATE TABLE settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

### Controller Settings

The `settings` table stores controller configuration as key-value pairs with a
`controller.` prefix. The web app UI (admin-only Settings modal) lets operators
rebind keyboard keys and adjust joystick sensitivity without modifying firmware.

**Settings keys:**
```
controller.keymap.line1_left     (default: 'a')
controller.keymap.line1_right    (default: 'd')
controller.keymap.line2_left     (default: 'j')
controller.keymap.line2_right    (default: 'l')
controller.keymap.toggle_mode    (default: 'f')
controller.keymap.save           (default: ' ')
controller.keymap.reset          (default: 'r')
controller.sensitivity.coarse_step  (default: 0.005)
controller.sensitivity.fine_step    (default: 0.001)
```

**API:**
- `GET /api/settings/controller` — returns merged defaults + saved settings (any authenticated user)
- `PUT /api/settings/controller` — validates and saves (admin only)

**Architecture:**
- The Arduino controller sends fixed keycodes via USB HID (configured via serial/EEPROM)
- The web app maps incoming keycodes to actions using a reverse lookup map
- These are independent systems: if you change Arduino key mappings, update the web app settings to match
- The Controller Test tab in settings shows a joystick diagram that lights up when inputs are detected, useful for verifying dead zone and connectivity

**Demo app:** Uses `sessionStorage` key `axle_controller_settings` instead of the API, with identical UI and behavior.

### Screenshot Capture

When a measurement is saved, the app can capture a screenshot of the current
video frame + overlay as evidence. The Canvas API makes this simple:

```javascript
function captureScreenshot() {
    // Create a composite canvas
    const composite = document.createElement('canvas');
    composite.width = video.videoWidth;
    composite.height = video.videoHeight;
    const ctx = composite.getContext('2d');

    // Draw video frame
    ctx.drawImage(video, 0, 0);

    // Draw overlay on top
    ctx.drawImage(overlay, 0, 0);

    // Export as JPEG
    return composite.toDataURL('image/jpeg', 0.85);
}
```

This screenshot gets stored alongside the measurement record — proof of
what the operator measured, with the lines visible on the image.
