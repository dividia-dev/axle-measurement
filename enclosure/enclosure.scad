// CV Axle Controller — Enclosure (Two-Piece Shell)
// Optimized for Bambu Lab A1, 0.4mm nozzle, PLA
// All dimensions in mm. Source: docs/enclosure-component-spec.md
//
// Design: Top shell = main body (tall). Bottom shell = shallow tray with
// nesting lip. Alignment via friction-fit pins (bottom posts, top sockets).
//
// Usage:
//   - Render top shell:    set PART = "top"
//   - Render bottom shell: set PART = "bottom"
//   - Preview assembled:   set PART = "assembly"
//   - Export STL:          set PART to "top" or "bottom", then F6 → Export

// ─── PART SELECTOR ───────────────────────────────────────────────
PART = "assembly"; // "top", "bottom", or "assembly"

// ─── PRINT TUNING (Bambu Lab A1, 0.4mm nozzle) ──────────────────
NOZZLE       = 0.4;
LAYER_HEIGHT = 0.2;
TOL          = 0.3;   // general fit tolerance
BRIDGE_MAX   = 15;    // max unsupported bridge span (mm)

// ─── ENCLOSURE DIMENSIONS ────────────────────────────────────────
ENC_W        = 190;   // width (X)
ENC_D        = 100;   // depth (Y)
ENC_H        = 42;    // total external height (Z)
CORNER_R     = 8;     // corner radius
WALL         = 3;     // wall thickness
TOP_T        = 4;     // top panel thickness
BOT_T        = 3;     // bottom panel thickness

// ─── SHELL SPLIT ─────────────────────────────────────────────────
// Bottom = shallow tray. Top = main body that seats into bottom.
BOT_WALL_H   = 10;    // total bottom shell height (floor + short walls)
LIP_H        = 2;     // nesting lip overlap
LIP_W        = 1.5;   // nesting lip wall thickness

// Top shell walls extend down to overlap into bottom shell
// Top shell total height = ENC_H - BOT_WALL_H + LIP_H = 34mm

// ─── JOYSTICK ────────────────────────────────────────────────────
JOY_CC       = 100;   // center-to-center spacing
JOY_Y_OFF    = -4;    // Y offset from center (toward back = negative)
JOY_BARREL   = 36;    // panel cutout diameter
JOY_FLANGE   = 54;    // flange OD (clear zone on top surface)
JOY_SCREW_SQ = 32;    // M2.5 screw pattern (square, on-center)
JOY_SCREW_D  = 2.8;   // M2.5 clearance hole
JOY_BASE_SQ  = 42;    // base housing clearance
JOY_BASE_DEPTH = 25;  // below-panel depth

// ─── TOGGLE SWITCH ───────────────────────────────────────────────
TOGGLE_D     = 11.5 + TOL;  // mounting hole + tolerance
TOGGLE_Y_OFF = 14;    // mm below status LED (toward front)

// ─── LEDs ────────────────────────────────────────────────────────
LED_FINE_D   = 5.1;   // 5mm blue LEDs, friction fit
LED_FINE_CHAMFER = 0.3;
LED_RGB_D    = 10.1;  // 10mm RGB LED, friction fit
LED_RGB_COLLAR_OD = 14;   // underside collar outer diameter
LED_RGB_COLLAR_H  = 2;    // collar height (extends below panel)
LED_RGB_CHAMFER = 0.3;

// Fine LED positions: inboard of each joystick flange
FINE_LED_INBOARD = 32; // distance from joystick center toward enclosure center

// ─── USB-C BREAKOUT ──────────────────────────────────────────────
USBC_W       = 10 + TOL;  // cutout width
USBC_H       = 4 + TOL;   // cutout height
USBC_Y       = 10;    // center of cutout from bottom of enclosure (Z=0)
USBC_PCB_W   = 18;    // breakout PCB width
USBC_PCB_H   = 13;    // breakout PCB height (along Z)
// Mount posts: 2x M2 screw posts on bottom shell floor, near back wall
// PCB sits on 2mm perfboard-height standoffs, USB-C connector on top faces the cutout
// Mounting holes estimated ~12mm apart horizontally, centered on PCB
USBC_MOUNT_SPACING = 12;   // horizontal distance between mount holes
USBC_MOUNT_Y = ENC_D/2 - WALL - USBC_PCB_H/2;  // center of PCB (against back wall)
USBC_STANDOFF_H = 2;       // raise PCB to align connector with wall cutout
USBC_STANDOFF_OD = 5;
USBC_STANDOFF_ID = 2.2;    // M2

