# CV Axle Controller — Enclosure Component Spec

> Single reference for all physical dimensions driving the enclosure design.
> Measured 2026-05-13 with calipers unless noted.

---

## 1. JH-D400X-R4 Joystick (x2)

| Dimension | Value | Source |
|-----------|-------|--------|
| Grip diameter | 35mm | Datasheet |
| Stick travel | +/-25 deg from center | Datasheet |
| Above panel — grip | 42mm | Datasheet |
| Above panel — button top | 52.5mm (42 + 10.5) | Datasheet |
| Below panel — base depth | 25mm | Measured |
| Base housing (plastic) | 40mm x 40mm | Measured |
| Base with potentiometers | ~48-50mm (pots extend ~5mm each side) | Measured |
| Panel cutout (barrel) | 36mm diameter | Measured |
| Total height | ~84mm (includes seal compression variance) | Datasheet |

### Mounting System (top-down sandwich)

```
Flange plate (plastic, sits on top of enclosure)
  ID: 49.5mm, OD: 54mm, thickness: 4mm
Rubber seal (same OD as flange)
  ~54mm OD, 3mm thick, partially nests into flange collar
  Combined flange + seal: 4-6mm compressed
Enclosure top panel
  36mm cutout hole for barrel
Joystick base (below panel)
  40mm square housing, ~25mm deep
```

**Screw pattern:** 4x M2.5 holes, 32mm on-center between adjacent holes (square pattern)

**Assembly:** Joystick barrel pushes up from below through 36mm hole. Flange + seal sandwich the panel from above. Screws go down through flange, seal, panel, into joystick base.

**Enclosure top panel constraints:**
- Must have 36mm hole for barrel
- Must have 4x M2.5 clearance holes on 32mm square pattern
- Flange needs 54mm clear circle on top surface (no obstructions)
- Below panel needs 50mm x 50mm clear zone for base + pots

---

## 2. Arduino Pro Micro (x1)

| Dimension | Value |
|-----------|-------|
| PCB | 34mm x 19mm |
| Height on perfboard | 8mm (includes 2mm perfboard thickness) |
| USB-C port | Not used — separate breakout instead |

Mounted on perfboard inside enclosure. Position flexible since USB is broken out separately.

---

## 3. USB-C Breakout Board (x1)

| Dimension | Value | Source |
|-----------|-------|--------|
| PCB | ~18mm x 13mm | Estimated from photo (9mm USB-C ref) |
| Mounting holes | 2x, ~3mm diameter (H1, H2) | Photo |
| Port protrusion | Slight, past PCB edge | Photo |
| USB-C opening | 9mm W x 3.2mm H (standard) | Standard |
| Pins used | VBUS, GND, D-, D+ | Schematic |

**Panel mount:** USB-C port faces out through enclosure wall cutout. Board screws to interior wall standoffs. Needs rectangular cutout ~10mm W x 4mm H with 0.5mm tolerance.

---

## 4. Lock Toggle Switch (x1)

Using larger toggle (recommended for production feel).

| Dimension | Value |
|-----------|-------|
| Mounting hole | 11.5mm diameter |
| Below-panel depth | ~20mm |
| Type | SPST on/off |
| Wiring | D4 to one terminal, GND to other |

**Alt option:** Micro toggle with 6.5mm mounting hole, same ~20mm depth. Already wired but looks prototype-y.

---

## 5. Recal Button — D8 (x1)

| Dimension | Value |
|-----------|-------|
| Switch type | PCB tact switch, ~7mm x 7mm |
| Mounting | Glued/mounted inside enclosure against back wall |
| Access | Pinhole in enclosure wall (~2mm diameter) |
| Activation | Paperclip or pen tip through pinhole |

Prevents accidental recalibration. No external moving parts.

---

## 6. LEDs

### Blue Fine Mode LEDs (x2)
| Dimension | Value |
|-----------|-------|
| Diameter | 5mm |
| Mount hole | 5.1mm (friction fit) |
| Resistor | 220 ohm each |
| Pins | D6 (joystick 1), D7 (joystick 2) |

**Top panel seating:** 5.1mm hole through full 4mm panel thickness. LED's built-in flange (~6mm OD, ~1mm tall) sits flush with the top surface as a natural stop. 0.3mm top chamfer for clean look. No additional reinforcement needed — 4mm PLA around 5mm hole is structurally sound. LED inserts from below, flange catches on top surface.

### RGB Lock/Status LED (x1)
| Dimension | Value |
|-----------|-------|
| Type | 10mm RGB LED, common cathode, built-in resistors |
| Diameter | 10mm |
| Mount hole | 10.1mm (friction fit) |
| Resistor | None needed (built-in, confirmed bright at 5V) |
| Wires | 4: red, green, blue, black (common cathode/GND) |
| Pins used | D5 = red channel, D9 = green channel |
| Blue channel | Available if needed, leave unwired for now |
| Cost | ~$0.20/ea |
| States | Green = active/unlocked, Red = locked/inactive |
| Notes | Larger than 5mm fine LEDs intentionally — master status indicator |

