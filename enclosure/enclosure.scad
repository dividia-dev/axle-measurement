// CV Axle Controller — Enclosure v3
// Optimized for Bambu Lab A1, 0.4mm nozzle, PETG/PACF
// All dimensions in mm. Source: docs/enclosure-component-spec.md
// Revision notes: enclosure/REVISION_V3.md
//
// Design: Top shell = main body (tall). Bottom shell = shallow tray.
// Step (rabbet) joint for shell mating. 8x M3 screws for clamping.
//
// Usage:
//   - Render top shell:       set PART = "top"
//   - Render bottom shell:    set PART = "bottom"
//   - Preview assembled:      set PART = "assembly"
//   - Export STL:             set PART to desired part, then F6 → Export

// ─── PART SELECTOR ───────────────────────────────────────────────
PART = "assembly"; // "top", "bottom", "assembly"

// ─── PRINT TUNING (Bambu Lab A1, 0.4mm nozzle) ──────────────────
NOZZLE       = 0.4;
LAYER_HEIGHT = 0.2;
TOL          = 0.3;   // general fit tolerance
STEP_TOL     = 0.15;  // step joint tolerance (tighter sliding fit)

// ─── ENCLOSURE DIMENSIONS ────────────────────────────────────────
ENC_W        = 190;   // width (X)
ENC_D        = 100;   // depth (Y)
ENC_H        = 47;    // total external height (Z) [v2: was 42]
CORNER_R     = 8;     // corner radius
WALL         = 3;     // wall thickness
TOP_T        = 4;     // top panel thickness
BOT_T        = 3;     // bottom panel thickness

// ─── SHELL SPLIT — STEP (RABBET) JOINT ───────────────────────────
// Bottom = shallow tray. Top = main body.
// Step joint: each wall has half-thickness removed at the mating edge.
BOT_WALL_H   = 10;    // total bottom shell height (floor + short walls)
STEP_H        = 2;     // step overlap height
STEP_W        = WALL / 2;  // step width (half wall = 1.5mm)

// ─── JOYSTICK ────────────────────────────────────────────────────
JOY_CC       = 100;   // center-to-center spacing
JOY_Y_OFF    = -4;    // Y offset from center (toward back = negative)
JOY_BARREL   = 36;    // panel cutout diameter
JOY_FLANGE   = 54;    // flange OD (clear zone on top surface)
JOY_SCREW_SQ = 32;    // M2.5 screw pattern (square, on-center)
JOY_SCREW_D  = 3.8;   // M2.5 clearance hole [v2: was 2.8, +1mm for assembly ease]
JOY_BASE_SQ  = 42;    // base housing clearance
JOY_BASE_DEPTH = 25;  // below-panel depth

// ─── TOGGLE SWITCH ───────────────────────────────────────────────
TOGGLE_D     = 13 + TOL;    // mounting hole + tolerance [v2: was 11.5, +1.5mm]
TOGGLE_Y_OFF = 14;    // mm below FINE LEDs (toward front)

// ─── LEDs ────────────────────────────────────────────────────────
LED_FINE_D   = 5.1;   // 5mm blue LEDs, friction fit
LED_FINE_CHAMFER = 0.3;
LED_RGB_D    = 10.1;  // 10mm RGB LED, friction fit
LED_RGB_COLLAR_OD = 14;   // underside collar outer diameter
LED_RGB_COLLAR_H  = 2;    // collar height (extends below panel)
LED_RGB_CHAMFER = 0.3;

// Fine LED positions: inboard of each joystick flange
FINE_LED_INBOARD = 32; // distance from joystick center toward enclosure center

// Status LED offset: moved 14mm toward back from fine LEDs [v2]
STATUS_Y_OFFSET = 14;

// ─── USB-C BREAKOUT [v2: corrected measurements] ────────────────
USBC_W       = 10 + TOL;  // cutout width (10mm socket + tolerance)
USBC_H       = 3 + TOL;   // cutout height (3mm socket + tolerance)
USBC_Y       = 10;    // center of cutout from bottom of enclosure (Z=0)
USBC_PCB_W   = 22;    // breakout PCB width [v2: was 18]
USBC_PCB_D   = 13;    // breakout PCB depth [v2: renamed from H]
USBC_EXTEND  = 2;     // connector extends past PCB edge
// Mount posts: 2x M2.5 screw posts on bottom shell floor, near back wall
// Holes are on the front edge (USB-C side), 3mm from front and side edges
USBC_MOUNT_SPACING = 16;   // horizontal distance between mount holes [v2: was 12]
USBC_MOUNT_HOLE_INSET = 3; // hole distance from board edges
USBC_MOUNT_Y = ENC_D/2 - WALL - USBC_MOUNT_HOLE_INSET;  // 3mm from back wall interior
USBC_STANDOFF_H = 3.5;     // [v3: was 4.5, lowered so board sits 1mm below port cutout]
                            // Board top at Z=7.5 (3+3.5+1), cutout bottom at Z=8.35