usbc_mount_positions = [
    [-USBC_MOUNT_SPACING/2, USBC_MOUNT_Y],
    [ USBC_MOUNT_SPACING/2, USBC_MOUNT_Y]
];

// ─── RECAL PINHOLE ───────────────────────────────────────────────
RECAL_D      = 2;     // pinhole diameter
RECAL_X_OFF  = 40;    // mm right of center (viewed from back)
RECAL_Y      = 22;    // mm from bottom of enclosure (Z=0)

// ─── VENTILATION ─────────────────────────────────────────────────
// REMOVED: sealed enclosure for cement facility dust protection.
// Heat generation <1W, no ventilation needed.

// ─── M3 CORNER SCREWS ───────────────────────────────────────────
SCREW_INSET  = 12;    // from edge
SCREW_D      = 3.4;   // M3 clearance
SCREW_HEAD_D = 6.2;   // M3 countersunk head
SCREW_HEAD_DEPTH = 1.8;
BOSS_OD      = 8;     // threaded boss outer diameter
BOSS_ID      = 2.5;   // M3 tap hole (self-tap into PLA)

// ─── STRUCTURAL SUPPORT PILLARS ──────────────────────────────────
// 4x center-area pillars for enclosure rigidity + alignment.
// Male pin on bottom post, female socket in top boss. Friction-fit.
PILLAR_D         = 8;     // pillar body diameter
PILLAR_PIN_D     = 5;     // male pin diameter (narrower tip)
PILLAR_PIN_H     = 6;     // male pin height (extends above bottom pillar)
PILLAR_SOCKET_D  = PILLAR_PIN_D + TOL;  // female socket diameter
PILLAR_SOCKET_H  = PILLAR_PIN_H + 1;    // female socket depth (slightly deeper)
PILLAR_SPREAD    = 20;    // X offset from center
PILLAR_BACK_Y    = ENC_D/2 - 15;    // behind joystick bases, near back wall
PILLAR_FRONT_Y   = -(ENC_D/2 - 12); // in front of perfboard, near front wall
// Perfboard spans X:-35..+35, Y:-33..+17. Back pillars at Y=35 clear it.
// Front pillars at Y=-38 clear it (perfboard min Y = -33).

pillar_positions = [
    [-PILLAR_SPREAD,  PILLAR_BACK_Y],
    [ PILLAR_SPREAD,  PILLAR_BACK_Y],
    [-PILLAR_SPREAD,  PILLAR_FRONT_Y],
    [ PILLAR_SPREAD,  PILLAR_FRONT_Y]
];

// ─── PERFBOARD ───────────────────────────────────────────────────
PERF_W       = 70;    // along enclosure width
PERF_D       = 50;    // along enclosure depth
PERF_Y_OFF   = -8;    // shifted toward front (negative Y = front)
STANDOFF_H   = 5;
STANDOFF_OD  = 5;
STANDOFF_ID  = 2.2;   // M2 screw
STANDOFF_INSET = 3;   // from perfboard edge

// ─── RUBBER FEET ─────────────────────────────────────────────────
FOOT_SIZE    = 13;    // 13x13mm square
FOOT_DEPTH   = 0.8;   // recess depth in bottom shell
FOOT_INSET   = 12;    // from corner

// ─── BRANDING ────────────────────────────────────────────────────
BRAND_DEPTH  = 0.6;   // engraving depth

// ─── TOP EDGE FILLET ─────────────────────────────────────────────
FILLET_R     = 1.5;   // radius of top edge fillet

// ─── COMPUTED POSITIONS ──────────────────────────────────────────
// Origin at enclosure center, bottom at Z=0

// Joystick centers on top panel
joy1_x = -JOY_CC/2;
joy2_x =  JOY_CC/2;
joy_y  = JOY_Y_OFF;

// Status LED: centered between joysticks
status_x = 0;
status_y = joy_y;

// Fine LEDs: inboard of each joystick
fine1_x = joy1_x + FINE_LED_INBOARD;
fine2_x = joy2_x - FINE_LED_INBOARD;
fine_y  = joy_y;

