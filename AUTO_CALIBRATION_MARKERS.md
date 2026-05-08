# Auto-Calibration Marker Detection — V2 Design

## 2026-05-08

## Overview

Two brightly colored reference markers are mounted on the scale guide rail (the yellow
rail visible in the AxlEye screenshot), on the camera side. The software automatically
detects these markers in every frame (or periodically) and recalibrates the
pixels-per-inch ratio without operator intervention.

If the camera gets bumped, zoomed, or drifts over time, the system self-corrects
automatically.

---

## Physical Marker Design

### Placement
- Mounted on the **near-side guide rail** (the rail between the camera and the truck)
- Two markers, one near each end of the scale
- Precisely measured distance between them (e.g., 40 feet / 480 inches)
- Markers face the camera directly

### Marker Properties
- **Two different colors** — one marker is bright RED, the other is bright GREEN
  - Using two colors lets us identify which is which (left vs right)
  - Avoids confusion if other objects in the scene happen to match one color
- **Size:** 4-6 inches diameter or square — large enough to be clearly detected at
  camera distance but not so large as to be visually obtrusive
- **Material:** Reflective or fluorescent paint/material for visibility in all lighting
  - Retroreflective sheeting (like road signs) works well day and night
  - If the facility operates at night, consider small LED markers (always-on red and
    green LEDs behind diffuser caps)
- **Shape:** Circular preferred — a circle appears as a circle regardless of slight
  camera angle, making center-detection easier. But a square works fine too.
- **Mounting:** Bolted or welded to the guide rail. Must not move. The known distance
  between markers is the calibration reference — if a marker moves, calibration is wrong.

### Why Guide Rail Placement Works

```
Top-down view:
                                                    
  [CAMERA] -----> [MARKER_R]=====[GUIDE RAIL]=====[MARKER_G] 
                        |                              |
                        |        [TRUCK ON SCALE]      |
                        |                              |
                  [far rail]========================[far rail]

- Markers are on the NEAR rail, always visible to camera
- Truck drives between the rails, never blocks the markers
- Markers are at the same depth as the truck's near-side tires
  (actually slightly closer to camera, but the difference is
  the rail-to-tire distance, maybe 1-2 feet — negligible at
  50+ feet camera distance)
```

---

## Detection Algorithm

### Approach: Color Threshold Scanning

This is intentionally simple — no machine learning, no edge detection, no feature
matching. We're looking for two bright, known-color blobs in a video frame. The markers
are designed to be the only bright red and bright green objects in the scene.

### Color Space: HSV

Convert the detection region from RGB to HSV (Hue, Saturation, Value). HSV is far
better than RGB for color detection because it separates color (hue) from brightness
(value), making detection robust across lighting conditions.

```
RGB to HSV conversion:
  H = hue (0-360 degrees): the actual color
  S = saturation (0-255): how vivid the color is
  V = value (0-255): how bright it is

Red marker detection thresholds:
  H: 0-15 or 345-360 (red wraps around the hue circle)
  S: > 100 (must be vivid, not washed out)
  V: > 80 (must be reasonably bright, not in shadow)

Green marker detection thresholds:
  H: 80-160
  S: > 100
  V: > 80

These thresholds are tunable per-installation to account for:
  - Specific marker color shades
  - Lighting conditions (fluorescent vs. natural vs. IR)
  - Camera white balance
```

### Detection Steps

```
1. DEFINE SEARCH REGIONS
   - Don't scan the entire frame — the markers are always in approximately
     the same vertical band (near the bottom of the frame, on the rail)
   - Define two search rectangles:
     - Left search region:  x=0 to x=frame_width/3, y=frame_height*0.4 to y=frame_height
     - Right search region: x=frame_width*2/3 to x=frame_width, y=frame_height*0.4 to y=frame_height
   - This reduces the scan area to ~1/3 of the frame, improving speed
   - These regions are configurable and saved in calibration config

2. SCAN FOR COLOR MATCHES
   For each search region:
     - Iterate through pixels (can skip every 2nd or 3rd pixel for speed)
     - Convert RGB to HSV
     - Check if pixel matches the target color thresholds
     - Record matching pixel coordinates

3. CLUSTER MATCHING PIXELS
   - Matching pixels should form a tight cluster (the marker)
   - Find the centroid (average X, average Y) of all matching pixels
   - If fewer than MIN_PIXELS matches found, marker not detected
   - If matching pixels are too spread out (not a single blob), reject

4. COMPUTE MARKER CENTER
   - The centroid X coordinate is the marker's pixel position
   - This is what we use for calibration

5. VALIDATE
   - Both markers must be detected
   - The distance between them (in pixels) must be within a sane range
     (e.g., at least 50% of frame width — if it's less, something is wrong)
   - The vertical positions should be roughly similar (both on the rail)

6. RECALIBRATE
   - pixels_per_inch = |marker_R_x - marker_G_x| / known_distance_inches
   - Compare to previous calibration value
   - If change > threshold (e.g., 2%), update and log the change
   - If change > large_threshold (e.g., 10%), alert operator — camera
     may have been significantly disturbed
```

