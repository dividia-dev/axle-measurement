# Controller Enclosure — 3D Print Files

Two-piece enclosure for the CV Axle Controller. Top shell (main body with all
panel features) and bottom shell (shallow tray with nesting lip).

## Dimensions

- **External:** 190mm W x 100mm D x 42mm H
- **Print target:** Bambu Lab A1, 0.4mm nozzle, PLA
- **Source file:** `enclosure.scad` (parametric — all dimensions are variables)

## Generating STL Files

### Requirements

- [OpenSCAD](https://openscad.org/) — free, open source
- macOS install: `brew install --cask openscad`

### Command Line (no GUI needed)

```bash
# Top shell (prints upside down for smooth top surface)
openscad -o cv-axle-top-shell.stl -D 'PART="top"' enclosure.scad

# Bottom shell (prints right-side up)
openscad -o cv-axle-bottom-shell.stl -D 'PART="bottom"' enclosure.scad
```

Render takes ~20 seconds per part.

### Using the GUI

1. Open `enclosure.scad` in OpenSCAD
2. Change `PART = "assembly"` to `PART = "top"` or `PART = "bottom"`
3. Press **F6** (Render)
4. File → Export → STL

Use `PART = "assembly"` to preview both halves together with ghost joysticks
and perfboard for visual reference.

## Print Settings (Recommended)

| Setting | Value |
|---------|-------|
| Layer height | 0.2mm |
| Walls/perimeters | 3 |
| Infill | 15-20% gyroid or cubic |
| Supports | Not required |
| Top shell orientation | Upside down (smooth top surface on bed) |
| Bottom shell orientation | Right-side up |

## Design Notes

- **Sealed enclosure** — no ventilation holes. Heat generation is <1W,
  designed for dusty cement facility environment.
- **Top shell** contains: joystick barrel holes + M2.5 screw pattern, toggle
  switch hole, RGB status LED (with underside collar), 2x fine mode LED holes,
  USB-C cutout, recal pinhole, structural pillar sockets, screw bosses,
  reinforcement ribs, "AXLE CONTROLLER" front branding, "DIVIDIA" back branding,
  filleted top edges (1.5mm radius).
- **Bottom shell** contains: perfboard standoffs (M2), USB-C breakout mount posts,
  structural support pillars (male pins for friction-fit alignment with top shell),
  M3 countersunk corner screws, rubber foot recesses.
- All fit surfaces have 0.3mm tolerance built in.
- Full component spec: `docs/enclosure-component-spec.md`
- Panel layout planners: `docs/top-panel-layout.html`, `docs/back-panel-layout.html`,
  `docs/bottom-panel-layout.html`
