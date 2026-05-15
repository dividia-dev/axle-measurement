// Joystick Axis Restrictor — Y-Axis Only Gate Plate
// Replaces the stock circular gate plate with a vertical slot,
// restricting joystick movement to Y-axis only (no X travel).
//
// Fits: CHC-40B hall-effect joystick (and similar 40mm gate plates)
// Prints: Bambu Lab A1, 0.4mm nozzle, PLA
// All dimensions in mm.
//
// The stock plate has a 19mm circular opening allowing full 360-degree
// travel. This replacement uses a 13mm-wide vertical slot that blocks
// the X-axis gimbal carriage while allowing full Y-axis travel.
//
// Usage:
//   - Preview:  set PART = "preview"
//   - Export:   set PART = "print", then F6 -> Export STL

// ─── PART SELECTOR ──────────────────────────────────────────────
PART = "preview"; // "print" or "preview"

// ─── VARIANT SELECTOR [v3] ──────────────────────────────────────
// Two test variants to find optimal slot tightness:
//   1 = 12mm slot (1mm narrower than original 13mm)
//   2 = 11mm slot (2mm narrower than original 13mm)
// Set VARIANT before export. Debossed number on plate for identification.
VARIANT = 1;  // 1 or 2

// ─── PRINT TUNING ───────────────────────────────────────────────
NOZZLE       = 0.4;
LAYER_HEIGHT = 0.2;
TOL          = 0.2;   // fit tolerance for gate-to-housing

// ─── PLATE DIMENSIONS ───────────────────────────────────────────
PLATE_EXT    = 40;    // exterior plate width/height (square)
PLATE_INT    = 38;    // interior width between lips
PLATE_T      = 2.5;   // plate body thickness (matches stock)

// ─── EDGE LIPS ──────────────────────────────────────────────────
// Thin rails on all 4 edges that slide into slots in the controller
// housing bottom. These locate and retain the plate.
LIP_W        = 1;     // lip width (protrudes from plate edge)
LIP_H        = 2.5;   // lip height (extends below plate)

// ─── CORNER POSTS ───────────────────────────────────────────────
// Cylindrical standoffs at each corner for screw attachment.
// Screws pass through from below, thread into controller body above.
POST_H       = 8.5;   // total post height from plate bottom
POST_OD      = 6;     // post outer diameter
// No fillet — 3mm plate provides adequate base support

// ─── SCREW HOLES ────────────────────────────────────────────────
SCREW_CC     = 33;    // center-to-center spacing (square pattern)
SCREW_RECESS_D = 4;   // countersink recess diameter (top of plate)
SCREW_THROUGH_D = 3;  // passthrough hole diameter
SCREW_RECESS_DEPTH = 1.2; // countersink depth from plate top

// ─── RESTRICTOR SLOT ────────────────────────────────────────────
// Vertical slot replaces the stock 19mm circular opening.
// Width sized to match the X-axis gimbal carriage so the
// carriage physically cannot move side-to-side.
// [v3: parameterized by VARIANT for test prints]
SLOT_W       = (VARIANT == 1) ? 12 : 11;  // 12mm (-1) or 11mm (-2) from original 13
SLOT_L       = 25;    // slot length (Y direction — allows full Y travel)
                      // Sized to clear screw holes with 2mm margin

// Stock reference (not used in geometry, for documentation)
STOCK_HOLE_D = 19;    // original circular gate opening

// ─── REINFORCEMENT RIBS ─────────────────────────────────────────
// Two ribs run parallel to the slot on the top (interior) surface.
// They extend upward into the controller housing, creating a channel
// that physically blocks the gimbal carriage from X-axis travel.
RIB_H        = 3;     // rib height above plate top surface [v3: 3mm validated]
RIB_W        = 2.5;   // rib width (thickness in X direction)
RIB_L        = 25;    // rib length (matches slot length in Y direction)
RIB_GAP      = SLOT_W; // gap between ribs = slot width (ribs sit at slot edges)

// ─── VARIANT LABEL [v3] ────────────────────────────────────────
LABEL_DEPTH  = 0.6;   // deboss depth for variant number

// ─── SPRING CLEARANCE NOTES ─────────────────────────────────────
// Return springs: 17mm wide, extend 4mm from interior body wall.
// When joystick is at full Y deflection, one spring stretches to 16mm.
// Springs are BELOW the plate — no interference with plate geometry.
// If fins are added in a future version, they must avoid the spring zone:
//   Spring zone: 4mm inward from each Y-axis body wall edge.

// ─── COMPUTED VALUES ────────────────────────────────────────────
// Screw positions (centered on plate)
screw_offset = SCREW_CC / 2;
screw_positions = [
    [-screw_offset, -screw_offset],
    [ screw_offset, -screw_offset],
    [-screw_offset,  screw_offset],
    [ screw_offset,  screw_offset]
];

// ─── MODULES ────────────────────────────────────────────────────

// Rounded slot (stadium/oblong shape)
module slot_2d(w, l) {
    // Stadium: rectangle with semicircle caps
    hull() {
        translate([0, -(l/2 - w/2)])
            circle(d=w, $fn=48);
        translate([0,  (l/2 - w/2)])
            circle(d=w, $fn=48);
    }
}

