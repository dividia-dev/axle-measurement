// Recal Button Cradle — Test Print
// Isolated section of back wall with snap-in cradle.
// Print this to iterate on fit without reprinting the whole enclosure.
//
// Includes: wall section, pinhole, target ring recess, snap-in cradle, ghost button.
// Print flat (wall face down on bed).

// ─── WALL SECTION ───────────────────────────────────────────────
WALL         = 3;     // wall thickness (matches enclosure)
SECTION_W    = 30;    // width of test piece (X)
SECTION_H    = 30;    // height of test piece (Z)

// ─── RECAL PINHOLE ──────────────────────────────────────────────
RECAL_D          = 2;     // pinhole diameter
RECAL_RECESS_D   = 8;     // target ring outer diameter
RECAL_RECESS_DEPTH = 2.0; // target ring depth

// ─── BUTTON DIMENSIONS ─────────────────────────────────────────
// Measure YOUR tact switch and update these if different!
BTN_W        = 6;     // body width (X)
BTN_H        = 6;     // body height (Z)
BTN_DEPTH    = 3;     // body depth (Y, measured from face to back)
BTN_TOL      = 0.3;   // clearance per side
BTN_ACT_D    = 4;     // actuator diameter
BTN_ACT_EXT  = 2;     // actuator extension beyond body face

// ─── CRADLE DESIGN ──────────────────────────────────────────────
// Total pocket depth needs to fit: actuator extension + body depth
// without the floor pressing the actuator.
CRADLE_DEPTH = BTN_DEPTH + BTN_ACT_EXT + 0.5;  // body + actuator + clearance
SNAP_T       = 1.2;   // snap tab thickness
SNAP_LIP     = 0.8;   // tab lip overhang
FLOOR_T      = 1.0;   // floor thickness behind button (between button back and air)

// Pocket interior
pocket_w = BTN_W + 2*BTN_TOL;
pocket_h = BTN_H + 2*BTN_TOL;
// Cradle outer (pocket + snap tabs on top/bottom)
cradle_w = pocket_w + 2*SNAP_T;
cradle_h = pocket_h + 2*SNAP_T;
// Total cradle protrusion from wall
cradle_total_d = CRADLE_DEPTH + FLOOR_T;

// Center of button/pinhole in the test piece
CX = SECTION_W / 2;
CZ = SECTION_H / 2;

// ─── MODULES ────────────────────────────────────────────────────

module test_piece() {
    difference() {
        union() {
            // ── Wall section ──
            // Oriented: X=width, Y=thickness(wall), Z=height
            cube([SECTION_W, WALL, SECTION_H]);

            // ── Cradle block on interior face ──
            // Protrudes inward (negative Y in enclosure, positive Y here)
            translate([CX - cradle_w/2, WALL - 0.01, CZ - cradle_h/2])
                cube([cradle_w, cradle_total_d + 0.01, cradle_h]);
        }

        // ── Pinhole through wall (exterior access) ──
        translate([CX, -0.01, CZ])
            rotate([-90, 0, 0])
                cylinder(d=RECAL_D, h=WALL + 0.02, $fn=16);

        // ── Target ring recess on exterior face ──
        translate([CX, -0.01, CZ])
            rotate([-90, 0, 0])
                cylinder(d=RECAL_RECESS_D, h=RECAL_RECESS_DEPTH + 0.01, $fn=32);

        // ── Actuator channel through wall into cradle ──
        // From wall interior face through to cradle pocket
        translate([CX, WALL - 0.01, CZ])
            rotate([-90, 0, 0])
                cylinder(d=BTN_ACT_D + 0.4, h=BTN_ACT_EXT + 1, $fn=16);

        // ── Main pocket (button body cavity) ──
        // Starts after actuator clearance zone, goes to back of cradle
        // Leave FLOOR_T at the very back so button doesn't fall through
        translate([CX - pocket_w/2,
                   WALL + BTN_ACT_EXT,
                   CZ - pocket_h/2])
            cube([pocket_w, BTN_DEPTH + 1, pocket_h]);

        // ── Plunger clearance hole in pocket floor ──
        // This is the KEY FIX: a hole so the actuator isn't pressed
        // by the pocket floor when the button sits in the cradle
        translate([CX, WALL + BTN_ACT_EXT - 0.01, CZ])
            rotate([-90, 0, 0])
                cylinder(d=BTN_ACT_D + 1, h=0.5 + 0.02, $fn=16);

        // ── Top snap tab relief (allows tab to flex outward) ──
        translate([CX - pocket_w/2,
                   WALL + BTN_ACT_EXT,
                   CZ + pocket_h/2])
            cube([pocket_w, CRADLE_DEPTH - SNAP_LIP, SNAP_T + 0.01]);

        // ── Bottom snap tab relief ──
        translate([CX - pocket_w/2,
                   WALL + BTN_ACT_EXT,
                   CZ - pocket_h/2 - SNAP_T])
            cube([pocket_w, CRADLE_DEPTH - SNAP_LIP, SNAP_T + 0.01]);
    }
}

// ─── RENDER ─────────────────────────────────────────────────────

// Print orientation: wall face (exterior) down on bed
// Rotate so the flat wall face is on the build plate
rotate([90, 0, 0])
    test_piece();

// Ghost button for reference (won't print)
%rotate([90, 0, 0])
    translate([CX - BTN_W/2,
               WALL + BTN_ACT_EXT,
               CZ - BTN_H/2])
        cube([BTN_W, BTN_DEPTH, BTN_H]);
