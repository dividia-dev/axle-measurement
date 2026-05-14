# Enclosure Revision v2 — Test Print Feedback

> Notes collected 2026-05-14 from first test print on Bambu Lab A1.
> Each item will be incorporated into `enclosure.scad` before re-rendering.

---

## 1. USB-C Breakout Board Mount — Corrected Measurements

**Board:** 22mm wide x 13mm deep x 1mm thick (PCB only)

**USB-C socket:** 10mm wide x 3mm tall, mounted on top of PCB.
Total height at socket: 4mm (1mm board + 3mm socket).
Connector extends 2mm beyond front edge of PCB.

**Mounting holes:**
- 2x holes on the front edge (USB-C side), NOT diagonal corners
- 3mm diameter (M2.5 screw)
- Each hole is 3mm from the front edge and 3mm from the nearest side edge
- Horizontal spacing: 22mm - 3mm - 3mm = **16mm center-to-center**
- Depth from back wall (both holes): 3mm from board edge = board sits with
  USB-C edge flush against interior wall

**Standoff height calculation:**
- Wall cutout center: Z = 10mm from enclosure bottom
- Interior floor: Z = 3mm (bottom panel thickness)
- Floor to cutout center: 7mm
- USB-C socket center above standoff top: 1mm (board) + 1.5mm (half socket) = 2.5mm
- Required standoff height: 7mm - 2.5mm = **4.5mm**
- (v1 print had 2mm standoffs — too low)

**Standoff positions (in enclosure coordinates):**
- Board centered at X=0, USB-C edge against back wall interior (Y = ENC_D/2 - WALL)
- Hole 1: X = -8mm, Y = ENC_D/2 - WALL - 3mm
- Hole 2: X = +8mm, Y = ENC_D/2 - WALL - 3mm

**Cable seating — outside recess needed:**
- USB-C plug is ~8.5mm insertion depth
- Connector extends 2mm past board into the 3mm wall = only 5mm exposed outside
- Need ~6mm deep rectangular recess (counterbore) on outside of back panel
  around the USB-C cutout so cable can fully seat
- Recess dimensions: ~14mm W x 8mm H x 6mm deep (TBD)

**Changes to SCAD:**
- Update `USBC_MOUNT_SPACING` from 12mm to 16mm
- Update `USBC_STANDOFF_H` from 2mm to 4.5mm
- Update `USBC_STANDOFF_ID` to 3.2mm (M2.5 clearance, was 2.2mm for M2)
- Move standoff Y position closer to back wall (3mm from wall interior)
- Add outside recess/counterbore around USB-C cutout in back panel
- Update `USBC_W` cutout to match 10mm socket + tolerance
- Update `USBC_H` cutout to match 3mm socket + tolerance

---

## 2. Shell Mating Joint — Replace Nesting Lip with Step (Rabbet) Joint

**Problem:** The v1 nesting lip (1.5mm thin wall floating inside the main wall)
failed during printing. When the top shell prints upside down, the lip is the
last thing printed with nothing below it to anchor to. Strings of filament,
print aborted.

**Solution:** Step (rabbet) joint. Each shell wall gets half its thickness
removed at the mating edge, creating an interlocking step.

```
Bottom shell wall (cross-section, 3mm wall):
    ┌────┐
    │    │  ← outer 1.5mm at full height
    │    └───┐
    │        │  ← inner 1.5mm is 2mm shorter
    └────────┘

Top shell wall (cross-section, 3mm wall):
    ┌────────┐
    │        │  ← inner 1.5mm at full depth
    ┌────┘   │
    │        │  ← outer 1.5mm is 2mm shorter
    └────────┘

Assembled:
    ┌──┬─────┐
    │  │     │  ← 2mm interlocking overlap
    │  └──┬──┘
    │     │
    └─────┘
```

**Why step joint:**
- Zero floating material on either half — every layer on top of previous
- No supports needed
- 1.5mm wall on each side at overlap (3+ perimeters at 0.4mm nozzle)
- Self-aligning, dust-sealed, friction fit
- 2mm overlap depth — solid engagement, still easy to open