### C Implementation

```c
#include <math.h>
#include <string.h>

// ============================================================
// COLOR DETECTION TYPES
// ============================================================

typedef struct {
    int h_min, h_max;    // Hue range (0-360)
    int s_min;           // Minimum saturation (0-255)
    int v_min;           // Minimum value/brightness (0-255)
    int h_wrap;          // 1 if hue range wraps around 360 (red)
    int h_min2, h_max2;  // Second hue range (for red wrap-around)
} ColorThreshold;

typedef struct {
    int x, y;            // Search region top-left
    int width, height;   // Search region dimensions
} SearchRegion;

typedef struct {
    int detected;        // 1 if marker found, 0 if not
    int center_x;        // Pixel X of marker center
    int center_y;        // Pixel Y of marker center
    int pixel_count;     // Number of matching pixels (confidence indicator)
} MarkerResult;

typedef struct {
    // Marker color thresholds
    ColorThreshold red_thresh;
    ColorThreshold green_thresh;

    // Search regions (where to look in the frame)
    SearchRegion red_region;
    SearchRegion green_region;

    // Known distance between markers
    double known_distance_inches;

    // Detection parameters
    int min_pixels;          // Minimum matching pixels to consider detected
    int max_spread;          // Maximum pixel spread (reject if too scattered)
    int scan_skip;           // Skip every N pixels for speed (1=every pixel, 2=every other)

    // Recalibration thresholds
    double recal_threshold;  // Fractional change to trigger recalibration (e.g., 0.02 = 2%)
    double alert_threshold;  // Fractional change to trigger operator alert (e.g., 0.10 = 10%)

    // Current calibration
    double pixels_per_inch;
    int is_calibrated;

    // Last known marker positions
    int last_red_x;
    int last_green_x;
} AutoCalibration;

// ============================================================
// RGB TO HSV CONVERSION
// ============================================================

static void rgb_to_hsv(int r, int g, int b, int *h, int *s, int *v) {
    int max_c = r > g ? (r > b ? r : b) : (g > b ? g : b);
    int min_c = r < g ? (r < b ? r : b) : (g < b ? g : b);
    int delta = max_c - min_c;

    *v = max_c;

    if (max_c == 0) {
        *s = 0;
        *h = 0;
        return;
    }

    *s = (int)(255.0 * delta / max_c);

    if (delta == 0) {
        *h = 0;
        return;
    }

    double hue;
    if (max_c == r) {
        hue = 60.0 * (double)(g - b) / delta;
    } else if (max_c == g) {
        hue = 60.0 * (2.0 + (double)(b - r) / delta);
    } else {
        hue = 60.0 * (4.0 + (double)(r - g) / delta);
    }

    if (hue < 0) hue += 360;
    *h = (int)hue;
}

// ============================================================
// COLOR MATCH CHECK
// ============================================================

static int color_matches(int r, int g, int b, const ColorThreshold *thresh) {
    int h, s, v;
    rgb_to_hsv(r, g, b, &h, &s, &v);

    if (s < thresh->s_min || v < thresh->v_min) return 0;

    if (thresh->h_wrap) {
        // Red wraps around 360: check both ranges
        if ((h >= thresh->h_min && h <= thresh->h_max) ||
            (h >= thresh->h_min2 && h <= thresh->h_max2)) {
            return 1;
        }
    } else {
        if (h >= thresh->h_min && h <= thresh->h_max) {
            return 1;
        }
    }

    return 0;
}

// ============================================================
// MARKER DETECTION
// ============================================================

/*
 * Detect a colored marker within a search region of an RGB frame.
 *
 * frame_rgb:    pointer to RGB pixel data (3 bytes per pixel, row-major)
 * frame_width:  width of the full frame in pixels
 * frame_height: height of the full frame in pixels
 * region:       search region within the frame
 * thresh:       color thresholds for this marker
 * skip:         scan every Nth pixel for speed (1 = every pixel)
 * min_pixels:   minimum matching pixels to consider marker detected
 * result:       output marker detection result
 */
void detect_marker(const unsigned char *frame_rgb,
                   int frame_width, int frame_height,
                   const SearchRegion *region,
                   const ColorThreshold *thresh,
                   int skip, int min_pixels,
                   MarkerResult *result) {

    long sum_x = 0, sum_y = 0;
    int count = 0;
    int min_match_x = frame_width, max_match_x = 0;
    int min_match_y = frame_height, max_match_y = 0;

    // Clamp search region to frame bounds
    int x_start = region->x > 0 ? region->x : 0;
    int y_start = region->y > 0 ? region->y : 0;
    int x_end = region->x + region->width;
    int y_end = region->y + region->height;
    if (x_end > frame_width) x_end = frame_width;
    if (y_end > frame_height) y_end = frame_height;

    for (int y = y_start; y < y_end; y += skip) {
        for (int x = x_start; x < x_end; x += skip) {
            int idx = (y * frame_width + x) * 3;
            int r = frame_rgb[idx];
            int g = frame_rgb[idx + 1];
            int b = frame_rgb[idx + 2];

            if (color_matches(r, g, b, thresh)) {
                sum_x += x;
                sum_y += y;
                count++;

                if (x < min_match_x) min_match_x = x;
                if (x > max_match_x) max_match_x = x;
                if (y < min_match_y) min_match_y = y;
                if (y > max_match_y) max_match_y = y;
            }
        }
    }

    result->pixel_count = count;

    if (count < min_pixels) {
        result->detected = 0;
        return;
    }

    result->center_x = (int)(sum_x / count);
    result->center_y = (int)(sum_y / count);
    result->detected = 1;
}

// ============================================================
// AUTO-CALIBRATION CHECK
// ============================================================

typedef enum {
    AUTOCAL_OK,                // Calibration unchanged (within threshold)
    AUTOCAL_UPDATED,           // Calibration updated (small shift detected)
    AUTOCAL_ALERT,             // Large shift detected — operator alert
    AUTOCAL_FAILED_RED,        // Red marker not found
    AUTOCAL_FAILED_GREEN,      // Green marker not found
    AUTOCAL_FAILED_BOTH        // Neither marker found
} AutoCalResult;

/*
 * Run auto-calibration check on a frame.
 *
 * Call this periodically (e.g., every 5-10 seconds, or every N frames).
 * Not every frame — it's unnecessary and wastes CPU.
 *
 * frame_rgb:    pointer to RGB pixel data
 * frame_width:  width of frame
 * frame_height: height of frame
 * cal:          auto-calibration state (read and updated)
 *
 * Returns: status code indicating what happened
 */
AutoCalResult auto_calibrate_check(const unsigned char *frame_rgb,
                                    int frame_width, int frame_height,
                                    AutoCalibration *cal) {

    MarkerResult red_result, green_result;

    // Detect both markers
    detect_marker(frame_rgb, frame_width, frame_height,
                  &cal->red_region, &cal->red_thresh,
                  cal->scan_skip, cal->min_pixels,
                  &red_result);

    detect_marker(frame_rgb, frame_width, frame_height,
                  &cal->green_region, &cal->green_thresh,
                  cal->scan_skip, cal->min_pixels,
                  &green_result);

    // Check detection
    if (!red_result.detected && !green_result.detected) return AUTOCAL_FAILED_BOTH;
    if (!red_result.detected) return AUTOCAL_FAILED_RED;
    if (!green_result.detected) return AUTOCAL_FAILED_GREEN;

    // Both markers detected — compute new calibration
    double new_ppi = (double)abs(green_result.center_x - red_result.center_x)
                    / cal->known_distance_inches;

    // Sanity check: pixels_per_inch should be positive and reasonable
    if (new_ppi < 0.1 || new_ppi > 100.0) {
        return AUTOCAL_FAILED_BOTH;  // Something is very wrong
    }

    // Store marker positions
    cal->last_red_x = red_result.center_x;
    cal->last_green_x = green_result.center_x;

    // First calibration
    if (!cal->is_calibrated) {
        cal->pixels_per_inch = new_ppi;
        cal->is_calibrated = 1;
        return AUTOCAL_UPDATED;
    }

    // Compare to current calibration
    double change = fabs(new_ppi - cal->pixels_per_inch) / cal->pixels_per_inch;

    if (change > cal->alert_threshold) {
        // Large shift — update but alert operator
        cal->pixels_per_inch = new_ppi;
        return AUTOCAL_ALERT;
    }

    if (change > cal->recal_threshold) {
        // Small shift — silently update
        cal->pixels_per_inch = new_ppi;
        return AUTOCAL_UPDATED;
    }

    return AUTOCAL_OK;  // No significant change
}

// ============================================================
// INITIALIZATION — DEFAULT VALUES
// ============================================================

void auto_cal_init_defaults(AutoCalibration *cal, int frame_width, int frame_height) {

    // Red marker thresholds (bright red, wraps around hue=0/360)
    cal->red_thresh.h_min = 0;
    cal->red_thresh.h_max = 15;
    cal->red_thresh.h_wrap = 1;
    cal->red_thresh.h_min2 = 345;
    cal->red_thresh.h_max2 = 360;
    cal->red_thresh.s_min = 100;
    cal->red_thresh.v_min = 80;

    // Green marker thresholds
    cal->green_thresh.h_min = 80;
    cal->green_thresh.h_max = 160;
    cal->green_thresh.h_wrap = 0;
    cal->green_thresh.h_min2 = 0;
    cal->green_thresh.h_max2 = 0;
    cal->green_thresh.s_min = 100;
    cal->green_thresh.v_min = 80;

    // Search regions — left 1/3 and right 1/3 of frame, bottom half
    // These should be tuned per-installation
    cal->red_region.x = 0;
    cal->red_region.y = frame_height * 2 / 5;
    cal->red_region.width = frame_width / 3;
    cal->red_region.height = frame_height * 3 / 5;

    cal->green_region.x = frame_width * 2 / 3;
    cal->green_region.y = frame_height * 2 / 5;
    cal->green_region.width = frame_width / 3;
    cal->green_region.height = frame_height * 3 / 5;

    // Detection parameters
    cal->min_pixels = 20;        // Minimum blob size
    cal->max_spread = 100;       // Maximum blob spread in pixels
    cal->scan_skip = 2;          // Check every 2nd pixel (2x speed)

    // Recalibration thresholds
    cal->recal_threshold = 0.02;  // 2% change = silent update
    cal->alert_threshold = 0.10;  // 10% change = alert operator

    // State
    cal->pixels_per_inch = 0;
    cal->is_calibrated = 0;
    cal->known_distance_inches = 0;
    cal->last_red_x = 0;
    cal->last_green_x = 0;
}
```

