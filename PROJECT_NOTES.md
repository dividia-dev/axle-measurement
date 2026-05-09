# CV Axle Controller — Project Notes

## Overview
Physical control device + browser-based web application for measuring the distance
between truck axles on a weighing scale. Replaces competitor product **AxlEye**.

Operators use two joysticks (or keyboard) to position vertical measurement lines
over a side-view live camera feed, measuring the outer bridge distance between the
first and last axle. The system calculates max allowable gross weight per the
Federal Bridge Formula B (23 USC 127 / FL Statute 316.535).

Used for **Florida DOT oversize/overweight blanket permits** at an industrial
aggregate/cement facility (Central Florida Transport, Coleman FL).

## Environment
- Industrial aggregate/cement facility, indoors
- Operators may vary in age — controls must be robust, intuitive, easy to use
- Camera is a **side view** of the truck on the scale (not overhead)
- Trucks may not be centered on the scale, so both lines must be independently positionable

## Accuracy Analysis

### Measurement Precision
All measurement math uses **normalized coordinates (0-1)** relative to the video frame.
This is completely independent of window size, browser resize, or display resolution.
The software introduces zero measurement error.

### Physical Accuracy Limits

| Factor | Impact | Notes |
|--------|--------|-------|
| Camera resolution (1080p, 60ft FOV) | 1 pixel = 0.375 inches | Hard physics limit |
| Camera resolution (4K, 60ft FOV) | 1 pixel = 0.19 inches | 2x better |
| Operator line placement | +/- 2-3 pixels typical | Depends on image clarity |
| Calibration stake precision | Scales all measurements | Measure carefully |
| Lens distortion | 1-2% at edges | Negligible in center 70% of frame |

### Expected Total Accuracy
- **1080p camera: +/- 2-3 inches** (typical), +/- 4 inches (worst case at frame edges)
- **4K camera: +/- 1-2 inches** (typical)
- Error is in **inches, not feet** — well within bridge formula requirements
- Bridge formula uses whole-foot distances; +/- 6 inches doesn't change the weight calculation

### Why This Is Sufficient
The Federal Bridge Formula rounds distance to whole feet. The difference between
41 ft and 41 ft 3 in produces the same max weight. You'd need to be off by 6+
inches to potentially shift the result by one foot, and even then the weight
difference is small (~840 lbs per foot at typical configurations).

## Hardware

### Controller Core
- **Arduino Pro Micro (ATmega32U4)** — native USB HID keyboard emulation
- Sends standard keystrokes over USB — computer sees it as a keyboard
- No drivers needed, works with any OS

### Input — 2 Joysticks (JH-D400X-R4)
- 4-axis joystick with twist barrel (X, Y, Z rotation, pushbutton)
- X-axis: left/right line movement
- Twist barrel: fine adjustment (twist-locks-X prevents accidental coarse movement)
- Pushbutton: sends 'F' key to toggle coarse/fine mode in web app
- 10K potentiometers, standard analog read on Arduino ADC

### Movement Modes
- **Coarse/Fine toggle:** Press F (or joystick button) to switch modes
- On-screen indicator shows current mode (blue = COARSE, amber = FINE)
- Both modes available via single A/D or J/L keys — no separate fine keys needed

### Safety / Lock
- Physical toggle switch to disable all input
- LED indicator: green = active, off = locked

### Phase 2 — On-Device Config
- Small OLED display (SSD1306 128x64) + rotary encoder for menu navigation
- Configure keystroke mappings without a computer
- Currently configured via serial monitor commands

## Parts Status (2026-05-08)
- [x] Breadboard, jumper wires, LEDs, resistors, toggle switches, pushbuttons — ON HAND
- [x] Thumbstick modules — ON HAND (no usable button contacts on these units)
- [x] Micro-USB cables — ON HAND
- [ ] JH-D400X-R4 4-axis twist joysticks (x2) — ORDERED
- [ ] Arduino Pro Micro ATmega32U4 (multiple) — ORDERED
- Arduino Nano (ATmega328P) — ON HAND, not usable (no native USB HID)
- ESP8266 NodeMCU — ON HAND, not usable (no USB HID, only 1 analog pin)
- Arduino Uno R3 — ON HAND, not usable (no USB HID, no USB-B cable)

## Software Components

### 1. Arduino Firmware (firmware/)
- `axle_controller_pro_micro/` — Production firmware with native USB HID
  - Proportional + discrete toggle modes, twist-locks-X
  - EEPROM key mapping (survives power cycles)
  - Serial config interface (HELP, KEYS, SET, MODE, DEFAULTS commands)
  - Default: A/D = line 1, J/L = line 2, F = toggle mode
- `axle_controller_uno/` — Prototype firmware for Uno (serial output)
- `joystick_diagnostic/` — Diagnostic sketch for wiring identification

### 2. Serial Bridge (bridge/)
- `serial_keyboard_bridge.py` — Python script for Uno prototype (serial → keystrokes)
- Not needed with Pro Micro (native USB HID)