**Top panel seating:** 10.1mm hole through full 4mm panel thickness, plus a **printed underside collar** (14mm OD, 10.1mm ID, 2mm tall) extending down from the panel interior. Total seat depth: 6mm (4mm panel + 2mm collar). The collar adds rigidity around the larger hole and acts as a light shield preventing sideways glow bleed inside the enclosure. LED inserts from below, flange sits flush with top surface. 0.3mm top chamfer for clean look.

### LED Seating Summary (for 3D model)

```
Top surface (outside)
  ├── 0.3mm chamfer (cosmetic)
  ├── LED lens protrudes flush or ~0.5mm above surface
  ├── LED flange sits on top surface rim (natural stop)
  ├── 4mm panel thickness (hole: 5.1mm for blue, 10.1mm for RGB)
  └── Underside collar (10mm RGB only): 14mm OD, 10.1mm ID, 2mm tall
      Acts as extended seat + light shield + reinforcement
```

---

## 7. Resistors (x3)

2x 220 ohm for blue fine LEDs (D6, D7). Lock LED has built-in resistors.
Standard 1/4W through-hole. Soldered on perfboard, no enclosure impact.

---

## 8. Perfboard (x1)

| Dimension | Value |
|-----------|-------|
| Size | 50mm x 70mm (standard, on hand) |
| Orientation | 70mm along enclosure width, 50mm along depth |
| Height | ~2mm (board thickness) |
| Mounting | Standoffs or screw posts inside enclosure base |
| Carries | Pro Micro, resistors, wiring connections, USB-C breakout wires |

---

## Top Panel Layout (Approved 2026-05-13)

Interactive layout tool: `docs/top-panel-layout.html`

### Slider Values

| Setting | Value |
|---------|-------|
| Joystick center-to-center | 100mm |
| Y offset from center | -4mm (joysticks shifted slightly toward back) |
| Side margin | 18mm |
| Front margin | 16mm |
| Back margin | 30mm |
| Indicator layout | C: Fine LEDs near joysticks, status center |
| Toggle Y offset | 14mm (below status LED, toward front/operator) |

### Computed Enclosure Dimensions

| Dimension | Value |
|-----------|-------|
| **Enclosure width** | **190mm** |
| **Enclosure depth** | **100mm** |
| Enclosure height | ~42mm (est., internal cavity ~35mm) |
| Joystick C-C | 100mm |
| Gap between flanges | 46mm |
| Top panel thickness | 3-4mm (must support joystick flange screws) |

### Top Panel Arrangement (operator perspective, looking down)

```
╔══════════════════════════════════════════╗
║            BACK (USB-C / recal)          ║  ← away from operator
║  ┌─USB-C─┐            ○ recal pinhole    ║
║──────────────────────────────────────────║
║                                          ║
║   ╭──────╮    [STATUS]    ╭──────╮       ║
║   │ J1   │   ●  R/G LED  │  J2  │       ║
║   │      │ ● FINE    FINE ● │      │       ║
║   ╰──────╯    [LOCK]     ╰──────╯       ║
║               ↕ toggle                   ║
║──────────────────────────────────────────║
║          AXLE CONTROLLER                 ║  ← branding zone
║           FRONT (operator)               ║  ← nearest to operator
╚══════════════════════════════════════════╝
```

- **Status LED (10mm RGB):** centered between joysticks, slightly above center
- **Fine LEDs (5mm blue):** inboard of each joystick flange, at joystick center height
- **Lock toggle (11.5mm):** centered, 14mm below status LED toward operator
- **USB-C:** center-back, panel mounted
- **Recal pinhole:** back panel, offset right of USB-C
- **Branding zone:** front edge between joystick labels

---

## Back Panel Layout (Approved 2026-05-13)

Interactive layout tool: `docs/back-panel-layout.html`

### Slider Values

| Setting | Value |
|---------|-------|
| USB-C horizontal offset | 0mm (centered) |
| USB-C vertical position | 10mm from bottom (lower third) |
| Recal horizontal offset | 40mm right of center |
| Recal vertical position | 22mm from bottom (upper half of cavity) |
| Recal pinhole diameter | 2mm |
| Vent style | Hex pattern |
| Vent zone width | 34mm each |
| Vent zone height | 24mm each |
| Vent placement | Both sides (symmetric) |

### Back Panel Arrangement (rear view, looking at back)

```
╔══════════════════════════════════════════╗
║  TOP (panel surface)                     ║
║──────────────────────────────────────────║
║ ┌─hex──┐                      ┌─hex──┐  ║
║ │vent  │                      │vent  │  ║
║ │34x24 │     D I V I D I A    │34x24 │  ║
║ │      │                      │      │  ║
║ └──────┘  ┌USB-C┐   ○ RECAL  └──────┘  ║
║──────────────────────────────────────────║
║  BOTTOM                                  ║
╚══════════════════════════════════════════╝
  LEFT(front)                RIGHT(front)
```