USBC_STANDOFF_OD = 5.5;    // [v3: was 5, wider so it won't fall through PCB holes]
USBC_STANDOFF_ID = 2.2;    // [v3: was 3.2 clearance, now tap hole for self-tap into PLA]

// Cable recess on outside of back panel [v2: new]
USBC_RECESS_W = 14;   // recess width
USBC_RECESS_H = 8;    // recess height
USBC_RECESS_D = 2.5;  // [v3: was 6, capped to stay within wall thickness]

usbc_mount_positions = [
    [-USBC_MOUNT_SPACING/2, USBC_MOUNT_Y],
    [ USBC_MOUNT_SPACING/2, USBC_MOUNT_Y]
];

// ─── RECAL PINHOLE [v2: recessed target ring] ────────────────────
RECAL_D      = 2;     // pinhole diameter
RECAL_X_OFF  = 40;    // mm right of center (viewed from back)
RECAL_Y      = 22;    // mm from bottom of enclosure (Z=0)
RECAL_RECESS_D = 8;   // target ring outer diameter [v2: new]
RECAL_RECESS_DEPTH = 2.0; // target ring depth [v3: was 1.5, deeper for crisp print]

// ─── RECAL BUTTON MOUNT [v3: snap-in cradle] ────────────────────
// Standard 6x6mm tactile switch, 3mm body depth, 4mm actuator, 2mm extension.
// Snap-in cradle on back wall interior: pocket seats button body flush,
// flexible tabs (top/bottom) lock behind body, open sides for prong clearance.
// Actuator faces wall, aligns with pinhole. Force from pinhole jab is self-seating.
RECAL_BTN_W      = 6;     // button body width (X)
RECAL_BTN_H      = 6;     // button body height (Z)
RECAL_BTN_TOL    = 0.3;   // pocket tolerance per side
RECAL_BTN_DEPTH  = 3;     // button body depth (measured)
RECAL_BTN_ACT_D  = 4;     // actuator diameter
RECAL_BTN_ACT_EXT = 2;    // actuator extension beyond body
RECAL_CRADLE_DEPTH = 4;   // pocket depth (body flush + actuator clearance)
RECAL_SNAP_T     = 1.2;   // snap tab thickness
RECAL_SNAP_LIP   = 0.8;   // snap tab lip overhang (locks behind button body)

// ─── SEALED ENCLOSURE ────────────────────────────────────────────
// No ventilation — heat generation <1W, dust protection for cement facility.

// ─── M3 SCREWS — 8 TOTAL [v2: was 4 corners + 4 alignment pillars]
SCREW_INSET  = 12;    // from edge (for corner screws)
SCREW_D      = 3.4;   // M3 clearance
SCREW_HEAD_D = 6.2;   // M3 countersunk head
SCREW_HEAD_DEPTH = 1.8;
BOSS_OD      = 8;     // threaded boss outer diameter
BOSS_ID      = 2.5;   // M3 tap hole (self-tap into PLA)

// ─── PERFBOARD ───────────────────────────────────────────────────
PERF_W       = 70;    // along enclosure width
PERF_D       = 50;    // along enclosure depth
PERF_Y_OFF   = -8;    // shifted toward front (negative Y = front)
STANDOFF_H   = 5;
STANDOFF_OD  = 5;
STANDOFF_ID  = 2.2;   // M2 screw
STANDOFF_INSET = 2;   // from perfboard edge [v2: was 3]

// ─── RUBBER FEET ─────────────────────────────────────────────────
FOOT_SIZE    = 13;    // 13x13mm square
FOOT_DEPTH   = 0.8;   // recess depth in bottom shell
FOOT_INSET   = 12;    // from corner

// ─── BRANDING ────────────────────────────────────────────────────
BRAND_DEPTH  = 1.0;   // engraving depth [v3: was 0.6, deeper for cleaner print]
LABEL_DEPTH  = 1.0;   // label deboss depth [v3: was 0.6, deeper for cleaner print]

// ─── TOP EDGE FILLET ─────────────────────────────────────────────
FILLET_R     = 1.5;   // radius of top edge fillet

// ─── COMPUTED POSITIONS ──────────────────────────────────────────
// Origin at enclosure center, bottom at Z=0