// Toggle: centered, below status LED
toggle_x = 0;
toggle_y = joy_y - TOGGLE_Y_OFF;

// Corner screw positions
screw_positions = [
    [ ENC_W/2 - SCREW_INSET,  ENC_D/2 - SCREW_INSET],
    [-ENC_W/2 + SCREW_INSET,  ENC_D/2 - SCREW_INSET],
    [ ENC_W/2 - SCREW_INSET, -ENC_D/2 + SCREW_INSET],
    [-ENC_W/2 + SCREW_INSET, -ENC_D/2 + SCREW_INSET]
];

// Perfboard standoff positions
perf_cx = 0;
perf_cy = PERF_Y_OFF;
standoff_positions = [
    [perf_cx - PERF_W/2 + STANDOFF_INSET, perf_cy - PERF_D/2 + STANDOFF_INSET],
    [perf_cx + PERF_W/2 - STANDOFF_INSET, perf_cy - PERF_D/2 + STANDOFF_INSET],
    [perf_cx - PERF_W/2 + STANDOFF_INSET, perf_cy + PERF_D/2 - STANDOFF_INSET],
    [perf_cx + PERF_W/2 - STANDOFF_INSET, perf_cy + PERF_D/2 - STANDOFF_INSET]
];

// Rubber foot positions
foot_positions = [
    [ ENC_W/2 - FOOT_INSET - FOOT_SIZE/2,  ENC_D/2 - FOOT_INSET - FOOT_SIZE/2],
    [-ENC_W/2 + FOOT_INSET + FOOT_SIZE/2,  ENC_D/2 - FOOT_INSET - FOOT_SIZE/2],
    [ ENC_W/2 - FOOT_INSET - FOOT_SIZE/2, -ENC_D/2 + FOOT_INSET + FOOT_SIZE/2],
    [-ENC_W/2 + FOOT_INSET + FOOT_SIZE/2, -ENC_D/2 + FOOT_INSET + FOOT_SIZE/2]
];

// ─── MODULES ─────────────────────────────────────────────────────

module rounded_rect(w, d, r) {
    offset(r) square([w - 2*r, d - 2*r], center=true);
}

module rounded_box(w, d, h, r) {
    linear_extrude(h)
        rounded_rect(w, d, r);
}

// hex_grid module removed — enclosure is sealed (no vents)

// Top edge fillet: rounds the top perimeter edge of the enclosure.
// Creates a rounded outer shell by intersecting the box with a
// minkowski-expanded slightly smaller box, applied only at the top.
module fillet_top_box(w, d, h, corner_r, fillet_r) {
    // Below the fillet zone: original sharp box
    translate([0, 0, 0])
        linear_extrude(h - fillet_r)
            rounded_rect(w, d, corner_r);
    // Fillet zone at top: minkowski of 2D profile with a sphere
    translate([0, 0, h - fillet_r])
        minkowski() {
            linear_extrude(0.01)
                rounded_rect(w - 2*fillet_r, d - 2*fillet_r, max(1, corner_r - fillet_r));
            sphere(r=fillet_r, $fn=24);
        }
}

// ─── TOP SHELL ───────────────────────────────────────────────────
// Main body of the enclosure. Top panel + walls extending down.
// Seats into the bottom shell's lip.
module top_shell() {
    top_z = BOT_WALL_H - LIP_H;  // bottom of top shell walls (overlaps into bottom)
    cavity_h = ENC_H - top_z - TOP_T;  // internal cavity height

