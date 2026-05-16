# CV Axle Controller — Custom PCB Specification

> Complete reference for designing and ordering a custom PCB.
> Use this with EasyEDA (free, web-based) → order from JLCPCB.

---

## Board Overview

**Purpose:** Carrier board for Arduino Pro Micro with keyed connectors
for all external components. Replaces perfboard wiring entirely.

**Target size:** ~55mm × 40mm (2-layer, 1.6mm FR4)
**Mounting:** 4× M2 holes matching enclosure standoffs (2mm inset from edges)

---

## Schematic — All Connections

```
                        ┌──────────────────────────┐
                        │     ARDUINO PRO MICRO     │
                        │      (in socket)          │
                        │                           │
                 TXO ───┤1                      12├─── RAW
                 RXI ───┤2                      11├─── GND ★ master
                 GND ───┤3  (do not connect)    10├─── RST
                 GND ───┤4  (do not connect)     9├─── VCC ──┐
           J1_BTN D2 ───┤5                       8├─── A3    │ (freed)
           J2_BTN D3 ───┤6                       7├─── A2 ───┼── J2_SIG
            LOCK  D4 ───┤7                       6├─── A1    │ (freed)
         RGB_RED  D5 ───┤8                       5├─── A0 ───┼── J1_SIG
              D6 ───┤9  ──► R1 220Ω ──► FL1+    4├─── 15    │
              D7 ───┤10 ──► R2 220Ω ──► FL2+    3├─── 14    │
           RECAL  D8 ───┤11                      2├─── 16    │
        RGB_GRN  D9 ───┤12                      1├─── 10    │
                        └──────────────────────────┘         │
                                                              │
    ┌─────────────────────────────────────────────────────────┘
    │
    │   VCC bus ──────┬────────────────┐
    │                 │                │
    │            J1 VCC           J2 VCC
    │
    │   GND bus ──┬───┬───┬───┬───┬───┬───┬───┐
    │             │   │   │   │   │   │   │   │
    │          J1_GND J2_GND FL1- FL2- RGB- LK_G RC_G
    │          (shared (shared
    │          pot+btn) pot+btn)
```

---

## Bill of Materials (BOM)

### On-Board Components (soldered to PCB)

| Ref | Component | Value/Type | Package | Qty | Notes |
|-----|-----------|------------|---------|-----|-------|
| U1  | Female header | 12-pin, 2.54mm | Through-hole | 2 | Pro Micro socket (left + right) |
| R1  | Resistor | 220Ω | 0805 (SMD) or axial TH | 1 | Fine LED 1 current limit |
| R2  | Resistor | 220Ω | 0805 (SMD) or axial TH | 1 | Fine LED 2 current limit |
| J1  | JST-XH header | 4-pin, 2.54mm | Through-hole, right-angle | 1 | Joystick 1 connector |
| J2  | JST-XH header | 4-pin, 2.54mm | Through-hole, right-angle | 1 | Joystick 2 connector |
| J3  | JST-XH header | 2-pin, 2.54mm | Through-hole, right-angle | 1 | Fine LED 1 connector |
| J4  | JST-XH header | 2-pin, 2.54mm | Through-hole, right-angle | 1 | Fine LED 2 connector |
| J5  | JST-XH header | 3-pin, 2.54mm | Through-hole, right-angle | 1 | RGB LED connector |
| J6  | JST-XH header | 2-pin, 2.54mm | Through-hole, right-angle | 1 | Lock toggle connector |
| J7  | JST-XH header | 2-pin, 2.54mm | Through-hole, right-angle | 1 | Recal button connector |

**Total unique parts:** 4 (female headers, 220Ω resistors, JST-XH 4p, 2p, 3p)

### Off-Board Components (connect via JST cables)

| Component | Connector | Cable |
|-----------|-----------|-------|
| JH-D400X-R4 Joystick ×2 | J1, J2 (4-pin JST-XH) | Solder JST pigtail to pot + button |
| 5mm Blue LED ×2 | J3, J4 (2-pin JST-XH) | Solder JST pigtail to LED leads |
| 10mm RGB LED ×1 | J5 (3-pin JST-XH) | Solder JST pigtail to red, green, cathode |
| SPST Toggle Switch ×1 | J6 (2-pin JST-XH) | Solder JST pigtail to terminals |
| Tact Switch ×1 | J7 (2-pin JST-XH) | Solder JST pigtail to legs |
| USB-C Breakout ×1 | N/A | USB-C cable directly to Pro Micro port |