**Changes to SCAD:**
- Remove current `LIP_H` / `LIP_W` nesting lip from top shell
- Bottom shell: add step — inner half of wall top is 2mm shorter
- Top shell: add step — outer half of wall bottom is 2mm shorter
- Step width = WALL/2 = 1.5mm on each side
- Step height (overlap) = 2mm
- 0.15mm tolerance on the step for fit (half of normal 0.3mm since
  it's a sliding fit, not a hole)

---

## 3. Perfboard Standoff Spacing — Corrected Measurements

**Board:** 70mm x 50mm (confirmed)

**Mounting holes:**
- 4x holes, one in each corner
- 2mm from board edge to hole center (all sides)
- Hole diameter: 2mm

**Corrected standoff positions (relative to perfboard center):**
- Hole 1: (-33, -23) — was (-32, -22) with old 3mm inset
- Hole 2: (+33, -23)
- Hole 3: (-33, +23)
- Hole 4: (+33, +23)
- Spacing: 66mm x 46mm center-to-center

**Changes to SCAD:**
- Update `STANDOFF_INSET` from 3mm to 2mm
- Update `STANDOFF_ID` to 2.2mm (M2 clearance for 2mm holes)
- This shifts each standoff 1mm outward toward the board corners

---

## 4. Replace Alignment Pillars with Additional Screw Points (6 total)

**Problem:** Long edges of the enclosure (190mm) only have screws at the 4 corners.
The friction-fit alignment pillars provide center support but don't clamp the
shells together. Edges can flex or gap between the corner screws.

**Solution:** Remove the 4 friction-fit alignment pillar system entirely (male pins
on bottom, female sockets on top). Replace all 4 pillar positions with M3 screw
points identical to the corner screws. Total: **8 M3 screw points.**

**New screw layout (top view):**
```
┌──⊕────────⊕──────────⊕────────⊕──┐
│  corner    pillar→screw         corner  │
│                                         │
│                                         │
│  corner    pillar→screw         corner  │
└──⊕────────⊕──────────⊕────────⊕──┘
```

**4 new screw positions (replacing pillar_positions):**
- Back left:  X = -20, Y = +35 (was back-left pillar)
- Back right: X = +20, Y = +35 (was back-right pillar)
- Front left: X = -20, Y = -38 (was front-left pillar)
- Front right: X = +20, Y = -38 (was front-right pillar)

These are the existing `pillar_positions` — just changing from friction-fit
pillars to M3 screw points with the same boss/countersunk treatment as corners.

**Changes to SCAD:**
- Remove `align_pin_positions`, `ALIGN_PIN_D/H`, `ALIGN_SOCKET_D/H` entirely
- Remove `pillar_positions`, `PILLAR_D`, `PILLAR_PIN_D/H`, `PILLAR_SOCKET_D/H`
- Remove alignment pin posts and structural pillar posts from bottom shell
- Remove alignment pin sockets and structural pillar bosses/sockets from top shell
- Add 4 pillar positions to `screw_positions` array (8 total)
- All 8 get identical treatment: threaded boss in top shell, countersunk M3
  hole through bottom shell
- Rubber foot recesses only on the original 4 corner positions (not the new 4)

---

## 5. Joystick M2.5 Screw Holes — Increase Diameter for Assembly Ease

**Problem:** M2.5 screw clearance holes at 2.8mm are too tight for easy assembly,
especially with the flange sandwich design and rubber boot friction.

**Solution:** Increase screw hole diameter by 1mm for more play.

**Changes to SCAD:**
- Update `JOY_SCREW_D` from 2.8mm to 3.8mm
- Position unchanged (32mm square pattern, 4 per joystick)
- Barrel hole (36mm + tolerance) unchanged

---

## 6. Move 10mm Status LED — 14mm Toward Back

**Problem:** The 10mm RGB status LED is currently at the same Y as the fine LEDs
(Y = joy_y = -4mm). The toggle switch is 14mm below it (toward front). The LED
is too close to the toggle vertically — can't mount the toggle in correct
orientation.

**Solution:** Move the 10mm status LED 14mm toward the back (increase Y by 14mm).

**New position:**
- Status LED Y: joy_y + 14 = -4 + 14 = **Y = +10mm** (10mm behind center)
- Toggle stays at Y = -18mm (unchanged)
- Fine LEDs stay at Y = -4mm (unchanged)
- Gap between status LED and toggle: 28mm (was 14mm) — plenty of room

**Decision: Keep toggle switch (not button)**
- Toggle shows physical state — operator sees up/unlocked, down/locked at a glance
- Deliberate flip prevents accidental activation (critical for DOT permit measurements)
- Tactile snap confirmation by feel, no need to check LED
- Industrial toggle reads as professional for cement facility context
- Flush button better suited for momentary actions (like recal), not state toggles

**Changes to SCAD:**
- Change status LED Y from `joy_y` to `joy_y + 14`
- Move the underside collar to match
- Toggle, fine LEDs, joystick positions all unchanged

---

## 7. Increase Enclosure Height by 5mm (42mm → 47mm)

**Problem:** Internal clearance is too tight. With perfboard, Arduino, joystick
bases, and potentiometer tabs hanging below the joystick housings, components
couldn't physically fit during test assembly (not yet soldered, but already
impossible to close).

