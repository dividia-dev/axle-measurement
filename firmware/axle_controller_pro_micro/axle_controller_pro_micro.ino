/*
 * CV Axle Controller - Production Firmware v2.0 (Clean Rewrite)
 * Arduino Pro Micro (ATmega32U4)
 *
 * USB HID keyboard emulation for CV axle measurement line control.
 * Two joysticks: X-axis = coarse movement, Y-axis/twist = fine movement.
 *
 * Architecture: per-joystick state machine with commercial-grade input handling.
 *
 * Input pipeline per axis:
 *   1. analogRead() raw sample
 *   2. Exponential moving average (EMA) low-pass filter
 *   3. Deflection from calibrated center
 *   4. Dead zone check (with hysteresis on fine axis)
 *   5. State machine decides action
 *   6. Keystroke output with rate limiting
 *
 * State machine per joystick:
 *   IDLE       - no output, waiting for input beyond dead zone
 *   COARSE     - sending coarse keystrokes at proportional rate
 *   FINE       - ratchet mode: fires on increasing deflection, silent on decrease
 *               direction locked until return to dead zone
 *   SUPPRESSED - brief lockout after fine disengages (prevents crosstalk)
 *
 * Hardware:
 *   Joystick 1: VRx=A0, VRy=A1, SW=D2
 *   Joystick 2: VRx=A2, VRy=A3, SW=D3
 *   Lock switch: D4 (toggle to GND)
 *   Lock LED:    D5 (green)
 *   Fine LED 1:  D6 (blue)
 *   Fine LED 2:  D7 (blue)
 *
 * IMPORTANT wiring notes:
 *   - Use RIGHT side GND only (left side GND has bad solder joints on clone boards)
 *   - Pot outer pins: left=GND, right=VCC
 *   - Twist barrel rests at ~35 (near electrical min), NOT at 512
 *   - Gripping to twist causes slight X-axis deflection (mechanical coupling)
 */

#include <Keyboard.h>
#include <EEPROM.h>

// ============================================================
// Pin Definitions
// ============================================================

#define JOY1_X_PIN  A0
#define JOY1_Y_PIN  A1
#define JOY1_BTN    2

#define JOY2_X_PIN  A2
#define JOY2_Y_PIN  A3
#define JOY2_BTN    3

#define LOCK_SWITCH  4
#define LED_LOCK     5
#define LED_FINE1    6
#define LED_FINE2    7

// ============================================================
// EEPROM Layout
// ============================================================

#define EEPROM_MAGIC_ADDR     0     // 1 byte: magic sentinel
#define EEPROM_MODE_ADDR      1     // 1 byte: control mode
#define EEPROM_KEYS_ADDR      2     // 10 bytes: key mappings
#define EEPROM_DZ_ADDR        12    // 4 bytes: dead zones (J1c, J1f, J2c, J2f)
#define EEPROM_CENTER_ADDR    16    // 8 bytes: centers (J1x, J1y, J2x, J2y) x 2 bytes each

#define EEPROM_MAGIC_VALUE    0xB0  // Bump this to force factory reset on flash

#define NUM_KEYS  10

// ============================================================
// Key Mapping
// ============================================================

enum KeySlot {
  K_J1_LEFT_COARSE = 0,
  K_J1_RIGHT_COARSE,
  K_J1_LEFT_FINE,
  K_J1_RIGHT_FINE,
  K_J1_BTN,
  K_J2_LEFT_COARSE,
  K_J2_RIGHT_COARSE,
  K_J2_LEFT_FINE,
  K_J2_RIGHT_FINE,
  K_J2_BTN
};

static const char DEFAULT_KEYS[NUM_KEYS] PROGMEM = {
  'a', 'd',   // J1 coarse left/right
  'q', 'e',   // J1 fine left/right
  'f',        // J1 button
  'j', 'l',   // J2 coarse left/right
  'u', 'o',   // J2 fine left/right
  ';'         // J2 button
};

char keys[NUM_KEYS];

// ============================================================
// Tuning Constants
// ============================================================

// Dead zone defaults (overwritten by auto-cal on every boot)
#define DEFAULT_DZ_COARSE   150
#define DEFAULT_DZ_FINE     30
#define DEFAULT_CENTER      512

// Fine axis hysteresis
#define FINE_ENGAGE      30    // Deflection to activate fine mode
#define FINE_DISENGAGE   12    // Deflection to deactivate (must be < ENGAGE)

// Timing
#define COARSE_REPEAT_FAST_MS   30    // Max speed at full deflection
#define COARSE_REPEAT_SLOW_MS   200   // Min speed at dead zone edge
#define FINE_MIN_INTERVAL_MS    300   // Minimum ms between fine keystrokes
#define COARSE_SUPPRESS_MS      250   // Lockout after fine disengages
#define BUTTON_DEBOUNCE_MS      200   // Button debounce
#define CAL_HOLD_MS             3000  // Hold button to start calibration
#define CAL_SAMPLE_MS           2000  // Duration of noise sampling
#define CAL_SETTLE_MS           3000  // Pre-cal settle time
#define DZ_MARGIN               25    // Added to measured noise peak
#define DZ_COARSE_FLOOR         100   // Minimum coarse dead zone
#define DZ_FINE_FLOOR            15   // Minimum fine dead zone