---

## Integration with Spot Decoder

### Where to Hook In

Spot Decoder decodes video frames via FFmpeg in `stream_thread.c`. After decoding
each frame to an AVFrame (RGB32 format), overlays are applied before rendering to
the framebuffer.

The auto-calibration check should be inserted:
- **After** frame decode
- **Before** overlay rendering
- **Not every frame** — every 150-300 frames (~5-10 seconds at 30fps)

```c
// In stream_thread.c, after frame decode:
static int frame_counter = 0;
frame_counter++;

if (frame_counter % 150 == 0) {  // Every ~5 seconds at 30fps
    AutoCalResult result = auto_calibrate_check(
        frame->data[0],       // RGB pixel data
        frame->width,
        frame->height,
        &auto_cal
    );

    switch (result) {
        case AUTOCAL_UPDATED:
            // Log: "Auto-calibration updated: %.4f ppi"
            break;
        case AUTOCAL_ALERT:
            // Display warning overlay: "CAMERA SHIFT DETECTED"
            // Log the event
            break;
        case AUTOCAL_FAILED_RED:
        case AUTOCAL_FAILED_GREEN:
        case AUTOCAL_FAILED_BOTH:
            // Marker(s) not visible — could be blocked by truck
            // Keep using last known calibration
            // If this persists for > 60 seconds with no truck, alert
            break;
        case AUTOCAL_OK:
            // No change — normal operation
            break;
    }
}
```