---

## Connector Pinouts

All JST-XH, 2.54mm pitch. Pin 1 is marked on silk screen.

### J1 — Joystick 1 (4-pin)

| Pin | Signal | Pro Micro Pin | Notes |
|-----|--------|---------------|-------|
| 1   | SIG    | A0            | X-axis pot wiper (center pin) |
| 2   | VCC    | VCC (5V)      | Pot right outer pin |
| 3   | GND    | GND           | Pot left outer + button GND (shared wire) |
| 4   | BTN    | D2            | Button signal (INPUT_PULLUP) |

### J2 — Joystick 2 (4-pin)

| Pin | Signal | Pro Micro Pin | Notes |
|-----|--------|---------------|-------|
| 1   | SIG    | A2            | X-axis pot wiper (center pin) |
| 2   | VCC    | VCC (5V)      | Pot right outer pin |
| 3   | GND    | GND           | Pot left outer + button GND (shared wire) |
| 4   | BTN    | D3            | Button signal (INPUT_PULLUP) |

### J3 — Fine LED 1 (2-pin)

| Pin | Signal | Notes |
|-----|--------|-------|
| 1   | LED+   | Anode (from R1 220Ω, driven by D6) |
| 2   | LED−   | Cathode to GND |

### J4 — Fine LED 2 (2-pin)

| Pin | Signal | Notes |
|-----|--------|-------|
| 1   | LED+   | Anode (from R2 220Ω, driven by D7) |
| 2   | LED−   | Cathode to GND |

### J5 — RGB Status LED (3-pin)

| Pin | Signal | Pro Micro Pin | Notes |
|-----|--------|---------------|-------|
| 1   | RED    | D5            | Red anode (built-in resistor in LED) |
| 2   | GRN    | D9            | Green anode (built-in resistor in LED) |
| 3   | GND    | GND           | Common cathode |

### J6 — Lock Toggle (2-pin)

| Pin | Signal | Pro Micro Pin | Notes |
|-----|--------|---------------|-------|
| 1   | SIG    | D4            | Toggle terminal 1 |
| 2   | GND    | GND           | Toggle terminal 2 |

### J7 — Recal Button (2-pin)

| Pin | Signal | Pro Micro Pin | Notes |
|-----|--------|---------------|-------|
| 1   | SIG    | D8            | Tact switch leg 1 |
| 2   | GND    | GND           | Tact switch leg 2 |

---

## PCB Layout Guidelines

### Board Dimensions & Mounting

```
         ~55mm
    ┌──────────────────┐
    │○              ○│  ← M2 mounting holes, 2mm from edges
    │                  │
    │                  │  ~40mm
    │                  │
    │○              ○│
    └──────────────────┘
```

- 2-layer board, 1.6mm FR4, HASL or ENIG finish
- Bottom layer: GND fill (ground plane)
- Top layer: signal traces + VCC trace
- Trace width: 0.3mm for signals, 0.5mm for power (VCC, GND)

### Component Placement (top view)

```
    ┌──────────────────────────────────────────────┐
    │  BACK                                         │
    │  ┌J5─┐  ┌J7─┐                                │
    │  │RGB│  │RCL│                                │
    │  └───┘  └───┘                                │
    │                                               │
    │ ┌J3─┐  ┌────── PRO MICRO ──────┐  ┌J4─┐    │
    │ │FL1│  │ [====================] │  │FL2│    │
    │ └───┘  │ [====================] │  └───┘    │
    │        └────────────────────────┘            │
    │   [R1]                            [R2]       │
    │                                               │
    │ ┌J1──┐   ┌J6─┐                  ┌J2──┐      │
    │ │JOY1│   │LCK│                  │JOY2│      │
    │ └────┘   └───┘                  └────┘      │
    │  FRONT                                        │
    └──────────────────────────────────────────────┘
```