// EMA filter: alpha = 1/EMA_DIVISOR. Higher = more smoothing.
// Using fixed-point x256 for precision without floats.
#define EMA_SHIFT  4   // 2^4 = 16, so alpha ~= 1/16 = 0.0625

// ============================================================
// Joystick State Machine
// ============================================================

enum JoyState {
  JS_IDLE,
  JS_COARSE,
  JS_FINE,
  JS_SUPPRESSED   // Brief lockout after fine disengages
};

struct Joystick {
  // Pin assignments
  uint8_t pinX;
  uint8_t pinY;
  uint8_t pinBtn;
  uint8_t pinLed;

  // Key slots (indices into keys[] array)
  uint8_t kLeftCoarse;
  uint8_t kRightCoarse;
  uint8_t kLeftFine;
  uint8_t kRightFine;
  uint8_t kBtn;

  // Calibration (per-axis center and dead zone)
  int16_t centerX;
  int16_t centerY;
  int16_t dzCoarse;
  int16_t dzFine;

  // EMA filter state (fixed-point x16 via shift)
  int32_t emaX;
  int32_t emaY;
  bool emaInitialized;

  // State machine
  JoyState state;
  unsigned long stateEnteredAt;

  // Timing
  unsigned long lastCoarseKeystroke;
  unsigned long lastFineKeystroke;
  unsigned long lastBtnPress;
  unsigned long suppressUntil;

  // Fine ratchet mode tracking
  bool fineEngaged;
  int8_t fineDirection;       // 0=none, +1=right, -1=left (locked until return to DZ)
  int16_t finePeakDeflect;    // Highest abs deflection seen in current engagement
  int16_t fineLastDeflect;    // Previous loop's deflection (to detect increasing)

  // Calibration hold detection
  unsigned long btnHoldStart;
  bool btnWasHeld;
};

// Two joystick instances
Joystick joy[2];

// ============================================================
// Global State
// ============================================================

uint8_t controlMode = 0;   // 0 = proportional, 1 = discrete toggle
bool locked = false;

// Serial command buffer
char serialBuf[32];
uint8_t serialPos = 0;

// ============================================================
// Forward Declarations
// ============================================================

void initJoystick(uint8_t idx, uint8_t px, uint8_t py, uint8_t pb, uint8_t pl,
                  uint8_t klc, uint8_t krc, uint8_t klf, uint8_t krf, uint8_t kb);
void loadSettings();
void saveDefaults();
void saveSettings();
void saveDeadzones();
void runStartupCal();
void runManualCal(uint8_t joyIdx);
void processJoystick(uint8_t idx);
void checkCalHold(uint8_t idx);
void handleSerial();
void processCommand(char *raw);
void sendKey(char key);

// ============================================================
// Setup
// ============================================================

void setup() {
  Serial.begin(115200);

  // Configure pins
  pinMode(JOY1_BTN, INPUT_PULLUP);
  pinMode(JOY2_BTN, INPUT_PULLUP);
  pinMode(LOCK_SWITCH, INPUT_PULLUP);
  pinMode(LED_LOCK, OUTPUT);
  pinMode(LED_FINE1, OUTPUT);
  pinMode(LED_FINE2, OUTPUT);

  // Initialize joystick structs
  initJoystick(0, JOY1_X_PIN, JOY1_Y_PIN, JOY1_BTN, LED_FINE1,
               K_J1_LEFT_COARSE, K_J1_RIGHT_COARSE, K_J1_LEFT_FINE, K_J1_RIGHT_FINE, K_J1_BTN);
  initJoystick(1, JOY2_X_PIN, JOY2_Y_PIN, JOY2_BTN, LED_FINE2,
               K_J2_LEFT_COARSE, K_J2_RIGHT_COARSE, K_J2_LEFT_FINE, K_J2_RIGHT_FINE, K_J2_BTN);

  // Load key mappings and mode from EEPROM
  loadSettings();

  // Auto-calibrate centers and dead zones (before keyboard starts)
  runStartupCal();

  // NOW start keyboard — no garbage keystrokes during cal
  Keyboard.begin();

  // Startup indicator: blink lock LED 3x
  for (uint8_t i = 0; i < 3; i++) {
    digitalWrite(LED_LOCK, HIGH); delay(100);
    digitalWrite(LED_LOCK, LOW);  delay(100);
  }

  Serial.println(F("AXLE_CONTROLLER_READY v2.0"));
  printHelp();
}