**Original clearance stack (spec):**
- 4mm top panel
- 25mm joystick base depth
- ~3mm clearance gap ← too tight
- 8mm Pro Micro on perfboard
- 5mm standoff
- 3mm bottom panel
- Total: 42mm (48mm worth of stuff in 42mm... was always marginal)

**New height: 47mm** (+5mm)
- Extra 5mm goes entirely into the internal cavity
- Internal cavity: 40mm (was 35mm)
- New clearance gap: ~8mm — room for pot tabs, wires, solder joints

**Changes to SCAD:**
- Update `ENC_H` from 42mm to 47mm
- All internal features that reference ENC_H will automatically adjust
  (screw bosses, pillar heights, wall heights, rib positions, etc.)
- USB-C cutout Z position may need verification (currently USBC_Y=10 from bottom)
- Recal pinhole Z position may need verification (currently RECAL_Y=22 from bottom)
- Bottom shell height (BOT_WALL_H=10) stays the same — extra height goes to top shell
- Rubber feet, perfboard standoffs unchanged (they're relative to floor)

**Visual impact:** 5mm taller is barely noticeable in use. Still a low-profile
controller at 47mm (under 2 inches). Good tradeoff for actually being assemblable.

---

## 8. Rename and Relocate Top Panel Branding

**Change:** "AXLE CONTROLLER" → "AXLE TRACKER"

**Move:** From front of top panel (below toggle, toward operator) to back of
top panel (above joysticks, toward back wall).

**New position:**
- X = 0 (centered horizontally)
- Y = centered between the top edge of the 10mm status LED and the back edge
  of the enclosure
- Status LED new position: Y = +10 (from item #6)
- LED top edge: Y = 10 + 10.1/2 ≈ Y = +15
- Back edge of enclosure: Y = +50 (ENC_D/2)
- Center: (15 + 50) / 2 = **Y = +32.5**

**Changes to SCAD:**
- Update text string from "AXLE CONTROLLER" to "AXLE TRACKER"
- Update Y position from -ENC_D/2 + 13 to approximately +32.5
- Text size, font, depth unchanged
- Front zone is now clear for additional debossed items (next item)

---

## 9. Top Panel Labeling — Debossed Text and Indicators

**Layout (operator perspective, top-down):**
```
              AXLE TRACKER
    ┌─────────────────────────────┐
    │                              │
    │            LOCK              │  debossed, 4-5mm text
    │          ● (R/G)             │  10mm LED (Y=+10)
    │                              │
    │    ●  ── FINE ──  ●          │  blue LEDs (Y=-4) + label
    │  ╭────╮          ╭────╮      │
    │  │ J1 │          │ J2 │      │
    │  ╰────╯          ╰────╯      │
    │          ↕ toggle            │  (Y=-18)
    │                              │
    └─────────────────────────────┘
```

**Labels to deboss:**

1. **"LOCK"** — centered at X=0, above 10mm status LED.
   Y = status LED center + LED radius + ~4mm gap = 10 + 5 + 4 = **Y ≈ +19**
   Size: 4-5mm, depth: 0.6mm

2. **"FINE" with connecting dashes** — centered at X=0, at same Y as fine LEDs (Y=-4).
   Two horizontal dashes extend from the text toward each blue LED.
   - Text "FINE" centered between the two LEDs
   - Left dash: from text edge toward left LED, ~8-10mm long, 1mm wide, 0.6mm deep
   - Right dash: mirror of left
   - Total reads as: `● ── FINE ── ●`

3. **No toggle ON/OFF label** — toggle physical state (up/down) is self-evident,
   status LED confirms with color. Remove trim ring, keep it clean.

4. **No joystick labels** — obviously joysticks, web app shows line assignment.
   Debossed text under flanges would look tiny and unnecessary.

**Production method:**
- All labels debossed 0.6mm deep, printable with 0.4mm nozzle at 5mm+ text size
- Paint fill with white or yellow acrylic for contrast against dark PLA
- Squeeze paint into channels, wait 5 min, wipe flat surface with damp paper towel
- Result: crisp factory-finish lettering, flush surface

**Changes to SCAD:**
- Add "LOCK" text deboss at (0, +19, ENC_H - BRAND_DEPTH)
- Add "FINE" text deboss at (0, -4, ENC_H - BRAND_DEPTH)
- Add two rectangular deboss channels (1mm wide x 0.6mm deep x ~10mm long) on
  either side of FINE text connecting toward blue LED positions
- Remove any toggle labeling

---

## 10. Back Panel — Branding, Recal Pinhole, and Labels

**A. DIVIDIA TECHNOLOGIES branding:**
- "DIVIDIA" — centered (X=0), current size (7mm), letter-spaced
- "TECHNOLOGIES" — centered below DIVIDIA, smaller font to match DIVIDIA width
  (~4mm text size should roughly match the visual width)
- Both vertically centered as a block on the back panel between the top edge
  and the USB-C/recal zone
- Verify centering with new 47mm enclosure height

**B. Recal pinhole — recessed target ring:**
- Replace bare 2mm pinhole with a professional recessed target:
  - 8mm diameter circular pocket, 1.5mm deep, on outside of back wall
  - 2mm pinhole at center of the pocket (goes through remaining wall)
  - Looks like a commercial reset button recess
  - Prints cleaner — larger outer circle resolves well, pinhole is recessed
    and less visible

**C. "RECAL" label:**
- Debossed above the target ring
- ~4mm text, 0.6mm depth
- Centered on the pinhole X position (X = +40mm from center)

**Back panel layout (rear view):**
```
┌──────────────────────────────────────┐
│                                      │  top
│           D I V I D I A              │
│          TECHNOLOGIES                │
│                                      │
│                         RECAL        │
│     ┌USB-C┐            (●)          │  recessed target ring
│                                      │
└──────────────────────────────────────┘
                                          bottom
```

**Changes to SCAD:**
- Fix DIVIDIA vertical centering on back panel
- Add "TECHNOLOGIES" text below DIVIDIA (~4mm size, match width)
- Replace 2mm pinhole cylinder with recessed target: 8mm pocket (1.5mm deep)
  + 2mm through-hole at center
- Add "RECAL" debossed text above the target ring
- All text mirrored for correct reading from outside (same as current DIVIDIA fix)

---

## 11. Bottom Panel — Product Info Block

**Debossed info block, centered on bottom panel:**
```
    DIVIDIA TECHNOLOGIES
       MODEL AT-C100
      S/N: __________
       5V DC · USB-C
        TEXAS, USA
      www.dividia.net
```

- Text size: 3-4mm, 0.6mm depth
- Centered as a block (X=0, Y=0 on bottom panel)
- Paint fill for contrast

**Serial number sticker:**
- Deboss "S/N:" as permanent label, leave blank area for future sticker
- Standard asset/serial label size: **25mm x 10mm** (1" x 0.4")
  Fits comfortably on the 190x100mm bottom panel
- Could also go larger: 50mm x 20mm for a combined barcode + S/N + company
  logo sticker if desired later
- Print on any thermal label printer (Brother P-Touch, DYMO, or proper
  asset tag stock from Amazon)
- Optionally deboss a shallow rectangular recess (0.3mm deep) at the sticker
  location so it sits flush and doesn't peel at the edges

**Changes to SCAD:**
- Add debossed text block on bottom panel exterior (Z = 0, facing down,
  text mirrored so it reads correctly when flipped over)
- Optionally add shallow rectangular recess (25x10mm or 50x20mm) for
  future S/N sticker placement, positioned below the text block

---

## 12. Joystick Axis Restrictor — SEPARATE PROJECT (Future)

**Goal:** Restrict joystick to X-axis only (freeze vertical/Y movement).
Either a replacement bottom cover for the JH-D400X-R4 or an insert that
blocks the Y-axis range of motion.

**Status:** Needs measurements and reference photos of the joystick base
interior / existing bottom cover. Will be a separate print file, not part
of the enclosure SCAD.

**Next steps:**
- Take measurements of joystick base interior and bottom cover
- Get images of the mechanism that allows Y-axis movement
- Design restrictor insert or replacement cover
- Separate SCAD file: `enclosure/joystick-restrictor.scad`

---

## Summary of All v2 Changes

1. USB-C mount: 16mm spacing, 4.5mm standoffs, M2.5 holes, outside cable recess
2. Step (rabbet) joint replacing nesting lip
3. Perfboard standoff inset: 3mm → 2mm
4. 8 total M3 screws (4 corners + 4 replacing alignment pillars)
5. Joystick screw holes: 2.8mm → 3.8mm
6. Status LED moved +14mm toward back (Y=+10)
7. Enclosure height: 42mm → 47mm
8. "AXLE TRACKER" moved to back of top panel
9. Top panel labels: LOCK, FINE with dashes, no toggle label
10. Back panel: DIVIDIA TECHNOLOGIES, recessed recal target, RECAL label
11. Bottom panel: product info block (AT-C100, Texas, dividia.net, S/N area)
12. Joystick axis restrictor (separate future project)
