# Single-Camera Distance Measurement for Truck Axle Spacing

## Research Report — 2026-05-08

Comprehensive technical research for measuring real-world distances between truck axles
using a single fixed camera overlooking a truck scale (weighbridge).

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Measurement Techniques Overview](#2-measurement-techniques-overview)
3. [Recommended Approach: Two-Point Linear Calibration](#3-recommended-approach-two-point-linear-calibration)
4. [The Math](#4-the-math)
5. [When Linear Calibration Breaks Down](#5-when-linear-calibration-breaks-down)
6. [Depth / Parallax Error Analysis](#6-depth--parallax-error-analysis)
7. [Calibration Methods](#7-calibration-methods)
8. [Camera Placement and Hardware](#8-camera-placement-and-hardware)
9. [Accuracy Expectations](#9-accuracy-expectations)
10. [Lens Distortion](#10-lens-distortion)
11. [Implementation in C with FFmpeg](#11-implementation-in-c-with-ffmpeg)
12. [Full Implementation Algorithm](#12-full-implementation-algorithm)
13. [When to Upgrade to Homography](#13-when-to-upgrade-to-homography)
14. [Recommendations Summary](#14-recommendations-summary)
15. [Sources](#15-sources)

---

## 1. Executive Summary

For measuring truck axle spacing with a fixed overhead camera, a **simple two-point
linear calibration** is the recommended starting approach. Place two reference markers
(stakes, painted marks, bolts) at a known distance apart on the scale surface, visible
in the camera frame. The system calculates a pixels-per-unit ratio from these reference
points and uses it to convert any pixel measurement to real-world distance.

**Key finding:** If the camera is mounted roughly perpendicular to the measurement plane
(looking straight down or nearly so), and the reference markers are at approximately the
same height as the truck axles, a simple linear pixel-to-distance ratio achieves
accuracy of +/- 1 inch or better with a 1080p camera covering a 60-foot span. No
complex computer vision libraries are required.

The main risk factor is **parallax error from height differences** — if the reference
stakes are on the ground but the axles are 1-2 feet higher, and the camera is at an
angle rather than directly overhead, measurement error increases significantly.

---

## 2. Measurement Techniques Overview

There are four main approaches to single-camera distance measurement, ranked from
simplest to most complex:

### A. Simple Pixels-Per-Unit Ratio (LINEAR)
- Use two known reference points to establish: `real_distance / pixel_distance = scale`
- Multiply any pixel measurement by this scale factor
- Works when: camera is perpendicular to measurement plane, no lens distortion
- Accuracy: Excellent for the center of the image, degrades toward edges

### B. Multi-Point Linear Calibration
- Same as above but use multiple reference points across the field of view
- Allows detecting non-linearity and creating a piecewise-linear correction
- Works when: slight camera angle or mild lens distortion

### C. Homography / Perspective Transform
- Use 4+ reference points to compute a 3x3 transformation matrix
- Maps pixel coordinates to real-world coordinates on a plane
- Corrects for camera angle (perspective distortion) mathematically
- Works when: camera is at a significant angle to the measurement plane

### D. Full Camera Calibration (Intrinsic + Extrinsic)
- Compute focal length, principal point, lens distortion coefficients, camera pose
- Most accurate but most complex — requires checkerboard calibration patterns
- Works when: high precision needed, significant lens distortion, complex geometry

**For this application:** Start with approach A. If accuracy tests reveal problems,
upgrade to B or C. Approach D is overkill unless sub-millimeter precision is needed.

---

## 3. Recommended Approach: Two-Point Linear Calibration

### Concept

Two fixed reference markers (e.g., steel stakes, painted marks, or bolt heads) are
placed on or near the truck scale, aligned along the direction of travel (the same
axis along which axle distance is measured). These markers are at a **known, precisely
measured distance** apart.

During a one-time calibration step:
1. The operator identifies the two reference markers in the video feed
2. The system records their pixel X-coordinates
3. The system stores the known real-world distance
4. A pixels-per-unit scale factor is computed

During measurement:
1. The operator positions two vertical lines over the leftmost and rightmost axles
2. The system reads the pixel X-coordinates of the two lines
3. The pixel distance is converted to real-world distance using the scale factor

### Why This Works

For a fixed camera looking at a flat surface:
- The relationship between pixel distance and real-world distance is **linear** along
  any line parallel to the image plane, provided:
  - The camera optical axis is perpendicular to the measurement surface
  - There is negligible lens distortion in the measurement region
  - All measured points are at the same depth (distance from camera)

Since truck axles are spaced along a single axis (the truck's direction of travel),
and the scale surface is flat, this is essentially a 1D measurement problem — making
linear calibration ideal.

---

## 4. The Math

### Basic Linear Calibration

```
Given:
  ref_pixel_1  = pixel X-coordinate of reference marker 1
  ref_pixel_2  = pixel X-coordinate of reference marker 2
  ref_distance = known real-world distance between markers (e.g., in inches)

Compute scale factor:
  pixels_per_unit = |ref_pixel_2 - ref_pixel_1| / ref_distance

Measure axle distance:
  axle_pixel_1 = pixel X-coordinate of line 1 (left axle)
  axle_pixel_2 = pixel X-coordinate of line 2 (right axle)

  axle_distance = |axle_pixel_2 - axle_pixel_1| / pixels_per_unit
```

### In C code:

```c
// Calibration (run once, store results)
typedef struct {
    int    ref_pixel_1;      // pixel X of reference point 1
    int    ref_pixel_2;      // pixel X of reference point 2
    double ref_distance;     // known distance in inches (or desired unit)
    double pixels_per_unit;  // computed scale factor
} CalibrationData;

void calibrate(CalibrationData *cal) {
    cal->pixels_per_unit = (double)abs(cal->ref_pixel_2 - cal->ref_pixel_1)
                         / cal->ref_distance;
}

// Measurement (run each time)
double measure_distance(const CalibrationData *cal, int line1_x, int line2_x) {
    double pixel_dist = (double)abs(line2_x - line1_x);
    return pixel_dist / cal->pixels_per_unit;
}
```

### Example Calculation

```
Camera: 1920x1080, covering ~60 feet (720 inches) of scale length
Reference stakes: 20 feet (240 inches) apart
  Stake 1 at pixel X = 480
  Stake 2 at pixel X = 1440

pixels_per_unit = |1440 - 480| / 240 = 960 / 240 = 4.0 pixels/inch

Operator places lines at:
  Left axle:  pixel X = 600
  Right axle: pixel X = 1320

axle_distance = |1320 - 600| / 4.0 = 720 / 4.0 = 180.0 inches = 15.0 feet
```

### Resolution Limit

With 4 pixels per inch, the minimum measurable increment is:
```
1 pixel = 1/4 inch = 0.25 inches
```

This is the **theoretical resolution limit**. Practical accuracy depends on how
precisely the operator can align the lines with the axle centers.

---

## 5. When Linear Calibration Breaks Down

Linear calibration assumes uniform scale across the measurement region. This breaks
down in three scenarios:

### A. Camera at an Angle (Perspective Distortion)

If the camera is tilted rather than looking straight down, objects closer to the camera
appear larger (more pixels per inch) and objects farther away appear smaller (fewer
pixels per inch). The relationship between pixel distance and real-world distance
becomes **non-linear**.

**Severity:** For a camera mounted at 45 degrees looking across a 60-foot scale, the
pixels-per-inch ratio could vary by 2:1 or more from near edge to far edge. This makes
simple linear calibration unusable.

**For a camera nearly overhead (within ~15 degrees of perpendicular):** The non-linearity
is mild — perhaps 5-10% variation across the field of view. Linear calibration still
works if the reference markers span the region where measurements will be taken.

### B. Lens Distortion

Wide-angle lenses cause barrel distortion — straight lines appear curved, and the
scale factor varies radially from the image center. This is particularly noticeable
with:
- Wide-angle lenses (focal length < 25mm on full-frame equivalent)
- Cheap lenses / IP cameras with small sensors
- Measurements near the image edges

**Severity:** Barrel distortion can cause 2-5% measurement error at the image edges
with typical IP cameras. It is negligible near the image center.

### C. Depth Differences (Parallax)

If the reference markers and the measured objects are at different heights (distances
from the camera), the scale factor differs between them. See Section 6 for detailed
analysis.

---

## 6. Depth / Parallax Error Analysis

### The Problem

Reference stakes are typically at **ground level** (on the scale surface). Truck axles
are at **axle height** — approximately 12-24 inches above the scale surface, depending
on the truck.

If the camera is not directly overhead, objects at different heights project to
different pixel positions. A point that is higher (closer to the camera) appears
shifted outward from the image center.

### The Geometry

```
Camera at position C, height H above scale surface
Object on ground at horizontal position X: projects to pixel p_ground
Object at height h at same horizontal position X: projects to pixel p_elevated

The apparent shift depends on:
  - h: height difference between reference plane and measurement plane
  - H: camera height above ground
  - theta: angle between camera optical axis and vertical
  - d: horizontal distance from camera to object
```

### Parallax Error Formula

For a camera looking down at angle theta from vertical, at height H:

```
Apparent horizontal shift (in real-world units) due to height h:

  error = h * tan(alpha)

where alpha is the angle from vertical to the line of sight at that point.

For a point at horizontal distance d from the point directly below the camera:
  alpha = arctan(d / H)

So:
  error = h * d / H
```

**The parallax error in the measured distance between two axles:**

```
If two axles are at positions x1 and x2 (measured from directly below camera),
both at height h above the reference plane:

  error_1 = h * x1 / H
  error_2 = h * x2 / H

  distance_error = error_2 - error_1 = h * (x2 - x1) / H

  Fractional error = h / H
```

**This is the critical result: the fractional measurement error equals the ratio
of the height difference to the camera height.**

### Practical Error Calculations

```
Scenario 1: Camera at H = 30 feet, axle height h = 1.5 feet
  Fractional error = 1.5 / 30 = 5%
  On a 15-foot axle span: error = 0.75 feet = 9 inches  [UNACCEPTABLE]

Scenario 2: Camera at H = 30 feet, axle height h = 1.5 feet, camera DIRECTLY OVERHEAD
  If camera is directly above the scale center:
  error_1 and error_2 partially cancel (symmetrically placed axles)
  But if truck is off-center, error remains proportional to h/H

Scenario 3: Camera at H = 50 feet, directly overhead, axle height h = 1.5 feet
  Fractional error = 1.5 / 50 = 3%
  On a 15-foot span: error = 5.4 inches  [STILL SIGNIFICANT]
```

### IMPORTANT: When Parallax Error Cancels

Parallax error **does NOT affect** the measurement when:
1. The camera is **directly overhead** (perpendicular to the ground), AND
2. Both axles are at the **same height** above the reference plane

In this case, both axles shift by the same amount in the same direction in the image,
so the distance between them is preserved. The shift is purely radial from the image
center, and for points at the same height, the scaling is uniform.

More precisely: when the camera looks straight down, the image is an **orthographic-like
projection** of the horizontal plane at any given height. The scale factor changes with
height (objects closer to camera appear larger), but it changes **uniformly**. So the
ratio between any two horizontal distances at the same height is preserved.

**The fractional scale error for the distance measurement is:**
```
scale_error = h / (H - h)

For H = 30 ft, h = 1.5 ft:
  scale_error = 1.5 / 28.5 = 5.3%
  This means the measured distance reads 5.3% LONGER than actual

For H = 50 ft, h = 1.5 ft:
  scale_error = 1.5 / 48.5 = 3.1%
```

This is a **systematic error** that can be corrected if you know the axle height:
```
corrected_distance = measured_distance * (H - h) / H
```

Or better: calibrate with reference markers at the same height as the axle plane.

### Strategies to Minimize Parallax Error

1. **Mount camera as high as possible** — doubles the height, halves the error
2. **Mount camera directly overhead** (perpendicular to scale) — eliminates
   asymmetric parallax
3. **Place reference markers at axle height** — if stakes/markers are at ~18 inches
   above the scale (average axle height), the calibration automatically accounts for
   the measurement plane height
4. **Apply height correction** — if axle height is approximately known, apply the
   formula: `corrected = measured * (H - h) / H`
5. **Use camera angle close to perpendicular** — even 10-15 degrees off vertical
   introduces significant parallax for off-center trucks

---

## 7. Calibration Methods

### Method 1: Two-Stake Calibration (Recommended for v1)

**Setup:**
- Install two fixed reference markers (steel stakes, painted marks, bolt heads, or
  any durable, visible markers) along the direction of truck travel
- Measure the distance between them precisely (tape measure, surveyor's tool)
- Markers should be at the edges of the useful measurement zone
- Ideally, markers should be at approximately axle height (mount on short posts)

**Calibration Procedure:**
1. Display live video feed
2. Operator clicks/positions a cursor on reference marker 1 -> record pixel X
3. Operator clicks/positions a cursor on reference marker 2 -> record pixel X
4. Enter known distance
5. System computes and stores pixels_per_unit
6. Calibration is stored persistently (file, EEPROM, etc.)

**When to recalibrate:**
- If the camera is moved, adjusted, or replaced
- If zoom level changes
- Periodic verification (e.g., monthly) against a known reference

### Method 2: Multi-Point Calibration (If non-linearity is detected)

Place 3-5 reference markers at known intervals. Compute pixels_per_unit for each
adjacent pair. If they differ by more than 2-3%, use piecewise-linear interpolation:

```c
// For N reference points, sorted by pixel position
typedef struct {
    int    pixel_x;
    double world_x;   // real-world position in inches
} RefPoint;

double pixel_to_world(RefPoint *refs, int n_refs, int pixel_x) {
    // Find which segment this pixel falls in
    for (int i = 0; i < n_refs - 1; i++) {
        if (pixel_x >= refs[i].pixel_x && pixel_x <= refs[i+1].pixel_x) {
            double frac = (double)(pixel_x - refs[i].pixel_x)
                        / (double)(refs[i+1].pixel_x - refs[i].pixel_x);
            return refs[i].world_x + frac * (refs[i+1].world_x - refs[i].world_x);
        }
    }
    // Extrapolate if outside range (use nearest segment's ratio)
    if (pixel_x < refs[0].pixel_x) {
        double ppu = (double)(refs[1].pixel_x - refs[0].pixel_x)
                   / (refs[1].world_x - refs[0].world_x);
        return refs[0].world_x - (double)(refs[0].pixel_x - pixel_x) / ppu;
    } else {
        int last = n_refs - 1;
        double ppu = (double)(refs[last].pixel_x - refs[last-1].pixel_x)
                   / (refs[last].world_x - refs[last-1].world_x);
        return refs[last].world_x + (double)(pixel_x - refs[last].pixel_x) / ppu;
    }
}

double measure_distance_multipoint(RefPoint *refs, int n_refs,
                                    int line1_x, int line2_x) {
    double world1 = pixel_to_world(refs, n_refs, line1_x);
    double world2 = pixel_to_world(refs, n_refs, line2_x);
    return fabs(world2 - world1);
}
```

### Method 3: Four-Point Homography (If camera is significantly angled)

If the camera views the scale at a steep angle, use four reference points to compute
a perspective transformation:

```c
// Homography maps pixel coordinates to world coordinates
// H is a 3x3 matrix computed from 4 point correspondences
//
// [wx']   [h11 h12 h13] [px]
// [wy'] = [h21 h22 h23] [py]
// [w  ]   [h31 h32 h33] [ 1]
//
// world_x = wx' / w
// world_y = wy' / w

typedef struct {
    double h[3][3];  // homography matrix
} Homography;

void apply_homography(const Homography *H, int px, int py,
                      double *world_x, double *world_y) {
    double w  = H->h[2][0] * px + H->h[2][1] * py + H->h[2][2];
    *world_x = (H->h[0][0] * px + H->h[0][1] * py + H->h[0][2]) / w;
    *world_y = (H->h[1][0] * px + H->h[1][1] * py + H->h[1][2]) / w;
}
```

Computing H from 4 point correspondences requires solving a system of 8 linear
equations. This can be done with a simple Gaussian elimination — no external library
needed. See Section 13 for when this is necessary.

---

## 8. Camera Placement and Hardware

### Optimal Camera Position

**Best: Directly overhead, looking straight down**
- Eliminates perspective distortion entirely
- Linear calibration works perfectly
- Parallax from height differences is uniform and correctable
- Drawback: requires mounting structure directly over the scale (gantry, pole, building overhang)

**Good: High and nearly overhead (within 15 degrees of vertical)**
- Mild perspective distortion — linear calibration still usable
- Some parallax error — correctable with height compensation
- More practical mounting options (building wall/roof near the scale)

**Acceptable: Elevated side view (30-45 degrees from vertical)**
- Significant perspective distortion — needs homography or multi-point calibration
- Parallax error becomes position-dependent
- Requires 4-point calibration minimum
- Mounting is easiest (wall-mounted, pole-mounted from the side)

**Poor: Low angle or near-horizontal**
- Extreme perspective distortion
- Axles may occlude each other
- Parallax error is severe
- Not recommended for measurement

### Camera Height Recommendations

```
Higher is better for measurement accuracy:
  - 20 feet: Adequate for overhead mount, 3% parallax error at 18" axle height
  - 30 feet: Good, 5% parallax with side-mounted, ~1.7% overhead
  - 50 feet: Excellent, minimal parallax (<1% overhead)

Higher cameras require:
  - Higher resolution to maintain pixels-per-inch
  - Better weatherproofing (if outdoor)
  - More robust mounting
```

### Camera Resolution Requirements

The resolution requirement depends on the field of view and desired measurement
precision:

```
Field of view: L feet (length of scale visible)
Desired precision: P inches
Required horizontal resolution: R pixels

R = L * 12 / P   (minimum)
R = L * 12 / P * 3  (recommended, using 3-pixel rule for reliable detection)

Examples:
  60-foot scale, 1-inch precision:
    Minimum: 60 * 12 / 1 = 720 pixels  -> 720p adequate
    Recommended: 720 * 3 = 2160 pixels  -> 4K recommended

  60-foot scale, 0.5-inch precision:
    Minimum: 1440 pixels -> 1080p adequate
    Recommended: 4320 pixels -> need 4K or higher, or narrower FOV

  40-foot scale, 1-inch precision:
    Minimum: 480 pixels -> any modern camera
    Recommended: 1440 pixels -> 1080p is fine
```

### Recommended Camera Specifications

For a truck scale application:
- **Resolution:** 1080p minimum, 4K preferred
- **Lens:** Fixed focal length, moderate angle (not wide-angle)
  - Avoid fisheye or wide-angle lenses (< 4mm focal length) — too much distortion
  - 8-12mm focal length on a 1/2.7" sensor is a good starting point
  - Adjust based on mounting height and required field of view
- **Type:** Industrial IP camera or USB camera with manual focus
- **Housing:** Weatherproof (IP66+) if outdoors, or indoor-rated
- **Frame rate:** 15+ fps for smooth live view (measurement is done on single frames)
- **Mount:** Vibration-resistant, fixed mount (no PTZ — zoom changes invalidate calibration)
- **Compression:** H.264/H.265 for streaming; measurement is on the video frame, not
  on compressed artifacts, so standard compression is fine

### Environmental Considerations

- **Vibration:** Camera mount must be rigid. Wind or equipment vibration causes frame-
  to-frame jitter. For measurement, the operator freezes a frame or averages position.
- **Lighting:** Consistent lighting helps operator identify axles. Consider IR illumination
  for 24-hour operation. Avoid backlighting (sun behind the truck).
- **Weather:** Rain, fog, snow reduce visibility. Heated enclosure prevents lens fogging.
- **Dirt/dust:** Cement/aggregate facility = dusty. Camera needs periodic lens cleaning.
  Consider compressed air nozzle or wiper.

---

## 9. Accuracy Expectations

### Theoretical Accuracy

With a well-calibrated system:

```
Resolution-limited accuracy:
  1080p camera, 60-foot FOV: 1 pixel = 0.75 inches -> ~1 inch accuracy
  4K camera, 60-foot FOV:    1 pixel = 0.375 inches -> ~0.5 inch accuracy
  1080p camera, 40-foot FOV: 1 pixel = 0.5 inches -> ~0.5 inch accuracy

Operator placement accuracy:
  Experienced operator, clear image: +/- 2-3 pixels = +/- 1.5-2.25 inches (1080p/60ft)
  
Combined accuracy (1080p, 60-foot FOV):
  Best case: +/- 1 inch
  Typical: +/- 2 inches
  Worst case (poor visibility, parallax): +/- 4-6 inches
```

### Factors Affecting Practical Accuracy

| Factor | Impact | Mitigation |
|--------|--------|------------|
| Camera resolution | Sets hard limit on precision | Use highest practical resolution |
| Operator skill | +/- 2-5 pixels typically | Training, zoomed view option |
| Lens distortion | 2-5% at edges | Use quality lens, calibrate in center |
| Parallax (height diff) | 3-5% systematic | Height correction, overhead mount |
| Camera angle | Non-linear if >15 degrees | Mount overhead or use homography |
| Video compression | Blurs edges by 1-2 pixels | Use high bitrate, I-frame for measurement |
| Vibration/wind | Frame-to-frame jitter | Rigid mount, frame freeze for measurement |
| Lighting | Poor contrast = uncertain edges | Add illumination, IR for night |
| Truck not centered | Parallax varies by position | Overhead camera eliminates this |

### Accuracy vs. Truck Scale Industry Standards

Typical truck scale accuracy for weight is +/- 0.1% to 0.5% of capacity. For axle
spacing measurement, the relevant standard is primarily for vehicle classification
(how many axles, what type of truck). Industry tolerance for axle spacing classification
is typically +/- 6 inches, which is well within reach of even a basic camera system.

If the goal is more precise (e.g., detecting overloaded axle groups based on spacing
regulations), +/- 1-2 inches is achievable with a properly configured 1080p system
and trained operators.

---

## 10. Lens Distortion

### Types

**Barrel distortion:** Straight lines bow outward from center. Common with wide-angle
lenses. The scale factor (pixels per inch) is larger at the center than at the edges.

**Pincushion distortion:** Straight lines bow inward. Common with telephoto lenses.
Less common in this application.

### Distortion Model

The standard Brown-Conrady model:
```
r = sqrt((x - cx)^2 + (y - cy)^2)   // distance from image center

x_distorted = x * (1 + k1*r^2 + k2*r^4 + k3*r^6)
y_distorted = y * (1 + k1*r^2 + k2*r^4 + k3*r^6)

Where:
  (cx, cy) = image center (principal point)
  k1, k2, k3 = radial distortion coefficients
  k1 < 0 = barrel distortion
  k1 > 0 = pincushion distortion
```

### When to Worry About It

For this application, lens distortion matters when:
- Using a wide-angle lens (focal length < 6mm on a small sensor)
- Measuring near the edges of the frame
- Accuracy requirement is better than 2%

**Quick test:** Display a straight edge (ruler, string) across the full frame. If it
appears curved in the image, distortion correction is needed.

### Practical Mitigation

1. **Use a lens with low distortion** — longer focal length, quality glass
2. **Make measurements near the image center** — distortion is minimal within the
   central 50% of the frame
3. **Calibrate with markers across the full field** — multi-point calibration
   implicitly absorbs distortion
4. **Apply software correction** — undistort the image using the Brown-Conrady model
   before measurement (requires computing k1, k2 from a calibration pattern)

For this application, option 2 (keeping measurements in the center) combined with
option 3 (multi-point calibration) is likely sufficient. Full distortion correction
is only needed if using a wide-angle lens.

---

## 11. Implementation in C with FFmpeg

### Architecture Overview

The existing system renders video via FFmpeg to a framebuffer. The measurement overlay
(two vertical lines) is drawn on top of the rendered video. The implementation adds:

1. **Calibration data structure** — stored in a config file
2. **Pixel-to-distance conversion function** — pure math, no dependencies
3. **Display of calculated distance** — rendered as text overlay on the framebuffer
4. **Calibration mode** — one-time setup to record reference point positions

### Core Implementation

```c
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// CALIBRATION DATA
// ============================================================

typedef struct {
    // Reference points (pixel X coordinates)
    int ref1_px;
    int ref2_px;

    // Known real-world distance between reference points (inches)
    double ref_distance_inches;

    // Computed scale factor
    double pixels_per_inch;

    // Optional: height correction
    double camera_height_inches;   // H: camera height above scale surface
    double axle_height_inches;     // h: typical axle height above scale surface
    int    apply_height_correction; // boolean

    // Valid flag
    int is_calibrated;
} AxleCalibration;

// ============================================================
// CALIBRATION FUNCTIONS
// ============================================================

int calibration_compute(AxleCalibration *cal) {
    if (cal->ref1_px == cal->ref2_px) {
        return -1;  // Error: reference points at same pixel
    }
    if (cal->ref_distance_inches <= 0.0) {
        return -2;  // Error: invalid distance
    }

    cal->pixels_per_inch = (double)abs(cal->ref2_px - cal->ref1_px)
                         / cal->ref_distance_inches;
    cal->is_calibrated = 1;
    return 0;
}

int calibration_save(const AxleCalibration *cal, const char *filepath) {
    FILE *f = fopen(filepath, "w");
    if (!f) return -1;

    fprintf(f, "ref1_px=%d\n", cal->ref1_px);
    fprintf(f, "ref2_px=%d\n", cal->ref2_px);
    fprintf(f, "ref_distance_inches=%.4f\n", cal->ref_distance_inches);
    fprintf(f, "pixels_per_inch=%.6f\n", cal->pixels_per_inch);
    fprintf(f, "camera_height_inches=%.2f\n", cal->camera_height_inches);
    fprintf(f, "axle_height_inches=%.2f\n", cal->axle_height_inches);
    fprintf(f, "apply_height_correction=%d\n", cal->apply_height_correction);
    fprintf(f, "is_calibrated=%d\n", cal->is_calibrated);

    fclose(f);
    return 0;
}

int calibration_load(AxleCalibration *cal, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return -1;

    fscanf(f, "ref1_px=%d\n", &cal->ref1_px);
    fscanf(f, "ref2_px=%d\n", &cal->ref2_px);
    fscanf(f, "ref_distance_inches=%.4lf\n", &cal->ref_distance_inches);
    fscanf(f, "pixels_per_inch=%lf\n", &cal->pixels_per_inch);
    fscanf(f, "camera_height_inches=%lf\n", &cal->camera_height_inches);
    fscanf(f, "axle_height_inches=%lf\n", &cal->axle_height_inches);
    fscanf(f, "apply_height_correction=%d\n", &cal->apply_height_correction);
    fscanf(f, "is_calibrated=%d\n", &cal->is_calibrated);

    fclose(f);
    return 0;
}

// ============================================================
// MEASUREMENT FUNCTIONS
// ============================================================

/*
 * Convert pixel distance between two vertical lines to real-world
 * distance in inches.
 *
 * line1_x, line2_x: pixel X coordinates of the two measurement lines
 *
 * Returns: distance in inches, or -1.0 if not calibrated
 */
double measure_axle_distance(const AxleCalibration *cal,
                              int line1_x, int line2_x) {
    if (!cal->is_calibrated) return -1.0;

    double pixel_dist = (double)abs(line2_x - line1_x);
    double distance = pixel_dist / cal->pixels_per_inch;

    // Apply height correction if enabled
    // Objects at height h appear (H/(H-h)) times larger than objects at ground level
    // If calibration was done at ground level but measurement is at axle height:
    //   actual_distance = measured_distance * (H - h) / H
    if (cal->apply_height_correction &&
        cal->camera_height_inches > 0 &&
        cal->axle_height_inches > 0) {
        distance = distance * (cal->camera_height_inches - cal->axle_height_inches)
                 / cal->camera_height_inches;
    }

    return distance;
}

/*
 * Format distance for display.
 * Outputs feet and inches, e.g., "15' 4.5\""
 */
void format_distance(double inches, char *buf, int buf_size) {
    int feet = (int)(inches / 12.0);
    double remaining_inches = fmod(inches, 12.0);
    snprintf(buf, buf_size, "%d' %.1f\"", feet, remaining_inches);
}
```

### Integration with FFmpeg/Framebuffer Rendering

The measurement system operates purely on pixel coordinates — it does not process
the video frames themselves. The integration points are:

```c
// In your main render loop:
void render_frame(FrameBuffer *fb, VideoFrame *frame) {
    // 1. Render the video frame to the framebuffer (existing code)
    render_video(fb, frame);

    // 2. Draw the two measurement lines (existing or new code)
    //    line1_x and line2_x are controlled by joystick input / keystrokes
    draw_vertical_line(fb, line1_x, COLOR_RED);
    draw_vertical_line(fb, line2_x, COLOR_RED);

    // 3. Calculate and display distance
    if (calibration.is_calibrated) {
        double dist = measure_axle_distance(&calibration, line1_x, line2_x);
        char dist_str[64];
        format_distance(dist, dist_str, sizeof(dist_str));

        // Draw distance text on the framebuffer
        // Position: centered between the two lines, near the top
        int text_x = (line1_x + line2_x) / 2;
        int text_y = 30;
        draw_text(fb, text_x, text_y, dist_str, COLOR_WHITE);
    }
}
```

### Calibration UI Flow

```c
// Calibration mode — triggered by operator (e.g., key combo or menu)
typedef enum {
    CAL_IDLE,
    CAL_WAITING_REF1,    // "Move line to reference marker 1 and press ENTER"
    CAL_WAITING_REF2,    // "Move line to reference marker 2 and press ENTER"
    CAL_ENTER_DISTANCE,  // "Enter known distance" (or use pre-configured value)
    CAL_COMPLETE
} CalibrationState;

void calibration_step(AxleCalibration *cal, CalibrationState *state,
                      int current_line_x, double known_distance) {
    switch (*state) {
        case CAL_WAITING_REF1:
            cal->ref1_px = current_line_x;
            *state = CAL_WAITING_REF2;
            break;
        case CAL_WAITING_REF2:
            cal->ref2_px = current_line_x;
            *state = CAL_ENTER_DISTANCE;
            break;
        case CAL_ENTER_DISTANCE:
            cal->ref_distance_inches = known_distance;
            calibration_compute(cal);
            calibration_save(cal, "/etc/axle_cal.conf");
            *state = CAL_COMPLETE;
            break;
        default:
            break;
    }
}
```

---

## 12. Full Implementation Algorithm

### Step-by-step algorithm for each measurement:

```
1. SETUP (one-time):
   a. Mount camera in fixed position overlooking scale
   b. Install two reference markers at known distance apart
   c. Run calibration procedure:
      - Position line over marker 1 -> record pixel X
      - Position line over marker 2 -> record pixel X
      - Store known distance
      - Compute pixels_per_inch = |pixel2 - pixel1| / distance
      - Save calibration to file

2. MEASUREMENT (each truck):
   a. Truck drives onto scale and stops
   b. Operator views live video feed
   c. Operator uses joystick 1 to move line 1 to leftmost axle
   d. Operator uses joystick 2 to move line 2 to rightmost axle
   e. System computes: distance = |line2_x - line1_x| / pixels_per_inch
   f. Optionally apply height correction
   g. Display distance in feet and inches
   h. Operator confirms / records the measurement

3. VALIDATION (periodic):
   a. Place a known-length object on the scale (e.g., 10-foot pole)
   b. Measure it with the system
   c. Compare to actual length
   d. If error > acceptable threshold, recalibrate
```

---

## 13. When to Upgrade to Homography

Upgrade from simple linear calibration to homography-based calibration when ANY of
these conditions are true:

| Condition | Test | Solution |
|-----------|------|----------|
| Camera angle > 15 degrees from vertical | Multi-point calibration shows > 5% variation in pixels_per_inch | 4-point homography |
| Measurements span > 70% of image width | Lens distortion visible at edges | Multi-point or distortion correction |
| Accuracy requirement < 0.5 inch | Linear calibration verified to be insufficient | Full camera calibration |
| Camera has visible barrel distortion | Straight lines appear curved in image | Lens distortion correction + homography |
| Multiple measurement axes needed | Need to measure both along and across truck | 2D homography required |

### Homography Implementation (if needed)

Computing a 3x3 homography matrix from 4 point correspondences requires solving
an 8x8 linear system. Here is a self-contained C implementation with no external
dependencies:

```c
/*
 * Compute 3x3 homography matrix from 4 point correspondences.
 *
 * src[4][2] = pixel coordinates of 4 reference points
 * dst[4][2] = world coordinates of 4 reference points
 * H[3][3]   = output homography matrix
 *
 * Maps src -> dst: [x', y', w]^T = H * [u, v, 1]^T
 *   world_x = x' / w
 *   world_y = y' / w
 */
int compute_homography(double src[4][2], double dst[4][2], double H[3][3]) {
    // Build 8x9 matrix A for the system Ah = 0
    // Each point correspondence gives 2 equations:
    //   -u*X' - v*Y' - X' + u*x'*X' + v*x'*Y' + x' ... (standard DLT formulation)
    //
    // Simplified approach: solve 8x8 system with h33 = 1 normalization

    double A[8][8];
    double b[8];

    for (int i = 0; i < 4; i++) {
        double u = src[i][0], v = src[i][1];
        double x = dst[i][0], y = dst[i][1];

        // Row 2i:   u*h11 + v*h12 + h13 - u*x*h31 - v*x*h32 = x
        A[2*i][0] = u;  A[2*i][1] = v;  A[2*i][2] = 1;
        A[2*i][3] = 0;  A[2*i][4] = 0;  A[2*i][5] = 0;
        A[2*i][6] = -u*x; A[2*i][7] = -v*x;
        b[2*i] = x;

        // Row 2i+1: u*h21 + v*h22 + h23 - u*y*h31 - v*y*h32 = y
        A[2*i+1][0] = 0;  A[2*i+1][1] = 0;  A[2*i+1][2] = 0;
        A[2*i+1][3] = u;  A[2*i+1][4] = v;  A[2*i+1][5] = 1;
        A[2*i+1][6] = -u*y; A[2*i+1][7] = -v*y;
        b[2*i+1] = y;
    }

    // Gaussian elimination to solve 8x8 system
    // (standard textbook implementation)
    for (int col = 0; col < 8; col++) {
        // Find pivot
        int pivot = -1;
        double max_val = 0;
        for (int row = col; row < 8; row++) {
            if (fabs(A[row][col]) > max_val) {
                max_val = fabs(A[row][col]);
                pivot = row;
            }
        }
        if (pivot < 0 || max_val < 1e-10) return -1; // Singular

        // Swap rows
        if (pivot != col) {
            for (int j = 0; j < 8; j++) {
                double tmp = A[col][j]; A[col][j] = A[pivot][j]; A[pivot][j] = tmp;
            }
            double tmp = b[col]; b[col] = b[pivot]; b[pivot] = tmp;
        }

        // Eliminate
        for (int row = 0; row < 8; row++) {
            if (row == col) continue;
            double factor = A[row][col] / A[col][col];
            for (int j = col; j < 8; j++) {
                A[row][j] -= factor * A[col][j];
            }
            b[row] -= factor * b[col];
        }
    }

    // Extract solution
    double h[8];
    for (int i = 0; i < 8; i++) {
        h[i] = b[i] / A[i][i];
    }

    H[0][0] = h[0]; H[0][1] = h[1]; H[0][2] = h[2];
    H[1][0] = h[3]; H[1][1] = h[4]; H[1][2] = h[5];
    H[2][0] = h[6]; H[2][1] = h[7]; H[2][2] = 1.0;

    return 0;
}

/*
 * Apply homography to convert pixel coordinates to world coordinates.
 */
void pixel_to_world(const double H[3][3], double px, double py,
                    double *wx, double *wy) {
    double w  = H[2][0] * px + H[2][1] * py + H[2][2];
    *wx = (H[0][0] * px + H[0][1] * py + H[0][2]) / w;
    *wy = (H[1][0] * px + H[1][1] * py + H[1][2]) / w;
}

/*
 * Measure distance between two pixel points using homography.
 */
double measure_with_homography(const double H[3][3],
                                int line1_x, int line1_y,
                                int line2_x, int line2_y) {
    double wx1, wy1, wx2, wy2;
    pixel_to_world(H, line1_x, line1_y, &wx1, &wy1);
    pixel_to_world(H, line2_x, line2_y, &wx2, &wy2);
    double dx = wx2 - wx1;
    double dy = wy2 - wy1;
    return sqrt(dx*dx + dy*dy);
}
```

---

## 14. Recommendations Summary

### For the Truck Scale Axle Measurement System:

1. **Start with two-point linear calibration** — it is simple, requires no external
   libraries, and is accurate enough for axle spacing measurement when the camera is
   mounted overhead.

2. **Mount the camera as close to directly overhead as possible** — this is the single
   most important factor for accuracy. Even 10-15 degrees off vertical is acceptable.
   Avoid side-mounted cameras at steep angles.

3. **Mount the camera as high as practical** — higher camera = less parallax error from
   height differences. 30 feet minimum, 50+ feet ideal.

4. **Use a 1080p camera minimum**, 4K preferred — with a 60-foot field of view, 1080p
   gives ~4 pixels/inch (0.25 inch resolution). 4K doubles this to ~8 pixels/inch.

5. **Place reference markers at axle height if possible** — even roughly matching the
   height (mounting stakes on 18-inch posts) eliminates the largest source of systematic
   error. If not possible, apply the height correction formula.

6. **Avoid wide-angle lenses** — use a moderate focal length lens to minimize barrel
   distortion. 8-12mm on a 1/2.7" sensor is a good starting point.

7. **Validate the calibration** — after setup, measure a known object (e.g., place a
   10-foot reference bar on the scale) and verify the system reads correctly. Do this
   monthly.

8. **The simple approach works** — for this application, you do NOT need OpenCV,
   homography, or complex computer vision. A single division operation
   (`pixel_distance / pixels_per_inch`) gives you the real-world distance. Upgrade to
   homography only if the camera cannot be mounted overhead.

### Expected Accuracy:

| Setup | Expected Accuracy |
|-------|-------------------|
| Overhead 1080p, 60ft FOV, ground-level calibration | +/- 2-3 inches |
| Overhead 1080p, 60ft FOV, height-corrected | +/- 1-2 inches |
| Overhead 4K, 60ft FOV, axle-height calibration | +/- 0.5-1 inch |
| Angled 1080p with homography | +/- 2-4 inches |
| Angled 1080p with linear calibration (no correction) | +/- 4-8 inches (unreliable) |

---

## 15. Sources

- [Camera Calibration Using Homography Estimation - Galliot](https://galliot.us/blog/camera-calibration-using-homography-estimation/)
- [OpenCV: Basic concepts of the homography explained with code](https://docs.opencv.org/4.x/d9/dab/tutorial_homography.html)
- [OpenCV: Camera Calibration and 3D Reconstruction](https://docs.opencv.org/4.x/d9/d0c/group__calib3d.html)
- [Building an Interactive Distance Measurement Tool with OpenCV and Homography](https://medium.com/@pkusolruangchai/building-an-interactive-distance-measurement-tool-in-python-with-opencv-and-homography-fb8719811c11)
- [Camera Calibration and World Coordinates - Adaptive Vision](https://docs.adaptive-vision.com/4.12/studio/machine_vision_guide/CameraCalibrationAndWorldCoordinates.html)
- [Find distance from camera to object using Python and OpenCV](https://pyimagesearch.com/2015/01/19/find-distance-camera-objectmarker-using-python-opencv/)
- [From Pixels to Meters - Algorithms for Automated Driving (Inverse Perspective Mapping)](https://thomasfermi.github.io/Algorithms-for-Automated-Driving/LaneDetection/InversePerspectiveMapping.html)
- [Parallax Error in Video-Image Systems (ResearchGate)](https://www.researchgate.net/publication/236211165_Parallax_Error_in_Video-Image_Systems)
- [Parallax Error (Paul Bourke)](https://paulbourke.net/miscellaneous/parallaxerror/)
- [Homography examples using OpenCV (LearnOpenCV)](https://learnopencv.com/homography-examples-using-opencv-python-c/)
- [Measuring Planar Objects with a Calibrated Camera - MATLAB](https://www.mathworks.com/help/vision/ug/measuring-planar-objects-with-a-calibrated-camera.html)
- [Distortion (optics) - Wikipedia](https://en.wikipedia.org/wiki/Distortion_(optics))
- [Effects of camera external parameters error on measurement accuracy in monocular vision](https://www.sciencedirect.com/science/article/abs/pii/S0263224124002987)
- [Resolution of sensors - industrial cameras (Vision Doctor)](https://www.vision-doctor.com/en/camera/resolution-of-sensors.html)
- [Single-camera distance estimation (Patent WO2013126989A1)](https://patents.google.com/patent/WO2013126989A1/en)
- [Weigh in motion - Wikipedia](https://en.wikipedia.org/wiki/Weigh_in_motion)
- [Comparative Accuracy Analysis of Truck Weight Measurement Techniques (MDPI)](https://www.mdpi.com/2076-3417/11/2/745)
