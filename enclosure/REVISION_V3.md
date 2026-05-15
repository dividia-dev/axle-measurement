# Enclosure v3 Revision Notes

Print date: 2026-05-14 (overnight)
Evaluated: 2026-05-15

## What Printed Well (v2 validated)

- **Internal height increase** (47mm total, up from 42mm) — good clearance, no cramping
- **Step (rabbet) joint** — lip interaction between top and bottom shells prints clean, mates well
- **USB-C standoff positioning** — mount posts align correctly with PCB holes and port cutout
- **USB-C standoff dimensions** — OD 5.5mm, ID 2.2mm tap hole, height 3.5mm all working
- **Toggle switch mounting hole** — 13mm diameter, good fit
- **10mm RGB LED press fit** — good fit
- **5mm blue LED press fit** — good fit
- **Joystick mounting holes** — much improved in v2, good fit
- **Perfboard standoff mounts** — good fit

## Discussion: USB-C Cable Strain Relief

**Problem:** Cable plugs in solidly, but the connector end has significant wiggle room.
All the cable weight/leverage falls directly on the USB-C attachment to the breakout
board. In a cement facility environment, this will fatigue the solder joint over time.

**Idea:** Add a cable cradle/support extending from the back panel exterior. Standard
USB-C cable ends measure roughly 10-11mm wide x 4-6mm tall. An oval relief pocket
extending outward from the back wall (around the existing port cutout) could cradle
the plug housing and transfer cable strain to the enclosure instead of the PCB.

**Current state:** `USBC_RECESS_W=14, USBC_RECESS_H=8, USBC_RECESS_D=2.5` — this is
a shallow cosmetic recess. A strain relief would need to extend *outward* as a collar
or shroud, not inward. Dimensions TBD based on target cable profile.

**Option A: Oval collar / shroud (print-in-place)**
Extends ~8-10mm outward from the back wall around the existing port cutout. Oval bore
sized to standard USB-C cable overmold (~11mm W x 5mm H). The cable slides through the
collar, which cradles the plug body and transfers cable weight/leverage to the enclosure
wall instead of the PCB solder joint. Prints as part of the bottom shell, no extra parts.
Tradeoff: sized to one cable profile, may be loose on thinner cables.

**Option B: Integrated U-channel with zip-tie slot**
A U-shaped channel molded into the back wall exterior, running horizontally behind the
port cutout. Small slot (~2mm) cut through the channel floor for a standard small zip tie.
Cable routes through the channel, zip tie cinches it down. Industrial, reliable, works
with any cable diameter. Tradeoff: requires a zip tie (consumable), slightly less clean
visually.

**Status:** Decision deferred. Pick after v3 fixes are validated. Need to commit to a
reference USB-C cable profile (most standard ends: 10-11mm W x 4-6mm H).

---

## v3 Fixes Needed

### 1. Recal Button Mount — Redesign to Snap-In Cradle

**Problem:** Two issues with current standoff + retention plate design:
- Wall pocket too shallow (1mm). Button body is 3mm deep, actuator extends 2mm more.
  Button cannot seat flush without actuator being pre-depressed.
- Prong orientation deadlock: vertical standoffs block vertical prong placement,
  retention plate side-wings block horizontal prong placement. No valid orientation.

**Measured button:** 6x6mm body, 3mm deep, 4mm diameter actuator, 2mm actuator extension.

**Fix:** Replace standoffs + retention plate with a snap-in cradle molded into back wall:
- Pocket depth: ~4mm+ (button body seats flush, actuator free in pinhole channel)
- Two flexible snap tabs (top/bottom) lock button behind body shoulder
- Open left/right sides for prong clearance and wiring
- Force direction from pinhole jab is self-seating (into wall), tabs only resist gravity
- Remove: `recal_retention_plate()` module, mount post geometry, PART="recal_plate"
- Remove: 2mm rib STL export reference (only 3mm rib version validated)

### 2. Joystick Restrictor — Narrow Slot by 1mm

**Problem:** Minimal X-axis wobble/play remains with current 13mm slot width.

**Fix:** Two test versions to find the right tightness:
- Version A: `SLOT_W = 12` (1mm narrower than original 13mm)
- Version B: `SLOT_W = 11` (2mm narrower than original 13mm)
`RIB_GAP` tracks `SLOT_W`, so ribs close in automatically.
Rib height (3mm), rib width (2.5mm), all other dims unchanged.
3mm rib height is the validated production version. Remove 2mm rib variant from docs/exports.
Deboss variant number on plate top surface (corner, away from slot): "1" on 12mm, "2" on 11mm.
Export both STLs: `joystick-restrictor-12mm.stl` and `joystick-restrictor-11mm.stl`.

### 3. Bottom Shell Screw Bosses — 2mm Too Tall

**Problem:** Bottom shell bosses are `BOT_WALL_H - BOT_T = 7mm` tall. Top shell bosses
are 35mm. Combined = 42mm, but available interior height is only 40mm (47 - 4 top - 3 bot).
The 2mm excess equals `STEP_H` — the step joint overlap wasn't subtracted from the bottom
bosses. Top shell can't seat onto bottom without trimming.