// Joystick centers on top panel
joy1_x = -JOY_CC/2;
joy2_x =  JOY_CC/2;
joy_y  = JOY_Y_OFF;

// Status LED: centered horizontally, moved toward back [v2]
status_x = 0;
status_y = joy_y + STATUS_Y_OFFSET;  // [v2: was joy_y, now +10]

// Fine LEDs: inboard of each joystick, at joystick Y
fine1_x = joy1_x + FINE_LED_INBOARD;
fine2_x = joy2_x - FINE_LED_INBOARD;
fine_y  = joy_y;

// Toggle: centered, below fine LEDs
toggle_x = 0;
toggle_y = joy_y - TOGGLE_Y_OFF;

// All 8 screw positions: 4 corners + 4 center (replacing pillars) [v2]
// Pillar positions from v1: X=+/-20, back Y=35, front Y=-38
screw_positions = [
    // 4 corners
    [ ENC_W/2 - SCREW_INSET,  ENC_D/2 - SCREW_INSET],
    [-ENC_W/2 + SCREW_INSET,  ENC_D/2 - SCREW_INSET],
    [ ENC_W/2 - SCREW_INSET, -ENC_D/2 + SCREW_INSET],
    [-ENC_W/2 + SCREW_INSET, -ENC_D/2 + SCREW_INSET],
    // 4 center (former pillar positions)
    [-20,  ENC_D/2 - 15],
    [ 20,  ENC_D/2 - 15],
    [-20, -(ENC_D/2 - 12)],
    [ 20, -(ENC_D/2 - 12)]
];

// Rubber feet only on 4 corners (not on center screws) [v2]
foot_positions = [
    [ ENC_W/2 - FOOT_INSET - FOOT_SIZE/2,  ENC_D/2 - FOOT_INSET - FOOT_SIZE/2],
    [-ENC_W/2 + FOOT_INSET + FOOT_SIZE/2,  ENC_D/2 - FOOT_INSET - FOOT_SIZE/2],
    [ ENC_W/2 - FOOT_INSET - FOOT_SIZE/2, -ENC_D/2 + FOOT_INSET + FOOT_SIZE/2],
    [-ENC_W/2 + FOOT_INSET + FOOT_SIZE/2, -ENC_D/2 + FOOT_INSET + FOOT_SIZE/2]
];

// Perfboard standoff positions [v2: inset 2mm from edge, was 3mm]
perf_cx = 0;
perf_cy = PERF_Y_OFF;
standoff_positions = [
    [perf_cx - PERF_W/2 + STANDOFF_INSET, perf_cy - PERF_D/2 + STANDOFF_INSET],
    [perf_cx + PERF_W/2 - STANDOFF_INSET, perf_cy - PERF_D/2 + STANDOFF_INSET],
    [perf_cx - PERF_W/2 + STANDOFF_INSET, perf_cy + PERF_D/2 - STANDOFF_INSET],
    [perf_cx + PERF_W/2 - STANDOFF_INSET, perf_cy + PERF_D/2 - STANDOFF_INSET]
];

// ─── Top panel label positions [v2: new] ─────────────────────────
// "AXLE TRACKER" on back of top panel, centered between status LED top edge and back edge
axle_tracker_y = (status_y + LED_RGB_D/2 + ENC_D/2) / 2;  // ~32.5

// "LOCK" above status LED
lock_label_y = status_y + LED_RGB_D/2 + 4;  // ~19

// "FINE" between blue LEDs at their Y position
fine_label_y = fine_y;

// FINE pointer triangles [v3: replaced dashes with directional pointers]
fine_tri_base = 3;     // triangle base height (near text side)
fine_tri_len  = 5;     // triangle length toward LED
fine_tri_depth = 1.0;  // deboss depth (matches LABEL_DEPTH)

// ─── Back panel label positions [v2] ─────────────────────────────
// DIVIDIA TECHNOLOGIES centered vertically on back panel
// Zone: from top edge (ENC_H) down to above USB-C area (~15mm from bottom)
dividia_z = ENC_H/2 + 8;       // DIVIDIA vertical center
tech_z    = dividia_z - 9;     // TECHNOLOGIES below DIVIDIA

// recal label below the target ring (lowercase, subordinate utility label)
recal_label_z = RECAL_Y - RECAL_RECESS_D/2 - 4;

// ─── MODULES ─────────────────────────────────────────────────────

module rounded_rect(w, d, r) {
    offset(r) square([w - 2*r, d - 2*r], center=true);
}

module rounded_box(w, d, h, r) {
    linear_extrude(h)
        rounded_rect(w, d, r);
}