// ============================================================
// Main Loop
// ============================================================

void loop() {
  handleSerial();

  // Lock switch
  locked = (digitalRead(LOCK_SWITCH) == LOW);
  digitalWrite(LED_LOCK, locked ? LOW : HIGH);

  if (locked) {
    digitalWrite(LED_FINE1, LOW);
    digitalWrite(LED_FINE2, LOW);
    return;
  }

  // Check calibration hold on each joystick
  checkCalHold(0);
  checkCalHold(1);

  // Process each joystick through the state machine
  processJoystick(0);
  processJoystick(1);
}

// ============================================================
// Joystick Initialization
// ============================================================

void initJoystick(uint8_t idx, uint8_t px, uint8_t py, uint8_t pb, uint8_t pl,
                  uint8_t klc, uint8_t krc, uint8_t klf, uint8_t krf, uint8_t kb) {
  Joystick &j = joy[idx];
  j.pinX = px;
  j.pinY = py;
  j.pinBtn = pb;
  j.pinLed = pl;
  j.kLeftCoarse = klc;
  j.kRightCoarse = krc;
  j.kLeftFine = klf;
  j.kRightFine = krf;
  j.kBtn = kb;
  j.centerX = DEFAULT_CENTER;
  j.centerY = DEFAULT_CENTER;
  j.dzCoarse = DEFAULT_DZ_COARSE;
  j.dzFine = DEFAULT_DZ_FINE;
  j.emaX = 0;
  j.emaY = 0;
  j.emaInitialized = false;
  j.state = JS_IDLE;
  j.stateEnteredAt = 0;
  j.lastCoarseKeystroke = 0;
  j.lastFineKeystroke = 0;
  j.lastBtnPress = 0;
  j.suppressUntil = 0;
  j.fineEngaged = false;
  j.fineDirection = 0;
  j.finePeakDeflect = 0;
  j.fineLastDeflect = 0;
  j.btnHoldStart = 0;
  j.btnWasHeld = false;
}

// ============================================================
// Input Pipeline: Read + Filter + Deflection
// ============================================================

// Returns filtered analog value (0-1023 range)
int16_t readFiltered(uint8_t idx, bool isY) {
  Joystick &j = joy[idx];
  int16_t raw;
  int32_t *ema;

  if (isY) {
    raw = analogRead(j.pinY);
    ema = &j.emaY;
  } else {
    raw = analogRead(j.pinX);
    ema = &j.emaX;
  }

  if (!j.emaInitialized) {
    *ema = (int32_t)raw << EMA_SHIFT;
    // Don't set emaInitialized here — both axes need init.
    // We'll set it after both are read in processJoystick.
  } else {
    // EMA: new = old + (raw - old/scale)
    // With shift: ema = ema + raw - (ema >> SHIFT)
    *ema = *ema - (*ema >> EMA_SHIFT) + raw;
  }

  return (int16_t)(*ema >> EMA_SHIFT);
}

// ============================================================
// State Machine: Process One Joystick
// ============================================================

