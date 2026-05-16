# EasyEDA PCB Design Walkthrough — CV Axle Controller

> Follow these steps exactly in EasyEDA (https://easyeda.com).
> Each step tells you what to search for, where to place it, and how to wire it.

---

## Step 0 — Create Project

1. Go to https://easyeda.com — sign in (or create free account)
2. Click **New Project** → name it `CV-Axle-Controller`
3. Click **New** → **Schematic** → name it `Main`

---

## Step 1 — Place the Arduino Pro Micro

1. In the schematic editor, click **Library** (right panel)
2. Search: `Arduino Pro Micro`
3. Look for a component with **24 pins** (2×12) — specifically one labeled
   "Pro Micro ATmega32U4" or similar with pins: TXO, RXI, GND, GND, D2-D9
   on one side and RAW, GND, RST, VCC, A3-A0, 15, 14, 16, 10 on the other
4. If exact part not found, search: `2x12 female header 2.54mm`
   and we'll label the pins manually
5. Place it in the **center** of the schematic

**Alternative:** Search LCSC part number `C2977589` (2×12 pin header socket)

---

## Step 2 — Place Resistors

Search and place these **two** resistors:

| Ref | Search term | LCSC # | Value |
|-----|-------------|--------|-------|
| R1  | `220R 0805` | C17557 | 220Ω 0805 SMD |
| R2  | `220R 0805` | C17557 | 220Ω 0805 SMD |

- Place **R1** to the left of the Pro Micro (between D6 and J3)
- Place **R2** to the right of the Pro Micro (between D7 and J4)

> **Note:** 0805 is a small SMD resistor — easy to hand-solder. If you
> prefer through-hole, search `220R axial` instead (bigger, easier to solder,
> takes more board space).

---

## Step 3 — Place JST-XH Connectors

Search and place each connector. All are JST-XH, 2.54mm pitch, **right-angle**
(so wires exit sideways, not up into joystick bases).

| Ref | Pins | Search term | LCSC # | Placement |
|-----|------|-------------|--------|-----------|
| J1  | 4    | `JST XH 4P right angle` | C722733 | Left side (Joystick 1) |
| J2  | 4    | `JST XH 4P right angle` | C722733 | Right side (Joystick 2) |
| J3  | 2    | `JST XH 2P right angle` | C722731 | Left side (Fine LED 1) |
| J4  | 2    | `JST XH 2P right angle` | C722731 | Right side (Fine LED 2) |
| J5  | 3    | `JST XH 3P right angle` | C722732 | Upper area (RGB LED) |
| J6  | 2    | `JST XH 2P right angle` | C722731 | Lower center (Lock toggle) |
| J7  | 2    | `JST XH 2P right angle` | C722731 | Upper area (Recal button) |

> If exact right-angle parts aren't available, vertical JST-XH works too —
> just means wires go straight up. Search `JST XH 4P vertical` etc.

---

## Step 4 — Place Mounting Holes

1. Search: `mounting hole M2` or `MH2`
2. Place **4** mounting holes, one near each corner
3. These are non-electrical — just mechanical holes for M2 standoff screws

---

## Step 5 — Place Power Symbols

1. From the symbol library, search `GND` — place the **GND power symbol**
2. Search `VCC` — place the **VCC power symbol**
3. You'll attach these to the power pins and connector GND/VCC pins

---

## Step 6 — Wire the Schematic

### Power connections

| From | To | Notes |
|------|----|-------|
| Pro Micro **VCC** (right side) | VCC power symbol | |
| Pro Micro **GND** (right side, pin 2) | GND power symbol | The GOOD GND pin |
| Pro Micro **GND** (left side, pins 3+4) | **LEAVE UNCONNECTED** | Bad solder on clones — add "NC" note |
| J1 pin 2 (VCC) | VCC power symbol | |
| J1 pin 3 (GND) | GND power symbol | |
| J2 pin 2 (VCC) | VCC power symbol | |
| J2 pin 3 (GND) | GND power symbol | |
| J3 pin 2 (LED−) | GND power symbol | |
| J4 pin 2 (LED−) | GND power symbol | |
| J5 pin 3 (GND) | GND power symbol | |
| J6 pin 2 (GND) | GND power symbol | |
| J7 pin 2 (GND) | GND power symbol | |

### Signal connections — draw a wire between each pair

| From (Pro Micro pin) | To (Connector pin) | Signal |
|---------------------|--------------------|--------|
| A0 | J1 pin 1 (SIG) | Joystick 1 pot wiper |
| D2 | J1 pin 4 (BTN) | Joystick 1 button |
| A2 | J2 pin 1 (SIG) | Joystick 2 pot wiper |
| D3 | J2 pin 4 (BTN) | Joystick 2 button |
| D6 | R1 pin 1 | Fine LED 1 (through resistor) |
| R1 pin 2 | J3 pin 1 (LED+) | R1 output to LED 1 anode |
| D7 | R2 pin 1 | Fine LED 2 (through resistor) |
| R2 pin 2 | J4 pin 1 (LED+) | R2 output to LED 2 anode |
| D5 | J5 pin 1 (RED) | RGB LED red channel |
| D9 | J5 pin 2 (GRN) | RGB LED green channel |
| D4 | J6 pin 1 (SIG) | Lock toggle |
| D8 | J7 pin 1 (SIG) | Recal button |

### Wiring tips in EasyEDA
- Click the **Wire** tool (W key) to start drawing a wire
- Click on a pin to start, click on the destination pin to end
- Use **Net labels** (N key) as an alternative to long wires — place a
  label "A0" on the Pro Micro pin and another "A0" label on J1 pin 1,
  and EasyEDA connects them logically. Much cleaner schematic.
- **Power flags:** Every GND and VCC connection can use a power symbol
  instead of drawing wires to a bus — EasyEDA connects all GND symbols
  together automatically.

---

## Step 7 — Add Labels (for silkscreen)

1. Click **Text** tool
2. Add labels near each connector:
   - Near J1: `JOYSTICK 1 — SIG VCC GND BTN`
   - Near J2: `JOYSTICK 2 — SIG VCC GND BTN`
   - Near J3: `FINE LED 1 — + −`
   - Near J4: `FINE LED 2 — + −`
   - Near J5: `RGB LED — RED GRN GND`
   - Near J6: `LOCK — SIG GND`
   - Near J7: `RECAL — SIG GND`
3. Add title text: `CV AXLE CONTROLLER v1.0`
4. Add: `DIVIDIA TECHNOLOGIES`
5. Add near Pro Micro USB end: `← USB THIS END`

> These labels will appear on the silkscreen. Do this in the PCB
> layout step (Step 9), not the schematic.

---

## Step 8 — Run DRC (Design Rule Check)

1. Click **Design** → **Check ERC** (Electrical Rules Check)
2. Fix any errors — common ones:
   - "Unconnected pin" — make sure every used pin has a wire or net label
   - "Power pin not driven" — make sure VCC has a power flag
3. All warnings about unused Pro Micro pins (TXO, RXI, RAW, RST, 15, 14, 16, 10, A1, A3) are OK — leave them unconnected

---

## Step 9 — Convert to PCB Layout

1. Click **Design** → **Convert to PCB**
2. EasyEDA will create a new PCB file with all components as a "ratsnest" (unplaced, with connection lines showing what needs to connect)

### Set board outline
1. Switch to the **Board Outline** layer
2. Draw a rectangle: **55mm × 40mm**
3. Round the corners if desired (0.5mm radius)

### Place mounting holes
- One in each corner, **2mm from edges**
- That puts them at positions: (2,2), (53,2), (2,38), (53,38)

### Place components (approximate positions)

```
Board: 55mm wide × 40mm tall
(0,0) = bottom-left corner

    ┌──────────────────────────────────────────────┐
    │                                              │ 40mm
    │  J5(5,33)     PRO MICRO (15,14)    J7(42,33)│   ← BACK
    │               ┌──────────┐                   │
    │  J3(5,22)     │ 12mm×33mm│         J4(42,22)│
    │               └──────────┘                   │
    │  R1(5,17)                          R2(42,17) │
    │                                              │
    │  J1(5,5)      J6(22,3)            J2(42,5)  │   ← FRONT
    │                                              │
    └──────────────────────────────────────────────┘
                        55mm
```

- **Pro Micro sockets:** center of board, USB end toward back (top) edge
- **J1, J2:** front corners — wires drop straight down from joysticks
- **J3, J4:** left/right sides — near their respective joysticks
- **J5:** back-left — RGB LED drops from center-top panel
- **J7:** back-right — recal is on back panel
- **J6:** front-center — lock toggle is in front
- **R1, R2:** between Pro Micro and their LED connectors

### Route traces
1. Click **Route** → **Auto Router** for a first pass
2. Review the result — fix any traces that look weird
3. Manual routing tips:
   - Use **top layer** for signal traces (0.3mm width)
   - Use **top layer** for VCC trace (0.5mm width)
   - **Bottom layer** should be a **ground pour/fill** — this is your ground plane
4. To create ground plane: select **Bottom Copper** layer → **Design** →
   **Copper Area** → draw a rectangle covering the whole board → set net to **GND**

### Design rules for routing
| Parameter | Value |
|-----------|-------|
| Signal trace width | 0.3mm (12mil) |
| Power trace width | 0.5mm (20mil) |
| Clearance | 0.2mm (8mil) |
| Via size | 0.6mm hole / 1.0mm pad |

---

## Step 10 — Add Silkscreen Text

1. Switch to **Silkscreen** layer (TopSilk)
2. Add the connector labels from Step 7
3. Add board title and branding
4. Add pin 1 indicators (small dot or triangle) near each connector
5. Add `← USB` arrow near the back edge of the Pro Micro socket

---

## Step 11 — Final DRC + Order

1. **Design** → **DRC** (Design Rule Check on the PCB)
2. Fix any clearance or unrouted net errors
3. **Fabrication** → **PCB Fabrication File (Gerber)**
4. Review the Gerber preview — check:
   - All traces connected
   - Silkscreen readable
   - Mounting holes present
   - Board outline correct size
5. Click **Order at JLCPCB** or export Gerbers and upload to PCBWay

### PCBWay upload
1. Go to pcbway.com → **Quote Now** → **Quick-order PCB**
2. Upload the Gerber ZIP file
3. Settings (should auto-detect, verify these):
   - Size: 55 × 40 mm
   - Layers: 2
   - Thickness: 1.6mm
   - Solder mask: Green or Black
   - Silkscreen: White
   - Surface finish: HASL
   - Quantity: 5
4. Submit order

---

## Parts to Order Separately

These are NOT on the PCB — you solder JST-XH pigtail cables to them,
then plug into the board connectors.

| Part | Qty | Notes |
|------|-----|-------|
| JST-XH 4-pin pigtail cable | 2 | For joysticks. Or crimp your own. |
| JST-XH 3-pin pigtail cable | 1 | For RGB LED |
| JST-XH 2-pin pigtail cable | 4 | For Fine LEDs, lock toggle, recal |
| JST-XH crimping kit | 1 | Optional — lets you make custom length cables |
| Arduino Pro Micro (ATmega32U4, USB-C) | 1 | Plugs into the sockets on the PCB |

Amazon search terms:
- `JST XH 2.54mm connector kit` — usually comes with housings + crimp pins + pigtails
- `JST XH pre-crimped pigtail cables 2 3 4 pin` — pre-made cables, easiest option

---

## Summary

| What | Count |
|------|-------|
| Components to solder on PCB | 11 (2 headers, 2 resistors, 7 connectors) |
| Schematic connections | 12 signal + power/ground |
| Board size | 55 × 40 mm |
| Estimated PCB cost | $5-15 for 5 boards |
| Estimated design time | 1-2 hours in EasyEDA |
