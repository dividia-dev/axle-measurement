# Side-View Camera Axle Distance Measurement

## Updated Analysis — 2026-05-08

### Corrected Understanding

The camera is mounted at the **side** of the truck scale, viewing the truck in profile.
This is a side view — you see the wheels/tires, and the operator places vertical lines
on the outermost axle positions to measure the overall axle span.

The existing competitor product is **AxlEye**, which does exactly this.

### Why Side View Simplifies the Problem

1. **All measurement points are coplanar** — every tire contacts the scale surface
   at the same height (ground level). There is no height difference between measurement
   points, eliminating the parallax concern from the previous overhead analysis.

2. **1D measurement problem** — we only care about horizontal pixel distance along
   the ground plane. Vertical position is irrelevant.

3. **Linear calibration is ideal** — two reference markers at a known distance apart,
   at ground level, along the direction of travel. The pixels-per-inch ratio is
   effectively constant across the measurement zone because all measured points
   are at the same depth from the camera.

### Perspective Distortion — The Main Concern

With a side view, the primary distortion source is the **depth difference between the
near side and far side of the truck**. A truck is ~8 feet wide. If an axle on the
near side of the truck is closer to the camera, it appears at a slightly different
horizontal position than an axle on the far side.

**However, this is negligible because:**
- The operator is placing lines on the visible profile of the truck
- Both the reference markers and the truck axles are at approximately the same
  distance from the camera (the near side of the scale)
- The scale surface is flat, so the ground contact points of all tires on the
  visible side are at the same depth

**The only scenario where depth matters:**
- If the camera is very close to the scale (< 20 feet), the truck's width (8 feet)
  creates a measurable perspective difference. An axle on the near edge of the
  truck vs. the far edge would appear at slightly different positions.
- At 50+ feet camera distance, this is negligible (8/50 = 16% maximum depth
  variation, but since we're measuring along the ground line, the actual effect
  on horizontal position is much smaller)

### Calibration For Side View

#### Reference Marker Placement

Place two permanent, visible markers along the scale, at ground level, on the
**camera side** of the scale:

- Stakes, painted marks, bollards, or reflective markers
- Must be visible in the camera view at all times (not blocked by trucks)
- Placed at the ends of the useful measurement zone
- Measured precisely — this is the single most important accuracy factor

**Good placement options:**
- Metal stakes driven into the ground at each end of the scale
- Painted marks on the scale foundation wall/curb
- Reflective tape strips on permanent structures flanking the scale
- Bollards or posts with reflective markers at a known height

**Key requirement:** The markers must be at the same distance from the camera as the
truck's visible axle line. If the markers are on the near edge of the scale and the
truck tires are on the far edge (8 feet further from camera), there will be a small
scaling error. Ideally, markers are in-line with where the tires will be.

#### The Math (Same as Before, Simplified)

```
Calibration:
  pixels_per_inch = |marker2_pixel_x - marker1_pixel_x| / known_distance_inches

Measurement:
  axle_distance_inches = |line2_pixel_x - line1_pixel_x| / pixels_per_inch

Display:
  feet = floor(axle_distance_inches / 12)
  inches = axle_distance_inches mod 12
  → "41 ft 3 in" or just round to feet: "41 ft"
```

### Camera Placement Recommendations (Side View)

#### Position
- Mounted to the side of the scale, facing perpendicular to the direction of travel
- As far from the scale as practical — 50+ feet preferred
- Camera should be centered on the scale lengthwise (so the truck is centered in frame)
- Height: roughly ground level to slightly elevated (2-6 feet)
  - Too low: truck undercarriage obscures axles
  - Too high: looking down creates vertical perspective
  - Ideal: roughly axle height (2-3 feet) for best side profile view

#### Lens
- Moderate telephoto preferred — reduces perspective distortion
- For a camera 50 feet away viewing a 70-foot span:
  - On 1/2.7" sensor: ~8-12mm focal length
  - On 1/2" sensor: ~12-16mm focal length
- Avoid wide angle — it compresses the edges of the frame

#### Resolution vs. FOV for Side View

```
Scale length to cover: ~70 feet (truck + margin)
Horizontal resolution needed:

1080p (1920 pixels):
  1920 / (70 * 12) = 2.3 pixels per inch
  Resolution limit: ~0.44 inches per pixel
  Practical accuracy: +/- 1-2 inches (single pixel uncertainty)

4K (3840 pixels):
  3840 / (70 * 12) = 4.6 pixels per inch
  Resolution limit: ~0.22 inches per pixel
  Practical accuracy: +/- 0.5-1 inch

The competitor AxlEye appears to display measurements rounded to whole feet,
suggesting +/- 6 inch accuracy is considered acceptable for this application.
```

### Comparison to Competitor (AxlEye)

From the screenshot provided:
- AxlEye shows side-view camera with two blue vertical lines
- Measurement displayed as whole feet ("41 ft")
- Integrates with scale weight data ("Table 1: 73,271 lbs")
- Compares against permit weight ("Permit: 86,500 lbs")
- Has a "Settings" panel and "Saving..." indicator
- Shows a numeric keypad overlay (bottom right)
- Displays timestamp overlay on the video
- Shows a smaller reference image (bottom left, appears to be a saved/comparison view)
- Running on a Windows desktop

### What We Need to Build (Software Side)

To replicate and improve on AxlEye within Spot Decoder:

1. **Two movable vertical lines** drawn on the video stream
   - Full-height vertical lines in a visible color (blue, red, or green)
   - Each line controlled independently by the joystick controller
   - Lines persist across frames (position stored, redrawn each frame)

2. **Distance calculation and display**
   - Pixel distance between lines → real-world distance via calibration
   - Display as feet/inches or just feet (configurable)
   - Overlay the measurement on the video feed

3. **Calibration mode**
   - Operator positions lines over two reference markers
   - Enters known distance
   - System stores calibration data persistently

4. **Integration with scale data** (future)
   - Read weight from scale system
   - Compare against permit limits
   - Display weight and permit data alongside measurement

### Integration with Spot Decoder

Spot Decoder already has:
- ✅ Video stream rendering (FFmpeg → framebuffer)
- ✅ Overlay system (labels, boxes — could be adapted for lines)
- ✅ Keyboard/keypad input handling
- ✅ HTTP API for external control
- ✅ Configuration file system
- ✅ Text rendering on video frames

Needs to be added:
- ❌ Vertical line overlay type (thin box = 1-2px wide, full frame height)
- ❌ Line position controlled by keyboard input (our controller's keystrokes)
- ❌ Distance calculation from line positions
- ❌ Calibration data storage and UI
- ❌ Measurement display overlay

### Vertical Line Drawing Using Existing Box Overlay

The simplest implementation: use the existing box overlay with width=2 and
height=frame_height. Each "line" is just a very thin box.

Spot Decoder supports 4 boxes per stream. We need 2 for measurement lines,
leaving 2 for other purposes (reference marker indicators, etc.).

```
Box 0: Line 1 (left axle)  → x=line1_x, y=0, width=2, height=frame_height, color=0000FF
Box 1: Line 2 (right axle) → x=line2_x, y=0, width=2, height=frame_height, color=0000FF
```

The box positions would be updated via the existing HTTP API:
```
GET /?stream=N&box=0&x=<line1_x>&y=0&width=2&height=1080&color=0000FF&enabled=1
GET /?stream=N&box=1&x=<line2_x>&y=0&width=2&height=1080&color=0000FF&enabled=1
```

This means we could prototype the measurement UI WITHOUT modifying Spot Decoder's
C code — just use the existing HTTP API to position thin boxes as lines.