void processJoystick(uint8_t idx) {
  Joystick &j = joy[idx];
  unsigned long now = millis();

  // --- Read and filter both axes ---
  int16_t filtX = readFiltered(idx, false);
  int16_t filtY = readFiltered(idx, true);
  if (!j.emaInitialized) j.emaInitialized = true;

  // --- Compute deflection from calibrated center ---
  int16_t deflectX = filtX - j.centerX;
  int16_t deflectY = filtY - j.centerY;
  int16_t absDeflectX = abs(deflectX);
  int16_t absDeflectY = abs(deflectY);

  // --- Handle button press (short press only, not cal hold) ---
  bool btnDown = (digitalRead(j.pinBtn) == LOW);
  if (!btnDown && j.btnWasHeld && (now - j.lastBtnPress >= BUTTON_DEBOUNCE_MS)) {
    // Button just released — fire keystroke only if it was a short press (not cal hold)
    if (now - j.btnHoldStart < CAL_HOLD_MS) {
      j.lastBtnPress = now;
      sendKey(keys[j.kBtn]);
    }
  }

  // --- Fine axis hysteresis ---
  bool finePastThreshold;
  if (j.fineEngaged) {
    finePastThreshold = (absDeflectY > FINE_DISENGAGE);
  } else {
    finePastThreshold = (absDeflectY > FINE_ENGAGE);
  }

  // --- State machine ---
  switch (j.state) {

    case JS_IDLE:
      if (finePastThreshold) {
        // Engage fine: lock direction, start ratchet tracking
        j.state = JS_FINE;
        j.stateEnteredAt = now;
        j.fineEngaged = true;
        j.fineDirection = (deflectY > 0) ? 1 : -1;
        j.finePeakDeflect = absDeflectY;
        j.fineLastDeflect = absDeflectY;
        digitalWrite(j.pinLed, HIGH);

        // Fire first keystroke immediately
        if (now - j.lastFineKeystroke >= FINE_MIN_INTERVAL_MS) {
          char key = (j.fineDirection > 0) ? keys[j.kRightFine] : keys[j.kLeftFine];
          sendKey(key);
          j.lastFineKeystroke = now;
        }
      }
      else if (absDeflectX > j.dzCoarse && now >= j.suppressUntil) {
        j.state = JS_COARSE;
        j.stateEnteredAt = now;
      }
      break;

    case JS_COARSE:
      // Fine always takes priority over coarse
      if (finePastThreshold) {
        j.state = JS_FINE;
        j.stateEnteredAt = now;
        j.fineEngaged = true;
        j.fineDirection = (deflectY > 0) ? 1 : -1;
        j.finePeakDeflect = absDeflectY;
        j.fineLastDeflect = absDeflectY;
        digitalWrite(j.pinLed, HIGH);

        if (now - j.lastFineKeystroke >= FINE_MIN_INTERVAL_MS) {
          char key = (j.fineDirection > 0) ? keys[j.kRightFine] : keys[j.kLeftFine];
          sendKey(key);
          j.lastFineKeystroke = now;
        }
        break;
      }

      if (absDeflectX <= j.dzCoarse) {
        j.state = JS_IDLE;
        j.stateEnteredAt = now;
        break;
      }

      // Coarse: proportional speed repeat
      {
        int16_t magnitude = absDeflectX;
        if (magnitude > 512) magnitude = 512;
        int repeatMs = map(magnitude, j.dzCoarse, 512, COARSE_REPEAT_SLOW_MS, COARSE_REPEAT_FAST_MS);
        if (repeatMs < COARSE_REPEAT_FAST_MS) repeatMs = COARSE_REPEAT_FAST_MS;
        if (repeatMs > COARSE_REPEAT_SLOW_MS) repeatMs = COARSE_REPEAT_SLOW_MS;

        if (now - j.lastCoarseKeystroke >= (unsigned long)repeatMs) {
          char key = (deflectX > 0) ? keys[j.kRightCoarse] : keys[j.kLeftCoarse];
          sendKey(key);
          j.lastCoarseKeystroke = now;
        }
      }
      break;

    case JS_FINE:
      if (!finePastThreshold) {
        // Returned to dead zone, disengage, enter suppression
        j.fineEngaged = false;
        j.fineDirection = 0;
        j.finePeakDeflect = 0;
        j.fineLastDeflect = 0;
        j.state = JS_SUPPRESSED;
        j.stateEnteredAt = now;
        j.suppressUntil = now + COARSE_SUPPRESS_MS;
        digitalWrite(j.pinLed, LOW);
        break;
      }

      // Ratchet fine: fire keystroke only when deflection is INCREASING
      // in the locked direction. Decreasing = do nothing. Must return
      // to dead zone to change direction.
      {
        bool correctDirection = (j.fineDirection > 0) ? (deflectY > 0) : (deflectY < 0);

        if (correctDirection && absDeflectY > j.fineLastDeflect) {
          // Deflection increasing, fire keystroke (rate-limited)
          if (now - j.lastFineKeystroke >= FINE_MIN_INTERVAL_MS) {
            char key = (j.fineDirection > 0) ? keys[j.kRightFine] : keys[j.kLeftFine];
            sendKey(key);
            j.lastFineKeystroke = now;
          }
          if (absDeflectY > j.finePeakDeflect) {
            j.finePeakDeflect = absDeflectY;
          }
        }
        // Decreasing or wrong direction: silent, stay in FINE

        j.fineLastDeflect = absDeflectY;
      }
      break;

    case JS_SUPPRESSED:
      if (now >= j.suppressUntil) {
        j.state = JS_IDLE;
        j.stateEnteredAt = now;
      }
      // Allow fine re-engagement during suppression
      if (finePastThreshold) {
        j.state = JS_FINE;
        j.stateEnteredAt = now;
        j.fineEngaged = true;
        j.fineDirection = (deflectY > 0) ? 1 : -1;
        j.finePeakDeflect = absDeflectY;
        j.fineLastDeflect = absDeflectY;
        digitalWrite(j.pinLed, HIGH);

        if (now - j.lastFineKeystroke >= FINE_MIN_INTERVAL_MS) {
          char key = (j.fineDirection > 0) ? keys[j.kRightFine] : keys[j.kLeftFine];
          sendKey(key);
          j.lastFineKeystroke = now;
        }
      }
      break;
  }

}

// ============================================================
// Key Output
// ============================================================

void sendKey(char key) {
  Keyboard.press(key);
  delay(10);
  Keyboard.release(key);
}

// ============================================================
// Calibration: Startup Auto-Cal
// ============================================================

