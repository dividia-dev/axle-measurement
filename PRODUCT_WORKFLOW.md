# CV Axle Controller — Product Workflow

## 2026-05-08

## Overview

End-to-end system for measuring truck axle distances and verifying weight compliance
at an aggregate/cement facility in Florida. Replaces AxlEye (predecessor product,
now out of business). Side-view camera on a truck scale, operator positions
measurement lines on outermost axles, system calculates max allowable weight per
Federal Bridge Formula B.

---

## Full Workflow

### 1. Permit Ingestion
Customer uploads Florida overweight permits into the database, keyed by VIN and
license plate number.

### 2. Vehicle Arrival
License plate is read (LPR) as truck arrives at weigh scale. System looks up the
plate/VIN in the permit database.

### 3. Permit Retrieval
If a permit is found, the system pulls axle count and max weight from the permit
and auto-populates the measurement fields.

### 4. Permit Validation (FDOT)
System checks with FDOT that the permit is still valid and matches what's on file.
If expired or mismatched, alert the operator. Handling of invalid permits TBD.

See [FDOT Permit Verification](#fdot-permit-verification) below for portal details.

### 5. No Permit on File
If no permit is found, the operator enters the axle count manually. Max weight is
calculated via the Federal Bridge Formula B algorithm.

### 6. Axle/Trailer Mismatch
If the permit's axle count doesn't match what's visually on the trailer (e.g.,
folding axles deployed vs stowed), the operator adjusts the axle number. Max weight
recalculates automatically.

### 7. Length Measurement
Operator adjusts measurement lines to determine the distance between outermost
axles. This distance plus axle count determines max allowable weight.

### 8. Weighing
Truck is on the scale (loaded before or during). Scale sends weight data in real
time. System checks actual weight against max allowed weight and alerts if over.

### 9. Scale Ticket
Wait for the scale ticket to print. The ticket is the verification of scale data,
material type, and other transaction details.

### 10. Image Capture
On ticket receipt, capture images of the vehicle: side view, front view, and
loadbed. Save images with data overlay to the NVR. Optionally upload to cloud portal.

### 11. Cloud Portal
Users view transaction records, tickets, and images via the ScaleWatcher cloud
frontend. Related repos: `cloudapi`, `scalewatcher`.

---

## Folding Axles

Some sand/aggregate trailers have a folding (lift) axle set. When folded down and
locked in position (touching the ground), the trailer has more axles and can carry
more weight. When folded up (stowed, not touching ground), fewer axles are in use
and the max weight is lower.

The operator must visually confirm whether folding axles are deployed or stowed and
adjust the axle count accordingly. This directly affects the max weight calculation.

---

## FDOT Permit Verification

### Portal: PAS (Permit Application System)

This is the primary FDOT system for overweight/oversized permits. Carriers
self-issue trip permits here. Most likely what AxlEye was using for verification.

| Resource | URL |
|----------|-----|
| PAS Login | https://pas.fdot.gov/Account.aspx/LogOn |
| Create New Account | https://pas.fdot.gov/Account.aspx/CreateNewIsa |
| Permit Office Info | https://www.fdot.gov/maintenance/owodpermits.shtm |
| PAS Demo (PDF) | https://www.fdot.gov/docs/default-source/maintenance/str/owodp/PAS_Demo.pdf |

### Portal: OSP (One Stop Permitting)

Umbrella portal for all FDOT permit types. May not have the specific overweight
permit lookup needed.

| Resource | URL |
|----------|-----|
| OSP Portal | https://osp.fdot.gov/ |

### Contact

FDOT Permit Office: **(850) 410-5777** — Mon-Fri 8am-5pm

### Open Questions

- Does the customer already have a PAS account? (likely yes, since they pull permits)
- Can we get credentials or a sub-account for automated lookups?
- Can we create a new ISA account with a Dividia email, or does it require special
  licensing or carrier registration?
- Once logged in, does the portal have a REST/JSON API behind the UI, or will we
  need browser automation / HTML scraping?
- What data fields are available in a permit lookup response?

### Next Steps

1. **Ask the customer:** "Do you have a PAS login at pas.fdot.gov? Can we get
   credentials or a sub-account to automate permit verification?"
2. **Try creating a new ISA account** at pas.fdot.gov to see what the registration
   process requires.
3. **Once we have access:** inspect network traffic during permit lookup to find the
   cleanest automation path (hidden API vs scraping).
4. **Call FDOT Permit Office** at (850) 410-5777 to ask about API or programmatic
   access options.

### Implementation Approaches (in order of preference)

1. **Hidden API** — If the portal's web UI makes REST/JSON calls, hit those directly.
   Cleanest, fastest, most reliable.
2. **Browser automation** — Playwright/Puppeteer to log in and scrape lookup results.
   Works but fragile if FDOT changes their UI.
3. **Manual/periodic** — Operator manually checks portal. Fallback if automation
   isn't feasible.

---

## Scale Ticket Integration

Scale tickets are received via webhooks and trigger image capture and transaction
recording. Two ticket providers:

### Command Alkon Apex (Current)
- Existing integration, already in production on other ScaleWatcher deployments
- Ticket event data delivered as **XML** via webhook
- Parsing already implemented in the NVR/ScaleWatcher codebase

### FastWeigh (New — V1)
- New ticket provider integration needed for this customer
- Ticket event data delivered as **JSON** via webhook
- Same webhook-driven flow as Command Alkon, different payload format
- Need to build a JSON parser alongside the existing XML parser

### Ticket Flow
1. Truck is weighed, operator finalizes transaction in scale software
2. Scale software (Apex or FastWeigh) fires a webhook with ticket data
3. System receives webhook, parses ticket (XML or JSON depending on provider)
4. Ticket triggers image capture (side, front, loadbed)
5. Images + ticket data + measurement data saved to NVR
6. Transaction record uploaded to cloud portal

---

## NVR Integration (ScaleWatcher Threads)

The Dividia NVR is central to the production workflow. The NVR codebase uses a
thread-based architecture (ScaleWatcher threads) to handle:

- Receiving and parsing scale ticket webhooks
- Triggering camera image capture on ticket events
- Storing images with data overlays
- Uploading transaction records to the cloud portal

The NVR codebase needs to be reviewed to understand how the axle measurement and
permit verification features integrate with the existing ScaleWatcher thread model.
This is required to complete the full V1 integration.

---

## System Components

| Component | Description | Location |
|-----------|-------------|----------|
| Browser App | Dev/test harness for measurement logic and joystick controller | This repo (`cv-axle-controller`) |
| Dividia NVR | Network video recorder, image capture, ScaleWatcher threads, ticket handling | Physical device / NVR codebase |
| Spot Decoder | May handle the measurement piece in production (TBD) | Separate repo (`spot-decoder`) |
| Cloud API | Backend API for cloud portal | `cloudapi` repo |
| ScaleWatcher | Cloud frontend for viewing transactions | `scalewatcher` repo |
| Command Alkon Apex | Current scale ticket provider (XML webhooks) | External system |
| FastWeigh | New scale ticket provider (JSON webhooks) | External system |

---

## Future Scope (NOT V1)

These features are planned but not in the initial release:

- **AI axle counting** — Side camera automatically counts axles as truck passes.
  Camera can be positioned closer (no wide angle needed).
- **AI distance measurement** — Calculate first-to-last axle distance as truck
  moves past camera, eliminating manual line positioning.
- **AI axle-down detection** — Automatically determine if folding axles are
  deployed or stowed.