    difference() {
        union() {
            // Main shell: outer walls + top panel (filleted top edges)
            translate([0, 0, top_z])
                difference() {
                    fillet_top_box(ENC_W, ENC_D, ENC_H - top_z, CORNER_R, FILLET_R);
                    // Hollow interior
                    translate([0, 0, -0.01])
                        rounded_box(ENC_W - 2*WALL, ENC_D - 2*WALL,
                                    ENC_H - top_z - TOP_T + 0.01,
                                    CORNER_R - WALL);
                }

            // Nesting lip: thin wall that drops inside the bottom shell
            translate([0, 0, top_z])
                difference() {
                    rounded_box(ENC_W - 2*WALL - TOL,
                                ENC_D - 2*WALL - TOL,
                                LIP_H, CORNER_R - WALL);
                    translate([0, 0, -0.01])
                        rounded_box(ENC_W - 2*WALL - TOL - 2*LIP_W,
                                    ENC_D - 2*WALL - TOL - 2*LIP_W,
                                    LIP_H + 0.02,
                                    max(1, CORNER_R - WALL - LIP_W));
                }

            // Screw bosses (threaded posts, top panel down to near bottom)
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
            // 14mm OD, 10.1mm ID, 2mm tall, hanging below top panel
            translate([status_x, status_y, ENC_H - TOP_T - LED_RGB_COLLAR_H])
                difference() {
                    cylinder(d=LED_RGB_COLLAR_OD, h=LED_RGB_COLLAR_H, $fn=32);
                    translate([0, 0, -0.01])
                        cylinder(d=LED_RGB_D, h=LED_RGB_COLLAR_H + 0.02, $fn=32);
                }

            // Structural pillar bosses (solid cylinders hanging from top panel)
            // Female sockets are cut into these in the difference section below
            for (pos = pillar_positions) {
                pillar_boss_h = ENC_H - TOP_T - top_z;  // full cavity height
                translate([pos[0], pos[1], top_z])
                    cylinder(d=PILLAR_D, h=pillar_boss_h, $fn=24);
            }

            // Underside ribs — route BETWEEN features, not through them
            // Joystick barrel edge is at X ~= +/-68. Wall inner edge at +/-92.
            // Outer ribs: at X = +/-82 (centered between barrel edge and wall)
            rib_z = ENC_H - TOP_T - 3;
            rib_h = 3;
            for (rx = [-82, 82]) {
                translate([rx - 1.5, -ENC_D/2 + WALL, rib_z])
                    cube([3, ENC_D - 2*WALL, rib_h]);
            }
            // Inner ribs: at X = +/-15 (between LED/toggle zone and joystick barrels)
            // Skip the zone around LEDs and toggle (Y from toggle_y-8 to joy_y+8)
            for (rx = [-15, 15]) {
                // Back segment: from back wall to behind the feature cluster
                translate([rx - 1.5, joy_y + 22, rib_z])
                    cube([3, ENC_D/2 - WALL - (joy_y + 22), rib_h]);
                // Front segment: from front of feature cluster to front wall
                translate([rx - 1.5, -ENC_D/2 + WALL, rib_z])
                    cube([3, ENC_D/2 + (toggle_y - 12) - WALL, rib_h]);
            }
        }

        // ── CUTOUTS IN TOP PANEL ──

        // Joystick barrel holes (36mm)
        for (jx = [joy1_x, joy2_x]) {
            translate([jx, joy_y, ENC_H - TOP_T - 0.01])
                cylinder(d=JOY_BARREL + TOL, h=TOP_T + 0.02, $fn=64);
        }

        // Joystick M2.5 screw holes (4 per joystick)
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

        // Status LED hole (10.1mm) with chamfer
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

        // ── CUTOUTS IN BACK WALL ──

        // USB-C cutout
        translate([-USBC_W/2, ENC_D/2 - WALL - 0.01, USBC_Y - USBC_H/2])
            cube([USBC_W, WALL + 0.02, USBC_H]);

        // Recal pinhole
        translate([RECAL_X_OFF, ENC_D/2 - WALL - 0.01, RECAL_Y])
            rotate([-90, 0, 0])
                cylinder(d=RECAL_D, h=WALL + 0.02, $fn=16);

        // Vents removed — sealed enclosure for dust protection

        // Structural pillar female sockets (holes in bottom of top bosses)
        // Receives the male pin from the bottom shell pillars
        for (pos = pillar_positions) {
            translate([pos[0], pos[1], top_z - 0.01])
                cylinder(d=PILLAR_SOCKET_D, h=PILLAR_SOCKET_H + 0.01, $fn=24);
        }

        // "AXLE CONTROLLER" branding on front top panel (engraved)
        // Centered vertically between front edge and toggle hole
        // Front edge = -ENC_D/2 = -50, toggle bottom edge ~ -24
        // Center = (-50 + -24) / 2 = -37 = -ENC_D/2 + 13
        translate([0, -ENC_D/2 + 13, ENC_H - BRAND_DEPTH])
            linear_extrude(BRAND_DEPTH + 0.01)
                text("AXLE CONTROLLER", size=6, halign="center",
                     valign="center", font="Liberation Sans:style=Bold");

        // "DIVIDIA" branding on back panel (engraved from outside surface inward)
        translate([0, ENC_D/2 + 0.01, ENC_H/2 + 4])
            rotate([90, 0, 0])
                linear_extrude(BRAND_DEPTH + 0.02)
                    mirror([1, 0, 0])
                        text("DIVIDIA", size=7, halign="center",
                             valign="center", font="Liberation Sans:style=Bold",
                             spacing=1.3);
    }
}