### 3. Web Application (web-app/)
- **Backend:** Node.js + Express + SQLite (better-sqlite3)
- **Frontend:** Vanilla JS + HTML5 Canvas overlay (no build step)
- **Auth:** JWT in HttpOnly cookies, operator/admin roles
- **Video:** Test video for dev, go2rtc WebRTC for production
- **51 unit tests, all passing**

#### Features
- Login/logout with role-based access (operator: measure, admin: calibrate + manage)
- Live video with two movable vertical measurement lines
- Lines have black outline for visibility on light and dark backgrounds
- Coarse/Fine movement toggle (F key) with on-screen indicator
- Keyboard input: A/D = line 1, J/L = line 2, F = toggle, Space = save, R = reset
- Calibration: position lines on reference markers, enter feet + inches
- Calibration mode: tomato border + tomato lines for visual distinction
- Distance measurement in feet and inches
- **Federal Bridge Formula B weight calculator** (23 USC 127 / FL Statute 316.535)
  - Axle count +/- input (2-9 axles)
  - Dump/Mix checkbox for FL special vehicle 70,000 lb cap
  - Max allowable weight display
  - Legal/overweight status when scale weight is available
- Measurement history with SQLite persistence
- Window-size-independent measurements (normalized coordinates)

### 4. Research Documents
- `SPOT_DECODER_ANALYSIS.md` — Full analysis of existing Spot Decoder codebase
- `CAMERA_MEASUREMENT_SIDEVIEW.md` — Side-view camera measurement approach
- `CAMERA_DISTANCE_MEASUREMENT_RESEARCH.md` — Overhead analysis (superseded by sideview)
- `AUTO_CALIBRATION_MARKERS.md` — V2 auto-calibration with colored markers on guide rail
- `BROWSER_APP_ARCHITECTURE.md` — go2rtc + Node.js + Canvas architecture

## System Architecture (Production)

```
[RTSP Camera] → [go2rtc :1984] → WebRTC → [Browser <video>]
                                                ↓
                                  [Canvas overlay — 2 measurement lines]
                                                ↓
                                  [Node.js/Express :3000]
                                                ↓
                                  [SQLite — users, calibration, measurements]

[USB HID Controller] → keystrokes → [Browser keydown events] → line movement
```

Runs on the same Linux device as Spot Decoder (generic x86_64, CentOS 7).
go2rtc + Node.js as systemd services alongside existing Spot Decoder.

## Calibration Procedure

1. Install two permanent reference markers on the near-side guide rail
2. Precisely measure the distance between them (feet + inches)
3. In the web app, admin clicks "Calibrate"
4. Video border turns tomato, lines turn tomato
5. Position Line 1 over reference marker 1
6. Position Line 2 over reference marker 2
7. Enter the known distance (feet + inches)
8. Click "Save" — system computes inches_per_normalized_unit
9. All future measurements use this calibration (persisted in SQLite)
10. Recalibrate only if camera moves, zooms, or is replaced

## Bridge Formula (Federal / Florida)

```
W = 500 × ((L × N) / (N - 1) + 12N + 36)

W = max allowable gross weight (lbs, rounded to nearest 500)
L = outer bridge distance (feet) — what we measure
N = number of axles — operator enters this
```

**Caps:** Single axle 20,000 lbs | Tandem 34,000 lbs | GVW 80,000 lbs
**FL Special vehicles** (dump, concrete, waste, fuel): 70,000 lbs GVW cap

## Open Items
- [ ] Prototype controller on breadboard when Pro Micros + joysticks arrive
- [ ] Test coarse/fine toggle with operators to determine preferred sensitivity
- [ ] Design 3D-printed enclosure after physical parts validated
- [ ] Scale weight integration (read from scale, show legal/overweight status)
- [ ] go2rtc setup for production RTSP streaming
- [ ] Deploy web app to Spot Decoder device
- [ ] V2: auto-calibration with colored markers on guide rail
- [ ] V2: on-device OLED config screen
- [ ] Verify bridge formula against customer's actual FL DOT permits

## Design Decisions Log
- **2026-05-08:** Joystick over slider — industrial durability, center-return spring, glove-friendly
- **2026-05-08:** JH-D400X-R4 twist joystick selected — coarse (stick) + fine (twist) in one unit
- **2026-05-08:** Arduino Pro Micro (ATmega32U4) for native USB HID
- **2026-05-08:** Browser-based web app over native Windows app — any device, remote access, multi-user
- **2026-05-08:** go2rtc for RTSP-to-WebRTC (sub-500ms latency, no transcoding)
- **2026-05-08:** Side-view camera with two-point linear calibration (simple, accurate)
- **2026-05-08:** Normalized coordinates for window-size-independent measurements
- **2026-05-08:** Coarse/fine as single toggle key (F) with on-screen indicator
- **2026-05-08:** Feet + inches input for calibration (not just feet or just inches)
- **2026-05-08:** Lines with black outline for visibility on any background
- **2026-05-08:** Tomato color scheme for calibration mode visual distinction

## Git Log
- `2665401` — Initial commit: firmware, web app, research docs (50 tests)
- `ba52cd5` — UI improvements: calibration mode, line visibility, feet+inches
- `bafd1df` — Add coarse/fine toggle with on-screen indicator
- `6061adf` — Fix: normalized coordinates for window-size-independent measurements (51 tests)