void runStartupCal() {
  Serial.println(F("Auto-cal: hands off joysticks..."));

  // Blink LEDs during settle time
  unsigned long settleStart = millis();
  while (millis() - settleStart < CAL_SETTLE_MS) {
    bool on = ((millis() / 500) % 2) == 0;
    digitalWrite(LED_FINE1, on ? HIGH : LOW);
    digitalWrite(LED_FINE2, on ? HIGH : LOW);
    delay(50);
  }
  digitalWrite(LED_FINE1, LOW);
  digitalWrite(LED_FINE2, LOW);

  // Sample both joysticks simultaneously
  for (uint8_t idx = 0; idx < 2; idx++) {
    Joystick &j = joy[idx];
    long sumX = 0, sumY = 0;
    int16_t minX = 1023, maxX = 0, minY = 1023, maxY = 0;
    const int samples = 200;  // 2 seconds at 10ms interval

    for (int i = 0; i < samples; i++) {
      int16_t rx = analogRead(j.pinX);
      int16_t ry = analogRead(j.pinY);
      sumX += rx;
      sumY += ry;
      if (rx < minX) minX = rx;
      if (rx > maxX) maxX = rx;
      if (ry < minY) minY = ry;
      if (ry > maxY) maxY = ry;
      delay(10);
    }

    j.centerX = sumX / samples;
    j.centerY = sumY / samples;

    // Dead zone = measured noise peak + margin, with floor
    int16_t noiseX = max(abs(maxX - j.centerX), abs(minX - j.centerX));
    int16_t noiseY = max(abs(maxY - j.centerY), abs(minY - j.centerY));
    j.dzCoarse = max((int16_t)(noiseX + DZ_MARGIN), (int16_t)DZ_COARSE_FLOOR);
    j.dzFine = max((int16_t)(noiseY + DZ_MARGIN), (int16_t)DZ_FINE_FLOOR);

    // Initialize EMA with center values
    j.emaX = (int32_t)j.centerX << EMA_SHIFT;
    j.emaY = (int32_t)j.centerY << EMA_SHIFT;
    j.emaInitialized = true;

    Serial.print(F("  J"));
    Serial.print(idx + 1);
    Serial.print(F(": cX="));
    Serial.print(j.centerX);
    Serial.print(F(" cY="));
    Serial.print(j.centerY);
    Serial.print(F(" noiseX="));
    Serial.print(noiseX);
    Serial.print(F(" noiseY="));
    Serial.print(noiseY);
    Serial.print(F(" dzC="));
    Serial.print(j.dzCoarse);
    Serial.print(F(" dzF="));
    Serial.println(j.dzFine);
  }

  saveDeadzones();
  Serial.println(F("Cal done."));
}

// ============================================================
// Calibration: Manual (hold button 3s)
// ============================================================

void checkCalHold(uint8_t idx) {
  Joystick &j = joy[idx];
  bool pressed = (digitalRead(j.pinBtn) == LOW);
  unsigned long now = millis();

  if (pressed && !j.btnWasHeld) {
    j.btnHoldStart = now;
  } else if (pressed && j.btnWasHeld) {
    if (now - j.btnHoldStart >= CAL_HOLD_MS) {
      runManualCal(idx);
      j.btnHoldStart = 0;
      // Wait for button release
      while (digitalRead(j.pinBtn) == LOW) delay(10);
    }
  }
  j.btnWasHeld = pressed;
}

void runManualCal(uint8_t joyIdx) {
  Joystick &j = joy[joyIdx];

  Serial.print(F("CAL J"));
  Serial.print(joyIdx + 1);
  Serial.println(F(" - don't touch..."));

  // Fast blink LED
  for (uint8_t i = 0; i < 6; i++) {
    digitalWrite(j.pinLed, HIGH); delay(100);
    digitalWrite(j.pinLed, LOW);  delay(100);
  }
  delay(500);  // Let user release

  long sumX = 0, sumY = 0;
  int16_t minX = 1023, maxX = 0, minY = 1023, maxY = 0;
  int samples = 0;

  unsigned long start = millis();
  while (millis() - start < CAL_SAMPLE_MS) {
    int16_t rx = analogRead(j.pinX);
    int16_t ry = analogRead(j.pinY);
    sumX += rx;
    sumY += ry;
    samples++;
    if (rx < minX) minX = rx;
    if (rx > maxX) maxX = rx;
    if (ry < minY) minY = ry;
    if (ry > maxY) maxY = ry;
    digitalWrite(j.pinLed, (millis() / 80) % 2 ? HIGH : LOW);
    delay(5);
  }

  j.centerX = sumX / samples;
  j.centerY = sumY / samples;

  int16_t noiseX = max(abs(maxX - j.centerX), abs(minX - j.centerX));
  int16_t noiseY = max(abs(maxY - j.centerY), abs(minY - j.centerY));
  j.dzCoarse = max((int16_t)(noiseX + DZ_MARGIN), (int16_t)DZ_COARSE_FLOOR);
  j.dzFine = max((int16_t)(noiseY + DZ_MARGIN), (int16_t)DZ_FINE_FLOOR);

  // Re-seed EMA with new center
  j.emaX = (int32_t)j.centerX << EMA_SHIFT;
  j.emaY = (int32_t)j.centerY << EMA_SHIFT;

  saveDeadzones();

  // Solid LED to confirm
  digitalWrite(j.pinLed, HIGH);
  delay(1000);
  digitalWrite(j.pinLed, LOW);

  Serial.print(F("  cX="));
  Serial.print(j.centerX);
  Serial.print(F(" cY="));
  Serial.print(j.centerY);
  Serial.print(F(" dzC="));
  Serial.print(j.dzCoarse);
  Serial.print(F(" dzF="));
  Serial.println(j.dzFine);
  Serial.println(F("CAL DONE"));
}