// Top edge fillet: rounds the top perimeter edge of the enclosure.
module fillet_top_box(w, d, h, corner_r, fillet_r) {
    translate([0, 0, 0])
        linear_extrude(h - fillet_r)
            rounded_rect(w, d, corner_r);
    translate([0, 0, h - fillet_r])
        minkowski() {
            linear_extrude(0.01)
                rounded_rect(w - 2*fillet_r, d - 2*fillet_r, max(1, corner_r - fillet_r));
            sphere(r=fillet_r, $fn=64);  // [v3: was 24, higher res eliminates junction ridge]
        }
}

// ─── TOP SHELL ───────────────────────────────────────────────────
// Main body. Top panel + walls extending down.
// Step joint: outer half of wall bottom is STEP_H shorter.
module top_shell() {
    top_z = BOT_WALL_H - STEP_H;  // bottom of top shell outer walls
    top_inner_z = BOT_WALL_H;     // bottom of top shell inner step (extends lower)

    // Recal cradle dimensions (shared between union and difference)
    recal_wall_y = ENC_D/2 - WALL;
    cradle_w = RECAL_BTN_W + 2*RECAL_BTN_TOL;
    cradle_h = RECAL_BTN_H + 2*RECAL_BTN_TOL;
    cradle_d = RECAL_CRADLE_DEPTH;
    cradle_outer_w = cradle_w + 2*RECAL_SNAP_T;
    cradle_outer_h = cradle_h + 2*RECAL_SNAP_T;