- **J1 (Joystick 1):** front-left edge — wires drop straight down from J1
- **J2 (Joystick 2):** front-right edge — wires drop straight from J2
- **J3 (Fine LED 1):** left side — near J1
- **J4 (Fine LED 2):** right side — near J2
- **J5 (RGB LED):** back-left — near center top panel position
- **J6 (Lock Toggle):** front-center — near toggle position
- **J7 (Recal):** back-right — near recal pinhole position
- **R1, R2:** between Pro Micro and their respective LED connectors
- **Pro Micro:** centered, USB end toward back

### Right-Angle Connectors

Use **right-angle** JST-XH headers so wires exit horizontally toward
the enclosure walls, not vertically (would hit joystick bases above).

### Silkscreen Labels

Print on silk layer (white text on green/black solder mask):

- Each connector: "J1 JOYSTICK 1", "J2 JOYSTICK 2", etc.
- Pin 1 dots on all connectors
- Connector pinout: "SIG VCC GND BTN" next to J1/J2
- Board title: "CV AXLE CONTROLLER v1.0"
- "DIVIDIA" branding
- Pin 1 orientation arrow for Pro Micro: "← USB THIS END"
- Resistor values: "220Ω"

---

## Design Rules (for EasyEDA / KiCad)

| Parameter | Value |
|-----------|-------|
| Min trace width | 0.25mm (10mil) |
| Min clearance | 0.2mm (8mil) |
| Via size | 0.6mm hole, 1.0mm pad |
| Board thickness | 1.6mm |
| Copper weight | 1oz |
| Solder mask | Green (or black) |
| Silkscreen | White |
| Surface finish | HASL (cheapest) or ENIG (nicer) |
| Min hole size | 0.3mm |
| Layers | 2 |

---

## How to Order

### Option A: EasyEDA → JLCPCB (easiest)

1. Go to https://easyeda.com — create free account
2. New Project → draw schematic using this spec
3. Convert to PCB layout → place components, route traces
4. Click "Fabrication" → "Order at JLCPCB"
5. 5 boards, ~$2 + ~$6 shipping = **~$8 total**
6. Arrives in 5-7 days (standard) or 2-3 days (express)

### Option B: KiCad → Any manufacturer

1. Download KiCad (free) — https://www.kicad.org
2. Draw schematic, create PCB layout
3. Export Gerber files
4. Upload to JLCPCB, PCBWay, or OSH Park

### Option C: Hand it to someone

Take this spec document to any PCB designer on Fiverr/Upwork.
A board this simple should cost $20-50 for the design.
They'll give you Gerber files ready to upload to JLCPCB.

---

## JST-XH Pigtail Wiring Guide

For each component, solder a JST-XH pigtail cable (available pre-made
on Amazon, or crimp your own with a JST-XH crimping kit).

| Component | JST Pigtail | Wire Connections |
|-----------|-------------|-----------------|
| Joystick 1 | 4-pin | Pin1→pot center, Pin2→pot right, Pin3→pot left + btn GND, Pin4→btn signal |
| Joystick 2 | 4-pin | Same as J1 |
| Fine LED 1 | 2-pin | Pin1→anode (long leg), Pin2→cathode (short leg) |
| Fine LED 2 | 2-pin | Same as Fine LED 1 |
| RGB LED | 3-pin | Pin1→red leg, Pin2→green leg, Pin3→cathode (longest leg) |
| Lock toggle | 2-pin | Pin1→terminal 1, Pin2→terminal 2 |
| Recal button | 2-pin | Pin1→leg 1, Pin2→leg 2 |

---

## Notes

- **Left-side GND pins (pins 3, 4) on Pro Micro are NOT connected on the PCB.**
  Only the right-side GND (pin 11) is used. The PCB fixes the clone board bad-solder issue.
- **A1 and A3 are freed** (twist pots removed). Pads are exposed on the PCB
  but not routed. Future expansion headers could be added.
- **USB-C connection** is still a cable from panel breakout to Pro Micro's own port.
  The PCB does not carry USB data.
- **Ground plane** on bottom layer provides clean grounding and EMI shielding.
  All GND pins on connectors are via-stitched to the bottom plane.
