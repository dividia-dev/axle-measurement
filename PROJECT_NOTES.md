# CV Axle Controller — Project Notes

## Overview
Physical control device for measuring the distance between truck axles on a weighing scale. Operators use two joysticks to position vertical measurement lines over a live camera feed. The controller sends keystrokes over USB — the computer interprets them as left/right line movement commands.

## Environment
- Industrial aggregate/cement facility, indoors
- Operators may vary in age — controls must be robust, intuitive, easy to use
- Accuracy is the top priority ("best accuracy they can get")
- Trucks may not be centered on the scale, so both lines must be independently positionable

## Hardware Requirements

### Controller Core
- **Arduino Pro Micro (ATmega32U4)** or Leonardo — native USB HID keyboard emulation
- User has Arduino units on hand but needs to verify which models
- User has a 3D printer for custom enclosure

### Input — 2 Joysticks
- One joystick per measurement line (left axle line, right axle line)
- Movement restricted to **left/right axis only** (ignore Y-axis input)
- Joystick chosen over slider for: industrial durability, center-return spring, glove-friendly, harder to accidentally bump
- Consider joysticks with rotary barrel (potentiometer ring) for built-in fine control

### Movement Modes — Two Options (User-Configurable)
1. **Proportional mode (default):** Deflection distance from center = keystroke repeat speed. Small push = slow/fine, big push = fast/coarse.
2. **Discrete toggle mode:** Button switches between coarse and fine modes. LED indicates current mode.
- Let the operator choose which mode they prefer
- Stored in EEPROM so preference survives power cycles

### Safety / Lock
- Physical toggle switch (not momentary) to disable all input
- LED indicator: green = active, off = locked
- Prevents accidental movement if controller is bumped

### LED Indicators
- Lock status LED (green = active)
- Mode LED (if discrete toggle mode is active): e.g., blue = fine mode
- Per-joystick or shared — TBD based on enclosure design

### Phase 2 — Configurable Key Mapping
- Small OLED display (SSD1306 128x64) + rotary encoder for menu navigation
- Configure which keystrokes each joystick direction sends
- Store mappings in EEPROM

## Software / Integration Context
- Existing product: **Spot Decoder** — a PTZ camera control system that accepts keystrokes from a keypad and sends PTZ commands. Repo exists in `dev_projects/`.
- The overlay/measurement feature (painting lines on live video, calculating distance) has **NOT yet been implemented** — that is separate from this controller project
- Measurement likely based on known reference distance (stakes or known measurement in frame)
- This controller project focuses solely on the physical device + USB HID keystroke output
- Target keystrokes TBD — need to check Spot Decoder repo for existing key mappings

## Signal Flow
```
Joystick analog input → Arduino ADC read
  → Deadzone filtering
  → Movement speed calculation
  → HID Keyboard.press() / Keyboard.release()
  → USB → Computer receives keystrokes
  → Software maps keystrokes to line movement (separate project)
```

## Display / Config Screen
- User has 2 digit displays but no 2-line OLED screen
- Not needed for v1 — configuration done via serial monitor commands
- Phase 2: add OLED (SSD1306 128x64) + rotary encoder for on-device config

## Parts Status (2026-05-08)
- [x] Breadboard, jumper wires, LEDs, resistors, toggle switches, pushbuttons — ON HAND
- [x] Thumbstick modules — ON HAND (no usable button contacts)
- [ ] JH-D400X-R4 4-axis twist joysticks (x2) — ORDERED
- [ ] Arduino Pro Micro ATmega32U4 — ORDERED (multiple)
- [ ] Micro-USB cable — user has these on hand
- Arduino Nano (ATmega328P) — ON HAND, not usable (no native USB HID)
- ESP8266 NodeMCU — ON HAND, not usable (no USB HID, only 1 analog pin)
- Arduino Uno R3 — ON HAND, not usable (no USB HID, no USB-B cable available)

## Open Items
- [ ] Check Spot Decoder repo for existing keystroke mappings — DONE, see SPOT_DECODER_ANALYSIS.md
- [ ] Determine default keystroke assignments for v1 — DONE, defaults in firmware
- [ ] Prototype on breadboard when Pro Micros arrive
- [ ] Test proportional vs discrete mode with operators to determine preference
- [ ] Design 3D-printed enclosure after physical parts are in hand

## Software Components Built

### 1. Arduino Firmware (firmware/)
- `axle_controller_uno/` — Prototype firmware for Uno (serial output)
- `axle_controller_pro_micro/` — Production firmware with native USB HID
- `joystick_diagnostic/` — Diagnostic sketch for identifying joystick wiring
- Both support proportional + discrete toggle modes, EEPROM key mapping, serial config

### 2. Serial Bridge (bridge/)
- `serial_keyboard_bridge.py` — Python script for Uno prototype (serial → keystrokes)
- Not needed with Pro Micro (native USB HID)

### 3. Web Application (web-app/)
- **Backend:** Node.js + Express + SQLite
- **Frontend:** Vanilla JS + HTML5 Canvas overlay
- **Auth:** JWT with operator/admin roles
- **Features implemented:**
  - Login/logout with role-based access
  - Live video display (test video for dev, WebRTC via go2rtc for production)
  - Two movable vertical measurement lines on canvas overlay
  - Keyboard input for line movement (A/D/Q/E for line 1, J/L/U/O for line 2)
  - Calibration workflow (two-point reference with known distance)
  - Distance measurement calculation and display
  - Federal Bridge Formula B / FL Statute 316.535 weight calculator
  - Axle count input with +/- buttons
  - Special vehicle (dump/concrete/waste/fuel) weight cap
  - Max allowable weight display, legal/overweight status
  - Measurement history with persistence
  - 50 unit tests, all passing

### 4. Research Documents
- `SPOT_DECODER_ANALYSIS.md` — Full analysis of existing Spot Decoder codebase
- `CAMERA_DISTANCE_MEASUREMENT_RESEARCH.md` — Overhead camera measurement (superseded)
- `CAMERA_MEASUREMENT_SIDEVIEW.md` — Side-view camera measurement (correct approach)
- `AUTO_CALIBRATION_MARKERS.md` — V2 auto-calibration with colored markers on guide rail
- `BROWSER_APP_ARCHITECTURE.md` — go2rtc + Node.js + Canvas architecture design

## Design Decisions Log
- **2026-05-08:** Joystick over slider — industrial durability, center-return spring, glove-friendly, better for varying operator ages
- **2026-05-08:** Support both proportional and discrete movement modes, let user configure preference
- **2026-05-08:** Arduino Pro Micro (ATmega32U4) recommended for native USB HID — avoid Uno/Mega
- **2026-05-08:** 3D printed custom enclosure planned