    difference() {
        union() {
            // Main shell: outer walls + top panel (filleted top edges)
            // Outer walls go down to top_z, inner step goes down to top_z
            // Step joint: outer half shorter, inner half full depth
            translate([0, 0, top_z])
                difference() {
                    fillet_top_box(ENC_W, ENC_D, ENC_H - top_z, CORNER_R, FILLET_R);
                    // Hollow interior
                    translate([0, 0, -0.01])
                        rounded_box(ENC_W - 2*WALL, ENC_D - 2*WALL,
                                    ENC_H - top_z - TOP_T + 0.01,
                                    CORNER_R - WALL);
                    // Step cutout: remove outer half of wall for bottom STEP_H
                    // This leaves only the inner STEP_W standing at the bottom edge
                    difference() {
                        translate([0, 0, -0.01])
                            rounded_box(ENC_W + 0.02, ENC_D + 0.02,
                                        STEP_H + 0.01, CORNER_R);
                        translate([0, 0, -0.02])
                            rounded_box(ENC_W - 2*STEP_W - STEP_TOL,
                                        ENC_D - 2*STEP_W - STEP_TOL,
                                        STEP_H + 0.04,
                                        max(1, CORNER_R - STEP_W));
                    }
                }

            // Screw bosses (threaded posts, top panel down through cavity)
            for (pos = screw_positions) {
                boss_h = ENC_H - TOP_T - top_z;
                translate([pos[0], pos[1], top_z])
                    difference() {
                        cylinder(d=BOSS_OD, h=boss_h, $fn=24);
                        translate([0, 0, -0.01])
                            cylinder(d=BOSS_ID, h=boss_h + 0.02, $fn=16);
                    }
            }

            // RGB LED underside collar (light shield + extended seat for 10mm LED)
            translate([status_x, status_y, ENC_H - TOP_T - LED_RGB_COLLAR_H])
                difference() {
                    cylinder(d=LED_RGB_COLLAR_OD, h=LED_RGB_COLLAR_H, $fn=32);
                    translate([0, 0, -0.01])
                        cylinder(d=LED_RGB_D, h=LED_RGB_COLLAR_H + 0.02, $fn=32);
                }

            // Underside ribs — route BETWEEN features, not through them
            rib_z = ENC_H - TOP_T - 3;
            rib_h = 3;
            // Outer ribs: at X = +/-82
            for (rx = [-82, 82]) {
                translate([rx - 1.5, -ENC_D/2 + WALL, rib_z])
                    cube([3, ENC_D - 2*WALL, rib_h]);
            }
            // Inner ribs: at X = +/-15, skip around LED/toggle zone
            for (rx = [-15, 15]) {
                // Back segment
                translate([rx - 1.5, status_y + 22, rib_z])
                    cube([3, ENC_D/2 - WALL - (status_y + 22), rib_h]);
                // Front segment
                translate([rx - 1.5, -ENC_D/2 + WALL, rib_z])
                    cube([3, ENC_D/2 + (toggle_y - 12) - WALL, rib_h]);
            }

            // ── RECAL BUTTON MOUNT [v3: snap-in cradle] ──
            // Pocket on back wall interior seats button body flush against wall.
            // Snap tabs (top/bottom) flex outward during insertion, lock behind body.
            // Open left/right sides for prong clearance and wiring access.
            // Actuator pokes through wall into pinhole channel.
            // Cradle body: solid block on wall, pocket subtracted in difference()
            translate([RECAL_X_OFF - cradle_outer_w/2,
                       recal_wall_y - cradle_d,
                       RECAL_Y - cradle_outer_h/2])
                cube([cradle_outer_w, cradle_d + 0.01, cradle_outer_h]);
        }

        // ── CUTOUTS IN TOP PANEL ──

        // Joystick barrel holes (36mm)
        for (jx = [joy1_x, joy2_x]) {
            translate([jx, joy_y, ENC_H - TOP_T - 0.01])
                cylinder(d=JOY_BARREL + TOL, h=TOP_T + 0.02, $fn=64);
        }

        // Joystick M2.5 screw holes [v2: 3.8mm clearance]
        for (jx = [joy1_x, joy2_x]) {
            for (sx = [-1, 1]) {
                for (sy = [-1, 1]) {
                    translate([jx + sx*JOY_SCREW_SQ/2,
                               joy_y + sy*JOY_SCREW_SQ/2,
                               ENC_H - TOP_T - 0.01])
                        cylinder(d=JOY_SCREW_D, h=TOP_T + 0.02, $fn=16);
                }
            }
        }

        // Status LED hole (10.1mm) with chamfer [v2: moved to status_y = +10]
        translate([status_x, status_y, ENC_H - TOP_T - 0.01])
            cylinder(d=LED_RGB_D, h=TOP_T + 0.02, $fn=32);
        translate([status_x, status_y, ENC_H - LED_RGB_CHAMFER])
            cylinder(d1=LED_RGB_D, d2=LED_RGB_D + 2*LED_RGB_CHAMFER,
                     h=LED_RGB_CHAMFER + 0.01, $fn=32);

        // Fine LED holes (5.1mm) with chamfer
        for (fx = [fine1_x, fine2_x]) {
            translate([fx, fine_y, ENC_H - TOP_T - 0.01])
                cylinder(d=LED_FINE_D, h=TOP_T + 0.02, $fn=24);
            translate([fx, fine_y, ENC_H - LED_FINE_CHAMFER])
                cylinder(d1=LED_FINE_D, d2=LED_FINE_D + 2*LED_FINE_CHAMFER,
                         h=LED_FINE_CHAMFER + 0.01, $fn=24);
        }

        // Toggle switch hole
        translate([toggle_x, toggle_y, ENC_H - TOP_T - 0.01])
            cylinder(d=TOGGLE_D, h=TOP_T + 0.02, $fn=32);

        // ── TOP PANEL LABELS (debossed from top surface) ──
        // Top surface faces +Z. Text is extruded downward into the panel.
        // No mirror needed — text reads correctly when viewed from above.

        // "AXLE TRACKER" — back of top panel [v2: was AXLE CONTROLLER on front]
        translate([0, axle_tracker_y, ENC_H - BRAND_DEPTH])
            linear_extrude(BRAND_DEPTH + 0.01)
                text("AXLE TRACKER", size=6, halign="center",
                     valign="center", font="Liberation Sans:style=Bold");

        // "LOCK" — above status LED [v2: new]
        translate([0, lock_label_y, ENC_H - LABEL_DEPTH])
            linear_extrude(LABEL_DEPTH + 0.01)
                text("LOCK", size=4.5, halign="center",
                     valign="center", font="Liberation Sans:style=Bold");

        // "FINE" — centered between blue LEDs [v2: new]
        translate([0, fine_label_y, ENC_H - LABEL_DEPTH])
            linear_extrude(LABEL_DEPTH + 0.01)
                text("FINE", size=4, halign="center",
                     valign="center", font="Liberation Sans:style=Bold");

        // FINE pointer triangles — point from text toward each LED [v3: replaced dashes]
        // Left triangle: ▶ pointing toward left LED
        // Center the triangle in the gap between text edge and LED edge
        fine_text_half_w = 9;  // approximate half-width of "FINE" at size 4
        fine_left_gap_center = (fine1_x + LED_FINE_D/2 + (-fine_text_half_w)) / 2;
        fine_right_gap_center = (fine2_x - LED_FINE_D/2 + fine_text_half_w) / 2;

        // Left pointer (◀ toward left LED at negative X)
        translate([fine_left_gap_center, fine_label_y, ENC_H - fine_tri_depth])
            linear_extrude(fine_tri_depth + 0.01)
                polygon([
                    [-fine_tri_len/2, 0],                 // tip (toward LED, -X)
                    [ fine_tri_len/2,  fine_tri_base/2],  // base top
                    [ fine_tri_len/2, -fine_tri_base/2]   // base bottom
                ]);

        // Right pointer (▶ toward right LED at positive X)
        translate([fine_right_gap_center, fine_label_y, ENC_H - fine_tri_depth])
            linear_extrude(fine_tri_depth + 0.01)
                polygon([
                    [ fine_tri_len/2, 0],                 // tip (toward LED, +X)
                    [-fine_tri_len/2,  fine_tri_base/2],  // base top
                    [-fine_tri_len/2, -fine_tri_base/2]   // base bottom
                ]);

        // ── CUTOUTS IN BACK WALL ──

        // USB-C cutout
        translate([-USBC_W/2, ENC_D/2 - WALL - 0.01, USBC_Y - USBC_H/2])
            cube([USBC_W, WALL + 0.02, USBC_H]);

        // USB-C cable recess on outside of back panel [v2: new]
        translate([-USBC_RECESS_W/2,
                   ENC_D/2 - USBC_RECESS_D,
                   USBC_Y - USBC_RECESS_H/2])
            cube([USBC_RECESS_W, USBC_RECESS_D + 0.01, USBC_RECESS_H]);

        // ── RECAL CRADLE POCKET CUTOUTS [v3] ──
        // Strategy: cut the main pocket for button body, but leave snap lips
        // at top and bottom by cutting in two zones (center + relief slots).
        //
        // Center pocket: full button cavity, slightly shorter in Z than cradle
        // so the top/bottom snap_t material remains as tabs.
        translate([RECAL_X_OFF - cradle_w/2,
                   recal_wall_y - cradle_d - 0.01,
                   RECAL_Y - cradle_h/2])
            cube([cradle_w, cradle_d + 0.02, cradle_h]);

        // Relief slots behind snap tabs: allow tabs to flex outward.
        // Cut through the tab material except the lip at the pocket face.
        // Top relief: behind the top snap tab
        translate([RECAL_X_OFF - cradle_w/2,
                   recal_wall_y - cradle_d - 0.01,
                   RECAL_Y + cradle_h/2])
            cube([cradle_w, cradle_d - RECAL_SNAP_LIP, RECAL_SNAP_T + 0.01]);
        // Bottom relief: behind the bottom snap tab
        translate([RECAL_X_OFF - cradle_w/2,
                   recal_wall_y - cradle_d - 0.01,
                   RECAL_Y - cradle_h/2 - RECAL_SNAP_T])
            cube([cradle_w, cradle_d - RECAL_SNAP_LIP, RECAL_SNAP_T + 0.01]);

        // Recal — recessed target ring on outside of back panel [v2: new]
        translate([RECAL_X_OFF, ENC_D/2 + 0.01, RECAL_Y])
            rotate([90, 0, 0])
                cylinder(d=RECAL_RECESS_D, h=RECAL_RECESS_DEPTH + 0.01, $fn=32);
        // Recal pinhole: 2mm through full wall (paperclip access from exterior)
        translate([RECAL_X_OFF, ENC_D/2 - WALL - 0.01, RECAL_Y])
            rotate([-90, 0, 0])
                cylinder(d=RECAL_D, h=WALL + 0.02, $fn=16);
        // Actuator bore: wider channel from interior wall face into pocket
        // Lets the 4mm actuator + 2mm extension pass through without binding
        translate([RECAL_X_OFF, recal_wall_y - RECAL_BTN_ACT_EXT - 0.01, RECAL_Y])
            rotate([-90, 0, 0])
                cylinder(d=RECAL_BTN_ACT_D + TOL, h=RECAL_BTN_ACT_EXT + 0.02, $fn=16);

        // ── BACK PANEL LABELS (debossed from outside surface) ──
        // Back panel faces +Y. Text extruded in -Y direction (into wall).
        // Must mirror X so text reads correctly from outside.

        // "DIVIDIA" [v2: recentered vertically]
        translate([0, ENC_D/2 + 0.01, dividia_z])
            rotate([90, 0, 0])
                linear_extrude(BRAND_DEPTH + 0.02)
                    mirror([1, 0, 0])
                        text("DIVIDIA", size=7, halign="center",
                             valign="center", font="Liberation Sans:style=Bold",
                             spacing=1.4);  // [v3: was 1.3, wider to balance TECHNOLOGIES width]

        // "TECHNOLOGIES" below DIVIDIA [v2: new]
        translate([0, ENC_D/2 + 0.01, tech_z])
            rotate([90, 0, 0])
                linear_extrude(BRAND_DEPTH + 0.02)
                    mirror([1, 0, 0])
                        text("TECHNOLOGIES", size=3.8, halign="center",
                             valign="center", font="Liberation Sans:style=Bold",
                             spacing=1.1);

        // "USB-C" below USB-C port [v2: new]
        translate([0, ENC_D/2 + 0.01, USBC_Y - USBC_RECESS_H/2 - 3.5])
            rotate([90, 0, 0])
                linear_extrude(LABEL_DEPTH + 0.02)
                    mirror([1, 0, 0])
                        text("USB-C", size=2.5, halign="center",
                             valign="center", font="Liberation Sans");

        // "recal" below target ring (lowercase, smaller, utility label) [v2]
        translate([RECAL_X_OFF, ENC_D/2 + 0.01, recal_label_z])
            rotate([90, 0, 0])
                linear_extrude(LABEL_DEPTH + 0.02)
                    mirror([1, 0, 0])
                        text("recal", size=2.5, halign="center",
                             valign="center", font="Liberation Sans");
    }
}