### Performance Considerations

Scanning a search region for color matches is fast:

```
Frame: 1920x1080
Search region: 1/3 width × 3/5 height = 640 × 648 = ~415,000 pixels
With skip=2: ~104,000 pixels checked
Two regions: ~208,000 pixels
At 3 operations per pixel (RGB to HSV + threshold): ~624,000 operations

On any modern CPU: < 1 millisecond

Running every 5 seconds: negligible CPU impact
```

### Handling Trucks Blocking Markers

When a truck is on the scale:
- The truck body/trailer may partially block the markers
- The detection will fail → `AUTOCAL_FAILED_*`
- **This is fine** — keep using the last known calibration
- The camera hasn't moved just because a truck showed up

Logic:
```
if (marker_not_found && truck_on_scale) {
    // Expected — truck is blocking the marker
    // Keep using last calibration, no alert
}

if (marker_not_found && no_truck_on_scale && duration > 60_seconds) {
    // Unexpected — marker may have fallen off or camera aimed wrong
    // Alert operator
}
```

Detecting "truck on scale" could be:
- Integration with the scale's weight data (weight > threshold = truck present)
- Or simply: if the frame has a large dark mass in the center, truck is present

---

## Marker Detection Tuning Per-Installation

### Initial Setup Procedure

During installation, the auto-calibration system needs a one-time tuning:

1. **No truck on scale.** Camera viewing empty scale with markers visible.

2. **Color sampling:** Operator clicks on each marker in the live view. System samples
   a patch of pixels around the click and computes the HSV ranges:
   - Average H ± spread → h_min, h_max
   - Minimum S observed → s_min
   - Minimum V observed → v_min

3. **Search region definition:** System uses marker positions to define search regions
   with generous margins (±100 pixels in each direction).

4. **Validation run:** System runs detection 10 times and reports success rate and
   detected positions. If < 90% detection rate, thresholds need widening.

5. **Save configuration:** All thresholds and regions saved to config file.

### Configuration File Format

```ini
[auto_calibration]
enabled=1
known_distance_inches=480.0

# Red marker (left)
red_h_min=0
red_h_max=15
red_h_wrap=1
red_h_min2=345
red_h_max2=360
red_s_min=100
red_v_min=80
red_search_x=0
red_search_y=432
red_search_w=640
red_search_h=648

# Green marker (right)
green_h_min=80
green_h_max=160
green_h_wrap=0
green_s_min=100
green_v_min=80
green_search_x=1280
green_search_y=432
green_search_w=640
green_search_h=648

# Detection
min_pixels=20
scan_skip=2
recal_threshold=0.02
alert_threshold=0.10
check_interval_frames=150
```

---

## Lighting Considerations

### Day vs Night

The guide rail area may have different lighting conditions:
- **Daytime:** Natural light, potentially direct sunlight causing glare or shadows
- **Nighttime:** Artificial lighting (sodium vapor, LED floods, etc.)
- **Dawn/dusk:** Mixed, rapidly changing

### Solutions

1. **Retroreflective markers** — reflect camera's IR illuminator or ambient light back
   strongly. Very bright and consistent in any lighting.

2. **LED markers** — small always-on LED modules (red and green) behind weatherproof
   diffuser caps. Battery or low-voltage wired. Most reliable detection in all conditions.

3. **Wider HSV thresholds** — allow more variation in brightness/saturation, but
   increases false positive risk.

4. **Time-of-day profiles** — store separate HSV thresholds for day and night,
   switch automatically based on time or frame brightness.

5. **IR markers** — use markers that are distinctive in infrared. Many IP cameras
   have IR mode. An IR-reflective marker is invisible to the eye but bright in the
   camera's IR mode.

### Recommendation

**LED markers** are the most robust and cost almost nothing:
- Red LED module: ~$2
- Green LED module: ~$2
- Weatherproof housing: ~$5 each
- 12V power from scale electrical panel
- Always consistent regardless of ambient lighting
- Extremely easy to detect (high saturation, high value)

---

## Failure Modes and Handling

| Failure | Cause | Detection | Response |
|---------|-------|-----------|----------|
| Marker not found | Truck blocking | No marker pixels in region | Use last calibration |
| Marker not found | Marker fell off / dirty | Persistent failure (>60s, no truck) | Alert operator |
| Wrong color detected | Sunset glare, clothing, vehicle color | Blob in unexpected position | Validate against expected position ±margin |
| Both markers "detected" but wrong distance | False positive on one marker | pixel distance far from expected | Reject, use last calibration |
| Camera moved significantly | Physical bump | >10% calibration change | Alert + auto-update |
| Camera zoom changed | Unauthorized adjustment | Calibration change + different pixel count in markers | Alert, recalibrate, log |
| Camera replaced | Maintenance | Very large calibration change | Force manual calibration |
| Gradual drift | Vibration, thermal expansion | Slow calibration creep | Silent auto-update |

---

## Summary

### V1 (Ship first)
- Manual one-button recalibrate
- Operator positions lines on stakes, confirms
- 30-second process, only needed when camera changes

### V2 (Add after V1 is working)
- Two colored markers (LED-lit red and green) on near-side guide rail
- Automatic color-threshold detection every ~5 seconds
- Silent recalibration for small shifts
- Operator alert for large shifts
- Graceful handling of truck blocking markers
- Per-installation color tuning during setup
- All C code, no external dependencies