// Main restrictor plate
// Built as a monolithic body to avoid non-manifold T-junctions:
// Full-depth slab from -LIP_H to PLATE_T, then subtract the non-lip interior.
module restrictor_plate() {
    lip_len = PLATE_INT;

    difference() {
        union() {
            // ── MONOLITHIC PLATE + LIP BODY ──
            // Full slab extending from -LIP_H to PLATE_T
            translate([-PLATE_EXT/2, -PLATE_EXT/2, -LIP_H])
                cube([PLATE_EXT, PLATE_EXT, PLATE_T + LIP_H]);

            // ── REINFORCEMENT RIBS ──
            // Two ribs flanking the slot, extending upward from plate top
            // Left rib (negative X side of slot)
            translate([-(RIB_GAP/2 + RIB_W), -RIB_L/2, PLATE_T - 0.01])
                cube([RIB_W, RIB_L, RIB_H + 0.01]);
            // Right rib (positive X side of slot)
            translate([RIB_GAP/2, -RIB_L/2, PLATE_T - 0.01])
                cube([RIB_W, RIB_L, RIB_H + 0.01]);

            // ── CORNER POSTS ──
            // Extend upward from plate top surface
            for (pos = screw_positions) {
                translate([pos[0], pos[1], PLATE_T])
                    cylinder(d=POST_OD, h=POST_H - PLATE_T, $fn=24);
            }
        }

        // ── REMOVE INTERIOR BELOW PLATE (keep only lip rails) ──
        // Cut away the material below Z=0 except where lips are.
        // This leaves 1mm-wide rails on all 4 edges.

        // Interior block below plate: everything inside the lip perimeter
        translate([-(PLATE_EXT/2 - LIP_W), -(PLATE_EXT/2 - LIP_W), -LIP_H - 0.01])
            cube([PLATE_EXT - 2*LIP_W, PLATE_EXT - 2*LIP_W, LIP_H + 0.01]);

        // Also remove the lip-depth material at corners (lips don't extend into corners)
        // Front-left corner
        translate([-PLATE_EXT/2 - 0.01, -PLATE_EXT/2 - 0.01, -LIP_H - 0.01])
            cube([(PLATE_EXT - lip_len)/2 + 0.01, PLATE_EXT + 0.02, LIP_H + 0.01]);
        // Front-right corner
        translate([PLATE_EXT/2 - (PLATE_EXT - lip_len)/2, -PLATE_EXT/2 - 0.01, -LIP_H - 0.01])
            cube([(PLATE_EXT - lip_len)/2 + 0.01, PLATE_EXT + 0.02, LIP_H + 0.01]);
        // Back-left corner (Y axis)
        translate([-PLATE_EXT/2 - 0.01, -PLATE_EXT/2 - 0.01, -LIP_H - 0.01])
            cube([PLATE_EXT + 0.02, (PLATE_EXT - lip_len)/2 + 0.01, LIP_H + 0.01]);
        // Back-right corner (Y axis)
        translate([-PLATE_EXT/2 - 0.01, PLATE_EXT/2 - (PLATE_EXT - lip_len)/2, -LIP_H - 0.01])
            cube([PLATE_EXT + 0.02, (PLATE_EXT - lip_len)/2 + 0.01, LIP_H + 0.01]);

        // ── RESTRICTOR SLOT (through entire plate + lip depth) ──
        translate([0, 0, -LIP_H - 0.01])
            linear_extrude(PLATE_T + LIP_H + 0.02)
                slot_2d(SLOT_W, SLOT_L);

        // ── VARIANT NUMBER [v3] ──
        // Debossed in top-right corner of plate, away from slot and ribs
        translate([PLATE_EXT/2 - 5, PLATE_EXT/2 - 5, PLATE_T - LABEL_DEPTH])
            linear_extrude(LABEL_DEPTH + 0.01)
                text(str(VARIANT), size=4, halign="center",
                     valign="center", font="Liberation Sans:style=Bold");

        // ── SCREW HOLES ──
        for (pos = screw_positions) {
            // Through-hole (full height including post)
            translate([pos[0], pos[1], -LIP_H - 0.01])
                cylinder(d=SCREW_THROUGH_D, h=POST_H + LIP_H + 0.02, $fn=20);

            // Countersink recess from bottom (screw head sits flush)
            translate([pos[0], pos[1], -LIP_H - 0.01])
                cylinder(d=SCREW_RECESS_D, h=SCREW_RECESS_DEPTH + 0.01, $fn=20);
        }
    }
}

// ─── PART SELECTION ─────────────────────────────────────────────

if (PART == "print") {
    // Print orientation: plate bottom on bed, posts pointing up
    // Lips will need support or can bridge (only 2.5mm, should bridge fine)
    restrictor_plate();
}
else if (PART == "preview") {
    // Preview with colors and stock hole ghost
    color("SteelBlue") restrictor_plate();

    // Ghost: stock circular opening for comparison
    %translate([0, 0, PLATE_T/2])
        cylinder(d=STOCK_HOLE_D, h=PLATE_T + 0.1, center=true, $fn=48);
}

// ─── RENDER SETTINGS ────────────────────────────────────────────
$fn = 48;
