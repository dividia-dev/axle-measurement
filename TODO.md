# TODO

## UI/UX

- [ ] **User-configurable line/handle colors** — Let operators customize line colors
  for measurement mode, calibration mode, and drag handles via Settings. Include
  a live preview of changes, reset-to-defaults, and undo. Important for varying
  lighting environments (outdoor sun, indoor fluorescent, night shifts).

## Hardware

- [ ] **Lock switch LED** — Wire dual-color red/green LED for lock indicator.
  Green = active/unlocked, Red = locked/inactive. Needs two pins or bicolor LED.
  Firmware currently uses D5 for single lock LED.

- [ ] **Disable twist barrel physically** — Prevent customer confusion by locking
  or removing the twist barrel rotation on the JH-D400X-R4 joysticks. Fine mode
  is now button-toggled, twist is unused.

- [ ] **Migrate from HID Keyboard to HID Consumer Device** — Currently the
  controller sends regular keyboard characters (a, d, j, l, etc.) which can
  interfere with other apps if a joystick is bumped while typing. Switching to
  USB HID Consumer/Vendor-defined usage pages sends custom control codes that
  no other software recognizes. Requires firmware rewrite of the HID descriptor
  and web app migration to the WebHID API (or custom HID event parsing).
  Lock switch is the current mitigation.

- [ ] **Recessed recal button** — Source a recessed/pen-press momentary switch for
  the D8 recalibration input. Must not be easily bumped.

## Production Readiness

- [x] **Design controller enclosure** — Full component spec, top/back/bottom/side panel
  layouts approved. See `docs/enclosure-component-spec.md` and layout HTML files.

- [x] **3D print controller enclosure** — OpenSCAD model generated, STLs exported.
  Test print in progress on Bambu Lab A1, 0.4mm nozzle.

- [x] **Vent dust protection** — Eliminated vents entirely. Heat generation <1W,
  sealed enclosure is fine. Cleaner look, no dust ingress in cement facility.

- [ ] **Steel ballast plate** — ~170x85x3mm steel plate in bottom shell for weight
  (~350g). Print recessed pocket to hold it. Prevents controller sliding during use.

- [ ] **Upgraded rubber feet / perimeter grip strip** — Replace 13x13mm corner pads
  with larger feet (20mm+) or continuous rubber strip around bottom perimeter.
  Combined with steel plate for maximum grip.

- [ ] **Move from breadboard to PCB/perfboard** — Eliminate breadboard crosstalk
  and connection reliability issues. 50x70mm perfboard on hand.

## Integration

- [ ] **FDOT permit verification** — Get PAS credentials, try ISA account
- [ ] **FastWeigh webhook parser** — JSON parser alongside existing XML
- [ ] **NVR integration** — Review ScaleWatcher thread architecture
- [ ] **Scale weight integration** — Real-time weight data
