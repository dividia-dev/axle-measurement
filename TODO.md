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

- [ ] **Recessed recal button** — Source a recessed/pen-press momentary switch for
  the D8 recalibration input. Must not be easily bumped.

## Production Readiness

- [ ] **3D print controller enclosure** — Design housing for Pro Micro, 2x joysticks,
  lock switch, recal button, LEDs.

- [ ] **Move from breadboard to PCB/perfboard** — Eliminate breadboard crosstalk
  and connection reliability issues.

## Integration

- [ ] **FDOT permit verification** — Get PAS credentials, try ISA account
- [ ] **FastWeigh webhook parser** — JSON parser alongside existing XML
- [ ] **NVR integration** — Review ScaleWatcher thread architecture
- [ ] **Scale weight integration** — Real-time weight data