// ─── BOTTOM SHELL ────────────────────────────────────────────────
// Shallow tray. Step joint: inner half of wall top is STEP_H shorter.
module bottom_shell() {
    difference() {
        union() {
            // Tray: floor + short perimeter walls with step
            difference() {
                rounded_box(ENC_W, ENC_D, BOT_WALL_H, CORNER_R);
                // Hollow interior (leave floor + walls)
                translate([0, 0, BOT_T])
                    rounded_box(ENC_W - 2*WALL, ENC_D - 2*WALL,
                                BOT_WALL_H - BOT_T + 0.01,
                                CORNER_R - WALL);
                // Step cutout: remove inner half of wall for top STEP_H
                // This leaves only the outer STEP_W standing at the top edge
                translate([0, 0, BOT_WALL_H - STEP_H])
                    difference() {
                        rounded_box(ENC_W - 2*STEP_W + STEP_TOL,
                                    ENC_D - 2*STEP_W + STEP_TOL,
                                    STEP_H + 0.01,
                                    max(1, CORNER_R - STEP_W));
                        translate([0, 0, -0.01])
                            rounded_box(ENC_W - 2*WALL, ENC_D - 2*WALL,
                                        STEP_H + 0.03,
                                        CORNER_R - WALL);
                    }
            }

            // Perfboard standoffs [v2: inset 2mm from edge]
            for (pos = standoff_positions) {
                translate([pos[0], pos[1], BOT_T])
                    difference() {
                        cylinder(d=STANDOFF_OD, h=STANDOFF_H, $fn=20);
                        translate([0, 0, -0.01])
                            cylinder(d=STANDOFF_ID, h=STANDOFF_H + 0.02, $fn=16);
                    }
            }

            // USB-C breakout board mount posts [v2: corrected spacing and height]
            for (pos = usbc_mount_positions) {
                translate([pos[0], pos[1], BOT_T])
                    difference() {
                        cylinder(d=USBC_STANDOFF_OD, h=USBC_STANDOFF_H, $fn=20);
                        translate([0, 0, -0.01])
                            cylinder(d=USBC_STANDOFF_ID, h=USBC_STANDOFF_H + 0.02, $fn=16);
                    }
            }

            // Screw bosses — all 8 positions [v3: subtract STEP_H for shell overlap]
            for (pos = screw_positions) {
                translate([pos[0], pos[1], BOT_T])
                    cylinder(d=BOSS_OD, h=BOT_WALL_H - BOT_T - STEP_H, $fn=24);
            }
        }

        // Screw holes (countersunk from bottom) — all 8 positions
        for (pos = screw_positions) {
            translate([pos[0], pos[1], -0.01])
                cylinder(d=SCREW_D, h=BOT_WALL_H + 0.02, $fn=20);
            translate([pos[0], pos[1], -0.01])
                cylinder(d1=SCREW_HEAD_D, d2=SCREW_D,
                         h=SCREW_HEAD_DEPTH + 0.01, $fn=20);
        }

        // Rubber foot recesses (4 corners only)
        for (pos = foot_positions) {
            translate([pos[0] - FOOT_SIZE/2, pos[1] - FOOT_SIZE/2, -0.01])
                cube([FOOT_SIZE, FOOT_SIZE, FOOT_DEPTH + 0.01]);
        }

        // USB-C cutout in back wall (if it intersects bottom shell height)
        if (USBC_Y + USBC_H/2 > 0 && USBC_Y - USBC_H/2 < BOT_WALL_H)
            translate([-USBC_W/2, ENC_D/2 - WALL - 0.01,
                       USBC_Y - USBC_H/2])
                cube([USBC_W, WALL + 0.02, USBC_H]);

        // USB-C cable recess in back wall (if it intersects bottom shell)
        if (USBC_Y + USBC_RECESS_H/2 > 0 && USBC_Y - USBC_RECESS_H/2 < BOT_WALL_H)
            translate([-USBC_RECESS_W/2,
                       ENC_D/2 - USBC_RECESS_D,
                       USBC_Y - USBC_RECESS_H/2])
                cube([USBC_RECESS_W, USBC_RECESS_D + 0.01, USBC_RECESS_H]);

        // ── BOTTOM PANEL LABELS (debossed from bottom surface) ──
        // Bottom surface faces -Z. Text is viewed when enclosure is flipped over.
        // Must mirror X so text reads correctly from below.

        // Product info block — centered on bottom panel
        // "DIVIDIA TECHNOLOGIES"
        translate([0, 8, -0.01])
            mirror([1, 0, 0])
                linear_extrude(LABEL_DEPTH + 0.01)
                    text("DIVIDIA TECHNOLOGIES", size=4, halign="center",
                         valign="center", font="Liberation Sans:style=Bold");

        // "MODEL AT-C100"
        translate([0, 2, -0.01])
            mirror([1, 0, 0])
                linear_extrude(LABEL_DEPTH + 0.01)
                    text("MODEL AT-C100", size=3.5, halign="center",
                         valign="center", font="Liberation Sans:style=Bold");

        // "S/N: __________"
        translate([0, -3.5, -0.01])
            mirror([1, 0, 0])
                linear_extrude(LABEL_DEPTH + 0.01)
                    text("S/N: __________", size=3, halign="center",
                         valign="center", font="Liberation Sans");

        // "5V DC · USB-C"
        translate([0, -8.5, -0.01])
            mirror([1, 0, 0])
                linear_extrude(LABEL_DEPTH + 0.01)
                    text("5V DC  ·  USB-C", size=3, halign="center",
                         valign="center", font="Liberation Sans");

        // "TEXAS, USA"
        translate([0, -13.5, -0.01])
            mirror([1, 0, 0])
                linear_extrude(LABEL_DEPTH + 0.01)
                    text("TEXAS, USA", size=3, halign="center",
                         valign="center", font="Liberation Sans");

        // "www.dividia.net"
        translate([0, -18.5, -0.01])
            mirror([1, 0, 0])
                linear_extrude(LABEL_DEPTH + 0.01)
                    text("www.dividia.net", size=2.8, halign="center",
                         valign="center", font="Liberation Sans");

        // [v3: removed S/N sticker recess — caused slicer artifacts at 0.3mm depth]
    }
}