// ============================================================
// EEPROM
// ============================================================

void loadSettings() {
  if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VALUE) {
    saveDefaults();
  }

  controlMode = EEPROM.read(EEPROM_MODE_ADDR);
  if (controlMode > 1) controlMode = 0;

  for (uint8_t i = 0; i < NUM_KEYS; i++) {
    keys[i] = EEPROM.read(EEPROM_KEYS_ADDR + i);
    // Sanity: if key is null or non-printable, reset to default
    if (keys[i] < 0x20 || keys[i] > 0x7E) {
      keys[i] = pgm_read_byte(&DEFAULT_KEYS[i]);
    }
  }

  // Load dead zones (will be overwritten by auto-cal, but good fallback)
  for (uint8_t idx = 0; idx < 2; idx++) {
    Joystick &j = joy[idx];
    uint8_t base = idx * 2;
    j.dzCoarse = EEPROM.read(EEPROM_DZ_ADDR + base);
    j.dzFine = EEPROM.read(EEPROM_DZ_ADDR + base + 1);

    // Sanity check dead zones
    if (j.dzCoarse < 10 || j.dzCoarse > 250) j.dzCoarse = DEFAULT_DZ_COARSE;
    if (j.dzFine < 10 || j.dzFine > 250) j.dzFine = DEFAULT_DZ_FINE;

    // Load centers (2 bytes each, big-endian)
    uint8_t cBase = idx * 4;
    j.centerX = (EEPROM.read(EEPROM_CENTER_ADDR + cBase) << 8) |
                 EEPROM.read(EEPROM_CENTER_ADDR + cBase + 1);
    j.centerY = (EEPROM.read(EEPROM_CENTER_ADDR + cBase + 2) << 8) |
                 EEPROM.read(EEPROM_CENTER_ADDR + cBase + 3);

    if (j.centerX < 0 || j.centerX > 1023) j.centerX = DEFAULT_CENTER;
    if (j.centerY < 0 || j.centerY > 1023) j.centerY = DEFAULT_CENTER;
  }
}

void saveDefaults() {
  EEPROM.update(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
  EEPROM.update(EEPROM_MODE_ADDR, 0);
  for (uint8_t i = 0; i < NUM_KEYS; i++) {
    EEPROM.update(EEPROM_KEYS_ADDR + i, pgm_read_byte(&DEFAULT_KEYS[i]));
  }
  for (uint8_t i = 0; i < 4; i++) {
    EEPROM.update(EEPROM_DZ_ADDR + i,
                  (i % 2 == 0) ? DEFAULT_DZ_COARSE : DEFAULT_DZ_FINE);
  }
  for (uint8_t i = 0; i < 8; i++) {
    EEPROM.update(EEPROM_CENTER_ADDR + i,
                  (i % 2 == 0) ? highByte(DEFAULT_CENTER) : lowByte(DEFAULT_CENTER));
  }
}

void saveSettings() {
  EEPROM.update(EEPROM_MODE_ADDR, controlMode);
  for (uint8_t i = 0; i < NUM_KEYS; i++) {
    EEPROM.update(EEPROM_KEYS_ADDR + i, keys[i]);
  }
}

void saveDeadzones() {
  for (uint8_t idx = 0; idx < 2; idx++) {
    Joystick &j = joy[idx];
    uint8_t base = idx * 2;
    EEPROM.update(EEPROM_DZ_ADDR + base, j.dzCoarse);
    EEPROM.update(EEPROM_DZ_ADDR + base + 1, j.dzFine);

    uint8_t cBase = idx * 4;
    EEPROM.update(EEPROM_CENTER_ADDR + cBase, highByte(j.centerX));
    EEPROM.update(EEPROM_CENTER_ADDR + cBase + 1, lowByte(j.centerX));
    EEPROM.update(EEPROM_CENTER_ADDR + cBase + 2, highByte(j.centerY));
    EEPROM.update(EEPROM_CENTER_ADDR + cBase + 3, lowByte(j.centerY));
  }
}

// ============================================================
// Serial Command Interface
// ============================================================

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialPos > 0) {
        serialBuf[serialPos] = '\0';
        processCommand(serialBuf);
        serialPos = 0;
      }
    } else if (serialPos < sizeof(serialBuf) - 1) {
      serialBuf[serialPos++] = c;
    }
  }
}

