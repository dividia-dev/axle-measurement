# Joystick Axis Restrictor — Y-Axis Only Gate Plate

## Purpose

The CV axle controller uses two joysticks for camera PTZ control. The X-axis
(horizontal/pan) is handled by a twist barrel on the joystick, but this proved
unreliable. The firmware (v2.1) disables the twist axis entirely, and the
controller only needs Y-axis movement from the joystick gimbal.

This restrictor replaces the stock circular gate plate with a vertical slot,
physically preventing X-axis travel. The joystick can only move up/down.

## Stock Gate Plate — Measured Dimensions

| Feature | Dimension | Notes |
|---|---|---|
| Exterior (square) | 40 x 40 mm | Outer edges including lips |
| Interior (between lips) | 38 x 38 mm | Flat plate area |
| Plate thickness | 2.5 mm (with lip), ~2 mm (interior) | Plastic, not metal |
| Edge lips | 1 mm wide, 2.5 mm tall | Slide into housing slots, all 4 edges |
| Corner posts | ~8.5 mm tall | Screw standoffs |
| Screw pattern | 33 mm on-center (square) | 4x screws |
| Screw recess | 4 mm diameter | Countersink on bottom |
| Screw passthrough | 3 mm diameter | Through post |
| Center opening | 19 mm diameter (circle) | Full 360-degree travel |
| Diagonal post-to-post | ~14 mm | Corner post diagonal |

## Restrictor Design

- **Slot width:** 13 mm (matches X-axis gimbal carriage width)
- **Slot length:** 25 mm (full Y-axis travel, clears screw holes with margin)
- **Slot shape:** Stadium (rounded ends) for strength
- **Plate thickness:** 3 mm (thicker than stock 2.5 mm for plastic rigidity)
- **Material:** PLA, 0.4 mm nozzle, standard settings

## Internal Clearances

### Return Springs
- Width: 17 mm
- Extend 4 mm inward from interior body wall
- When joystick at full Y deflection, one spring stretches to ~16 mm
- Springs are BELOW the plate — no interference with plate geometry
- Future fin designs must avoid the spring zone

### Gimbal Carriage
- The Y-axis carriage moves within a 13 mm wide channel
- The slot width matches this exactly, blocking any X-axis drift
- The shaft passes through the slot center

## Print Notes

- Print with plate bottom on bed, posts pointing up
- Edge lips (2.5 mm tall, 1 mm wide) should bridge without supports
- If lips sag, add support for the lip overhangs only
- Test fit before committing — may need TOL adjustment for housing slot fit

## Future Improvements

1. **Vertical fins** — Add walls extending downward on either side of the slot
   for additional X-axis blocking at the shaft level. Requires exact spring
   position mapping to avoid interference. Only 1 mm depth available before
   hitting springs and other components.

2. **Enclosure bottom integration** — Could incorporate the restrictor slot
   directly into the controller enclosure bottom shell, eliminating this as
   a separate part.

## File

- OpenSCAD source: `enclosure/joystick-restrictor.scad`
- Set `PART = "print"` and export STL for printing