// ─── BOTTOM SHELL ────────────────────────────────────────────────
// Shallow tray with nesting lip. Top shell seats down into it.
module bottom_shell() {
    difference() {
        union() {
            // Tray: floor + short perimeter walls
            difference() {
                rounded_box(ENC_W, ENC_D, BOT_WALL_H, CORNER_R);
                // Hollow interior (leave floor + walls)
                translate([0, 0, BOT_T])
                    rounded_box(ENC_W - 2*WALL, ENC_D - 2*WALL,
                                BOT_WALL_H - BOT_T + 0.01,
                                CORNER_R - WALL);
            }

            // Perfboard standoffs
            for (pos = standoff_positions) {
                translate([pos[0], pos[1], BOT_T])
                    difference() {
                        cylinder(d=STANDOFF_OD, h=STANDOFF_H, $fn=20);
                        translate([0, 0, -0.01])
                            cylinder(d=STANDOFF_ID, h=STANDOFF_H + 0.02, $fn=16);
                    }
            }

            // USB-C breakout board mount posts (M2, near back wall)
            for (pos = usbc_mount_positions) {
                translate([pos[0], pos[1], BOT_T])
                    difference() {
                        cylinder(d=USBC_STANDOFF_OD, h=USBC_STANDOFF_H, $fn=20);
                        translate([0, 0, -0.01])
                            cylinder(d=USBC_STANDOFF_ID, h=USBC_STANDOFF_H + 0.02, $fn=16);
                    }
            }

            // Structural support pillars with male pin tips
            // 8mm body from floor to tray wall top, then 5mm pin extends above
            for (pos = pillar_positions) {
                // Body (8mm diameter, floor to wall top)
                translate([pos[0], pos[1], BOT_T])
                    cylinder(d=PILLAR_D, h=BOT_WALL_H - BOT_T, $fn=24);
                // Male pin tip (5mm diameter, extends above wall for friction fit)
                translate([pos[0], pos[1], BOT_WALL_H])
                    cylinder(d=PILLAR_PIN_D, h=PILLAR_PIN_H, $fn=24);
            }

            // Screw bosses (short posts rising from floor, for M3 self-tap)
            for (pos = screw_positions) {
                translate([pos[0], pos[1], BOT_T])
                    difference() {
                        cylinder(d=BOSS_OD, h=BOT_WALL_H - BOT_T, $fn=24);
                        // No through-hole here; top shell bosses mate from above
                    }
            }
        }

        // Screw holes (countersunk from bottom, through floor + boss)
        for (pos = screw_positions) {
            // Through hole (full height of bottom shell)
            translate([pos[0], pos[1], -0.01])
                cylinder(d=SCREW_D, h=BOT_WALL_H + 0.02, $fn=20);
            // Countersink from bottom
            translate([pos[0], pos[1], -0.01])
                cylinder(d1=SCREW_HEAD_D, d2=SCREW_D,
                         h=SCREW_HEAD_DEPTH + 0.01, $fn=20);
        }

        // Rubber foot recesses
        for (pos = foot_positions) {
            translate([pos[0] - FOOT_SIZE/2, pos[1] - FOOT_SIZE/2, -0.01])
                cube([FOOT_SIZE, FOOT_SIZE, FOOT_DEPTH + 0.01]);
        }

        // USB-C cutout in back wall (only if it intersects bottom shell height)
        if (USBC_Y + USBC_H/2 > 0 && USBC_Y - USBC_H/2 < BOT_WALL_H)
            translate([-USBC_W/2, ENC_D/2 - WALL - 0.01,
                       USBC_Y - USBC_H/2])
                cube([USBC_W, WALL + 0.02, USBC_H]);
    }
}

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
}

// ─── RENDER SETTINGS ─────────────────────────────────────────────
$fn = 48;