- **USB-C:** centered horizontally, 10mm from bottom — reduces cable stress
- **RECAL pinhole:** 2mm, 40mm right of center, 22mm from bottom — labeled, recessed ring
- **Ventilation:** hex pattern, 34x24mm zones on both sides — clears joystick bases
- **DIVIDIA:** centered branding, engraved/debossed into back panel surface

### Side Panels

Side panels are simple — no cutouts, no components. Dimensions:

| Dimension | Value |
|-----------|-------|
| Height | 42mm (matches enclosure height) |
| Depth | 100mm (matches enclosure depth) |
| Corner radius | 8mm (top and bottom edges) |
| Wall thickness | 3mm |

---

---

## Bottom Panel Layout (Approved 2026-05-13)

Interactive layout tool: `docs/bottom-panel-layout.html`

### Slider Values

| Setting | Value |
|---------|-------|
| Closure method | M3 corner screws (countersunk) |
| Screw inset from edge | 12mm |
| Perfboard X offset | 0mm (centered) |
| Perfboard Y offset | +8mm (shifted toward front, away from joystick bases) |
| Standoff height | 5mm |
| Foot inset from edge | 12mm |
| Center pillar spread | 20mm |
| Pillar diameter | 8mm |

### Bottom Panel Arrangement (underside view, front at bottom)

```
╔══════════════════════════════════════════╗
║  ⊕                    BACK            ⊕  ║  M3 screws
║   ┌─────USB-C breakout──────┐            ║
║   └─────────────────────────┘            ║
║  ┌J2 base┐              ┌J1 base┐       ║
║  │ 50x50 │  ◉  ribs  ◉  │ 50x50 │       ║
║  └───────┘  pillars      └───────┘       ║
║     ╔═══════PERFBOARD 70x50═══════╗      ║
║     ║  ○ [Pro Micro 34x19]    ○   ║      ║
║     ║                             ║      ║
║     ║  ○                      ○   ║      ║
║     ╚═════════════════════════════╝      ║
║  ⊕               FRONT               ⊕  ║  M3 screws
║  [▪]                              [▪]   ║  rubber feet
╚══════════════════════════════════════════╝
```

### Closure & Structure

| Feature | Spec |
|---------|------|
| **M3 corner screws** | 4x countersunk, 12mm inset from edges. Threaded into bosses on top shell |
| **Perimeter nesting lip** | 2mm wall on bottom shell, top shell sits over it. Full perimeter. Prevents lateral shift |
| **Support pillars** | 4x, 8mm diameter, 20mm spread from center. Back pair: behind joystick bases near back wall. Front pair: below perfboard near front wall. Post on bottom mates into socket on top |
| **Underside ribs** | 3mm tall cross-pattern on top shell underside, radiating from joystick mounts. Center span rib connecting both |
| **Joystick cradle lips** | 2mm raised lip on top shell underside, hugging joystick base OD (~42mm ring). Self-centering, prevents lateral movement |
| **Perfboard standoffs** | 4x M2 screw posts, 5mm tall, at perfboard corners (3mm inset from edge) |
| **Rubber feet** | 4x 13x13x4mm square adhesive pads, 12mm inset from corners. Slight recess printed into bottom shell |

### Internal Clearance Stack

```
Top panel surface (outside)
├── 4mm top panel thickness
├── 25mm joystick base (hanging from panel)
├── ~3mm clearance gap
├── 8mm Pro Micro on perfboard (2mm board + 6mm components)
├── 5mm standoff
├── 3mm bottom panel thickness
Bottom surface + 4mm rubber feet
```

**Total internal cavity:** 35mm (4mm top + 35mm cavity + 3mm bottom = 42mm external)
**Perfboard top to joystick base bottom:** ~3mm clearance — tight but workable. Wires route around sides.

---

## Height Summary

| Layer | Height |
|-------|--------|
| Rubber feet (below bottom) | 4mm |
| Bottom panel | 3mm |
| Standoffs | 5mm |
| Perfboard + Pro Micro | 8mm |
| Clearance gap | ~3mm |
| Joystick base depth | 25mm (from top panel down, includes pots) |
| Top panel | 4mm |
| **Total external height** | **42mm** (not counting feet or joystick grips) |
| Joystick grips above panel | 52.5mm |
| **Total desk-to-grip-top** | **~98.5mm** |

---

## Open Items

- [x] ~~Test 10mm multicolor LED~~ — Common cathode RGB, built-in resistors, bright at 5V
- [x] ~~Decide lock toggle~~ — 11.5mm (production feel)
- [x] ~~Top panel layout~~ — Approved, slider values recorded above
- [x] ~~Back panel layout~~ — Approved, settings recorded above
- [x] ~~Bottom panel layout~~ — Approved, structural reinforcement specified
- [x] ~~Choose perfboard size~~ — 50x70mm (on hand)
- [ ] Confirm USB-C breakout PCB dimensions with calipers
- [ ] 3D model creation (OpenSCAD or STL for printing)
