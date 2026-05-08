# Spot Decoder — Analysis for CV Axle Controller Integration

**Repo location:** `/Users/mustangdas/Desktop/dev_projects/spot-decoder`
**Version:** 5.1.90 (Release 20180811)
**Language:** C with HTML5/jQuery web UI
**Platform:** Linux (CentOS/RHEL, framebuffer-based display)

## What Spot Decoder Does

Reads RTSP video streams and displays them on a monitor with PTZ camera control. Supports up to 36 simultaneous streams across 40 views, with physical keypad, IR remote, and joystick input.

## Input System (Relevant to Our Controller)

### Supported Input Devices
Spot Decoder already detects and processes three input types from `/dev/input/eventN`:

1. **Physical Keyboard/Keypad** — detected by keywords: "Keypad", "keyboard", "MicroPad", specific USB HID IDs
2. **IR Remote** — detected by "CIR", "cir", "ir", "transceiver"
3. **Joystick** — detected by keyword "AXIS", uses EV_ABS (analog) and EV_KEY (buttons)

### How Input Flows
```
Physical device → /dev/input/eventN → input_wait() in input.c
  → device-specific mapping (IR codes, joystick buttons, keycodes)
  → ASCII map → main event loop → command execution
```

### Existing Joystick Support
- Joystick axes send `INPKT_TYPE_JOYSTICK` packets
- Values range -127 to +127 with ±6 unit deadzone
- X, Y, Z axes supported
- Joystick buttons map to F-keys:
  - BTN_TRIGGER → KEY_F1
  - BTN_THUMB → KEY_F2
  - BTN_THUMB2 → KEY_F3
  - BTN_TOP → KEY_F4
  - BTN_TOP2 → KEY_KPMINUS
  - BTN_PINKIE → KEY_KPPLUS

### Key Mappings Used
- Keypad digits 0-9 → view selection
- `*` + number → recall PTZ preset
- `**` + number → save PTZ preset
- `/` + number → select PTZ control stream
- `.` → toggle auto-scan
- `+`/`-` → navigate views
- F1-F4 → preset shortcuts (*1 through *4)
- Enter → confirm command
- Backspace → delete input

### PTZ Motion Control
- Joystick axes drive `ptz_motion_ctl()` directly
- Values rescaled by camera driver (e.g., Axis: 0-99 range, 15-unit steps)
- Supports Axis, Panasonic, Zavio, Acti, Foscam cameras
- Speed configurable per-stream (ptz_speed 0-100%)

## Existing Overlay Capabilities

### What Already Exists
- **Text labels** — up to 16 per stream, configurable position/size/color via HTTP API
- **Colored boxes** — up to 4 per stream, filled rectangles with position/size/color
- **Stream name and timestamp overlays** — corner-positioned
- **Font rendering** — 16x16, 24x32, 40x64 pixel fonts

### HTTP API for Overlays
- Labels: `?stream=N&label=M&text=...&x=...&y=...&size=...&color=...&enabled=...`
- Boxes: `?stream=N&box=M&x=...&y=...&width=...&height=...&color=...&enabled=...`
- Screenshot: `?screen_shot` (captures framebuffer as JPEG)

### What Does NOT Exist
- No vertical line drawing
- No measurement/distance calculation
- No calibration tools
- No ruler or reference point markers
- No line dragging or positioning UI

## Integration Strategy for Axle Measurement

### Option A: Build on Existing Overlay System
The box overlay could draw very thin rectangles (width=1 or 2) as vertical lines. The HTTP API already supports positioning boxes at arbitrary X coordinates. Our controller would:
1. Send keystrokes to a new process/script
2. Script calls Spot Decoder HTTP API to move box overlays
3. Distance = pixel difference between two box X positions, calibrated against known reference

**Pros:** Leverages existing code, minimal changes to Spot Decoder
**Cons:** Box overlay is filled rectangle, not a clean line. Limited to 4 boxes per stream.

### Option B: Add Line Overlay Feature to Spot Decoder
Add a new overlay type specifically for vertical measurement lines:
- Rendered during frame processing (like labels/boxes)
- Full-height vertical lines with configurable color/thickness
- Position controlled via HTTP API or keyboard input
- Distance calculation built in

**Pros:** Clean implementation, purpose-built
**Cons:** Requires modifying Spot Decoder C code

### Option C: Separate Measurement Application
Build a standalone application that:
- Captures frames from the same RTSP stream
- Displays with its own overlay/measurement UI
- Completely independent from Spot Decoder

**Pros:** No Spot Decoder modifications needed
**Cons:** Duplicates video decoding, separate display

## Key Files for Future Integration
- `src/input.c` / `src/input.h` — input device handling
- `src/main.c` — main event loop, command processing
- `src/stream_thread.c` — video rendering and overlay drawing
- `src/ptz.c` — PTZ control dispatch
- `src/http_thread.c` — HTTP API endpoints
- `src/conf.c` / `src/conf.h` — configuration parsing
- `src/font.c` — text rendering on video frames

## Configuration
- Config file: `/usr/local/etc/spot-decoder.conf`
- HTTP server runs on port 88
- Streams configured with `[stream_N]` sections
- Views configured with `[view_N]` sections