static void toUpperStr(char *s) {
  for (; *s; s++) {
    if (*s >= 'a' && *s <= 'z') *s -= 32;
  }
}

void processCommand(char *raw) {
  // Trim spaces
  while (*raw == ' ') raw++;
  char *end = raw + strlen(raw) - 1;
  while (end > raw && *end == ' ') *end-- = '\0';

  char cmd[32];
  strncpy(cmd, raw, sizeof(cmd) - 1);
  cmd[sizeof(cmd) - 1] = '\0';
  toUpperStr(cmd);

  if (strcmp(cmd, "HELP") == 0) {
    printHelp();
  }
  else if (strcmp(cmd, "STATUS") == 0) {
    printStatus();
  }
  else if (strcmp(cmd, "KEYS") == 0) {
    printKeys();
  }
  else if (strcmp(cmd, "DZ") == 0 || strcmp(cmd, "DEADZONE") == 0) {
    printDeadzones();
  }
  else if (strcmp(cmd, "RAW") == 0) {
    printRaw();
  }
  else if (strcmp(cmd, "MODE PROPORTIONAL") == 0 || strcmp(cmd, "MODE 0") == 0) {
    controlMode = 0;
    saveSettings();
    Serial.println(F("Mode: PROPORTIONAL"));
  }
  else if (strcmp(cmd, "MODE DISCRETE") == 0 || strcmp(cmd, "MODE 1") == 0) {
    controlMode = 1;
    saveSettings();
    Serial.println(F("Mode: DISCRETE"));
  }
  else if (strncmp(cmd, "SET ", 4) == 0) {
    handleSetKey(cmd + 4);
  }
  else if (strncmp(cmd, "DZ ", 3) == 0) {
    handleSetDZ(cmd + 3);
  }
  else if (strcmp(cmd, "CAL 1") == 0 || strcmp(cmd, "CALIBRATE 1") == 0) {
    runManualCal(0);
  }
  else if (strcmp(cmd, "CAL 2") == 0 || strcmp(cmd, "CALIBRATE 2") == 0) {
    runManualCal(1);
  }
  else if (strcmp(cmd, "DEFAULTS") == 0) {
    saveDefaults();
    loadSettings();
    Serial.println(F("Factory reset."));
    printKeys();
    printDeadzones();
  }
  else if (strcmp(cmd, "STATE") == 0) {
    printState();
  }
  else {
    Serial.println(F("Unknown cmd. Type HELP."));
  }
}

void handleSetKey(const char *args) {
  const char *sp = strchr(args, ' ');
  if (!sp) { Serial.println(F("Usage: SET <SLOT> <KEY>")); return; }

  char slot[5];
  int len = sp - args;
  if (len > 4) len = 4;
  strncpy(slot, args, len);
  slot[len] = '\0';
  toUpperStr(slot);

  const char *kp = sp + 1;
  while (*kp == ' ') kp++;
  if (*kp == '\0' || *(kp + 1) != '\0') {
    Serial.println(F("Key must be single char."));
    return;
  }

  char newKey = *kp;
  if (newKey >= 'A' && newKey <= 'Z') newKey += 32;

  int idx = -1;
  if      (strcmp(slot, "J1LC") == 0) idx = K_J1_LEFT_COARSE;
  else if (strcmp(slot, "J1RC") == 0) idx = K_J1_RIGHT_COARSE;
  else if (strcmp(slot, "J1LF") == 0) idx = K_J1_LEFT_FINE;
  else if (strcmp(slot, "J1RF") == 0) idx = K_J1_RIGHT_FINE;
  else if (strcmp(slot, "J1B") == 0)  idx = K_J1_BTN;
  else if (strcmp(slot, "J2LC") == 0) idx = K_J2_LEFT_COARSE;
  else if (strcmp(slot, "J2RC") == 0) idx = K_J2_RIGHT_COARSE;
  else if (strcmp(slot, "J2LF") == 0) idx = K_J2_LEFT_FINE;
  else if (strcmp(slot, "J2RF") == 0) idx = K_J2_RIGHT_FINE;
  else if (strcmp(slot, "J2B") == 0)  idx = K_J2_BTN;

  if (idx < 0) {
    Serial.println(F("Slots: J1LC J1RC J1LF J1RF J1B J2LC J2RC J2LF J2RF J2B"));
    return;
  }

  keys[idx] = newKey;
  saveSettings();
  Serial.print(slot);
  Serial.print(F("='"));
  Serial.print(newKey);
  Serial.println('\'');
}