// [v3: removed recal_retention_plate — replaced by snap-in cradle on wall]

// ─── ASSEMBLY / PART SELECTION ───────────────────────────────────

if (PART == "top") {
    // Print orientation: flip upside down (top surface on bed for smooth finish)
    translate([0, 0, ENC_H])
        rotate([180, 0, 0])
            top_shell();
}
else if (PART == "bottom") {
    // Print as-is (bottom surface on bed)
    bottom_shell();
}
else if (PART == "assembly") {
    // Preview: both halves in assembled position
    color("SteelBlue", 0.7) top_shell();
    color("SlateGray", 0.7) bottom_shell();

    // Ghost joysticks
    %for (jx = [joy1_x, joy2_x]) {
        translate([jx, joy_y, ENC_H])
            cylinder(d=35, h=42, $fn=32);
        translate([jx, joy_y, ENC_H - TOP_T - JOY_BASE_DEPTH])
            cylinder(d=40, h=JOY_BASE_DEPTH, $fn=4);
    }

    // Ghost perfboard
    %translate([perf_cx - PERF_W/2, perf_cy - PERF_D/2,
                BOT_T + STANDOFF_H])
        cube([PERF_W, PERF_D, 2]);

    // Ghost recal button (6x6x3mm body in snap cradle)
    recal_wall_y = ENC_D/2 - WALL;
    %translate([RECAL_X_OFF - RECAL_BTN_W/2,
                recal_wall_y - RECAL_BTN_DEPTH,
                RECAL_Y - RECAL_BTN_H/2])
        cube([RECAL_BTN_W, RECAL_BTN_DEPTH, RECAL_BTN_H]);
}

// ─── RENDER SETTINGS ─────────────────────────────────────────────
$fn = 48;