**Fix:** Bottom boss height: `BOT_WALL_H - BOT_T - STEP_H` = 10 - 3 - 2 = **5mm**.
Line 533: `h=BOT_WALL_H - BOT_T` → `h=BOT_WALL_H - BOT_T - STEP_H`.
Top shell boss height (35mm) stays unchanged.

### 4. FINE Label Dashes — Replace with Pointer Triangles

**Problem:** The debossed dashes between "FINE" text and LEDs are biased toward the LED
holes (~1mm gap to LED, ~6mm gap to text). They read as stray marks rather than
intentional design. Dash length (~2.5mm) too short to connect anything visually.

**Fix:** Remove dashes. Replace with debossed triangles pointing from text toward each LED:
`LED ◀ FINE ▶ LED`
- Base (near text side): ~3mm tall
- Length: ~4-5mm toward LED
- Centered in the gap between text edge and LED edge
- Same 0.6mm deboss depth as text labels
- Tip rounds to ~0.4mm radius naturally at nozzle width (reads as clean pointer)
- Remove: `fine_dash_w`, `fine_dash_len`, `fine_dash_depth` params and dash geometry (lines 213-216, 409-419)

### 5. Remove S/N Sticker Recess

**Problem:** 25x10mm x 0.3mm sticker recess (lines 612-615) is positioned at Y=-28 to Y=-18,
directly under the URL text instead of under the S/N line. At 0.3mm depth (1.5 layers),
the slicer generates sparse infill that prints as visible horizontal striping below the URL.

**Fix:** Remove the sticker recess entirely (lines 612-615).

### 6. Rubber Foot Recesses — Keep As-Is

**Problem (v2):** With supports enabled (needed for recal standoffs), slicer filled the
0.8mm foot recesses with impossible-to-remove support material.

**Resolution:** Keep recesses unchanged. V3 snap-in recal cradle eliminates the need for
supports on the bottom shell. If supports are still needed for any reason, use Bambu
Studio **support blockers** painted on the foot recess areas to exclude them from support
generation. No SCAD change needed.

### 7. Top Shell Fillet Junction Ridge

**Problem:** Visible/tactile ridge where the top panel meets the side walls. Caused by
two things: (1) `fillet_top_box` is two separate bodies joined at Z = h - fillet_r, and
the minkowski sphere uses `$fn=24` — the 24-sided polygon doesn't perfectly match the
straight extrusion below, creating a geometric step the slicer reproduces. (2) Elephant
foot effect when printing upside down (wall layers starting on solid top panel surface).

**Fix:** Increase fillet sphere `$fn` from 24 to 64 in `fillet_top_box` module (line 247).
Reduces geometric mismatch at junction to below print resolution (~0.01mm vs ~0.1mm).
Slightly slower render, no other side effects.

### 8. Z-Seam Visibility on Back Panel (Slicer Setting)

**Problem:** Visible vertical seam line running through "DIVIDIA" text on back wall.
Not present in v1 print. V2 geometry changes shifted slicer's seam placement from
a corner to X=0 on the branded back surface. This is a slicer issue, not SCAD.

**Fix (Bambu Studio):** Enable **Scarf joint seam** (Quality → Seam). Gradually ramps
extrusion at start/stop points so the seam is nearly invisible regardless of placement.
Works on flat surfaces like the back wall. No need to repaint when geometry changes.

**Alternative:** Use seam painting tool — paint exterior flat walls as blocker (red),
corner radii as enforced (green). Pushes seams to corners where 8mm radius hides them.

### 9. Deboss Ghosting / Ringing + Oval Recal Circle

**Problem:** Visible ripple/echo lines radiating from debossed text on back panel and top
panel. Recal target ring prints as an oval instead of a circle. Both caused by toolhead
ringing during rapid direction changes around small deboss perimeters on vertical walls.
At 0.6mm deboss depth (3 layers on vertical wall), the slicer traces marginal offsets
that amplify ghosting. Small circle chords at top/bottom of recal ring get distorted.

**SCAD fix:**
- `BRAND_DEPTH`: 0.6 → **1.0mm** (deeper = cleaner slicer perimeter paths, less ambiguity)
- `LABEL_DEPTH`: 0.6 → **1.0mm** (same reason, applies to all text labels)
- `RECAL_RECESS_DEPTH`: 1.5 → **2.0mm** (crisper ring definition on vertical wall)

**Slicer fix (Bambu Studio):**
- Outer wall speed: reduce to 30-40mm/s (less ringing around deboss features)
- Small perimeter speed: 20-25mm/s (targets letter voids and recal circle)
- Run input shaping recalibration from printer menu (retune for PETG behavior)

### 10. DIVIDIA / TECHNOLOGIES Width Balance

**Problem:** "TECHNOLOGIES" (12 chars, size 3.8, spacing 1.1) appears slightly wider than
"DIVIDIA" (7 chars, size 7, spacing 1.3) on the back panel. The narrow characters in
DIVIDIA (three I's) don't fill as much horizontal space as the wide characters in
TECHNOLOGIES (T, H, O, N) even with the spacing difference.

**Fix:** Increase "DIVIDIA" spacing from 1.3 to **1.4** to visually balance the two lines.
Line 453: `spacing=1.3` → `spacing=1.4`.