void handleSetDZ(const char *args) {
  int jn = 0, coarse = 0, fine = 0;
  if (sscanf(args, "%d %d %d", &jn, &coarse, &fine) != 3 || (jn != 1 && jn != 2)) {
    Serial.println(F("Usage: DZ <1|2> <coarse> <fine>"));
    return;
  }
  if (coarse < 10 || coarse > 250 || fine < 10 || fine > 250) {
    Serial.println(F("Range: 10-250"));
    return;
  }

  Joystick &j = joy[jn - 1];
  j.dzCoarse = coarse;
  j.dzFine = fine;
  saveDeadzones();
  Serial.print(F("J"));
  Serial.print(jn);
  Serial.print(F(" dzC="));
  Serial.print(coarse);
  Serial.print(F(" dzF="));
  Serial.println(fine);
}

// ============================================================
// Serial Output Helpers
// ============================================================

void printHelp() {
  Serial.println();
  Serial.println(F("=== CV Axle Controller v2.0 ==="));
  Serial.println(F("HELP STATUS KEYS STATE"));
  Serial.println(F("MODE PROPORTIONAL|DISCRETE"));
  Serial.println(F("SET <SLOT> <KEY>"));
  Serial.println(F("  J1LC J1RC J1LF J1RF J1B"));
  Serial.println(F("  J2LC J2RC J2LF J2RF J2B"));
  Serial.println(F("DZ         - Show dead zones"));
  Serial.println(F("DZ <1|2> C F - Set dead zones"));
  Serial.println(F("CAL <1|2>  - Auto-calibrate"));
  Serial.println(F("RAW        - Analog readings"));
  Serial.println(F("DEFAULTS   - Factory reset"));
  Serial.println(F("Hold btn 3s = auto-cal"));
}

void printStatus() {
  Serial.print(F("Mode: "));
  Serial.println(controlMode == 0 ? F("PROPORTIONAL") : F("DISCRETE"));
  Serial.print(F("Lock: "));
  Serial.println(locked ? F("LOCKED") : F("ACTIVE"));
  printDeadzones();
}

void printDeadzones() {
  Serial.println(F("\nDead Zones:"));
  for (uint8_t idx = 0; idx < 2; idx++) {
    Joystick &j = joy[idx];
    Serial.print(F("  J"));
    Serial.print(idx + 1);
    Serial.print(F(": dzC="));
    Serial.print(j.dzCoarse);
    Serial.print(F(" dzF="));
    Serial.print(j.dzFine);
    Serial.print(F(" cX="));
    Serial.print(j.centerX);
    Serial.print(F(" cY="));
    Serial.println(j.centerY);
  }
}

void printKeys() {
  Serial.println(F("\nKeys:"));
  Serial.print(F("  J1: L='"));Serial.print(keys[0]);
  Serial.print(F("' R='"));Serial.print(keys[1]);
  Serial.print(F("' LF='"));Serial.print(keys[2]);
  Serial.print(F("' RF='"));Serial.print(keys[3]);
  Serial.print(F("' B='"));Serial.print(keys[4]);
  Serial.println('\'');
  Serial.print(F("  J2: L='"));Serial.print(keys[5]);
  Serial.print(F("' R='"));Serial.print(keys[6]);
  Serial.print(F("' LF='"));Serial.print(keys[7]);
  Serial.print(F("' RF='"));Serial.print(keys[8]);
  Serial.print(F("' B='"));Serial.print(keys[9]);
  Serial.println('\'');
}

void printRaw() {
  Serial.println(F("Raw (10 readings):"));
  for (uint8_t i = 0; i < 10; i++) {
    Serial.print(F("  J1 X="));
    Serial.print(analogRead(JOY1_X_PIN));
    Serial.print(F(" Y="));
    Serial.print(analogRead(JOY1_Y_PIN));
    Serial.print(F("  J2 X="));
    Serial.print(analogRead(JOY2_X_PIN));
    Serial.print(F(" Y="));
    Serial.println(analogRead(JOY2_Y_PIN));
    delay(100);
  }
}

void printState() {
  const char *stateNames[] = {"IDLE", "COARSE", "FINE", "SUPPRESSED"};
  Serial.println(F("\nState Machine:"));
  for (uint8_t idx = 0; idx < 2; idx++) {
    Joystick &j = joy[idx];
    int16_t filtX = j.emaX >> EMA_SHIFT;
    int16_t filtY = j.emaY >> EMA_SHIFT;
    Serial.print(F("  J"));
    Serial.print(idx + 1);
    Serial.print(F(": "));
    Serial.print(stateNames[j.state]);
    Serial.print(F("  filtX="));
    Serial.print(filtX);
    Serial.print(F("("));
    Serial.print(filtX - j.centerX);
    Serial.print(F(") filtY="));
    Serial.print(filtY);
    Serial.print(F("("));
    Serial.print(filtY - j.centerY);
    Serial.print(F(") fineEng="));
    Serial.println(j.fineEngaged ? F("Y") : F("N"));
  }
}
