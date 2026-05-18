/*
 * CV Axle Controller - Production Firmware v2.0 (Clean Rewrite)
 * Arduino Pro Micro (ATmega32U4)
 *
 * USB HID keyboard emulation for CV axle measurement line control.
 * Two joysticks: X-axis = movement, button = toggle coarse/fine mode.
 * Twist barrel disabled (unreliable for bidirectional fine control).
 *
 * Architecture: per-joystick with button-toggled fine mode.
 *
 * Input pipeline:
 *   1. analogRead() X-axis raw sample
 *   2. Exponential moving average (EMA) low-pass filter
 *   3. Deflection from calibrated center
 *   4. Dead zone check
 *   5. Keystroke output (coarse or fine keys based on mode)
 *
 * Controls per joystick:
 *   Push left/right - move measurement line (coarse or fine step)
 *   Short button press - toggle between coarse and fine mode
 *   Hold button 3s - auto-calibrate dead zone
 *   LED on = fine mode active
 *
 * Hardware:
 *   Joystick 1: VRx=A0, VRy=A1, SW=D2
 *   Joystick 2: VRx=A2, VRy=A3, SW=D3
 *   Lock switch: D4 (toggle to GND)
 *   Lock LED:    D5 (red/locked), D9 (green/unlocked)
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
#define LED_LOCK_RED   5
#define LED_LOCK_GRN   9
#define LED_FINE1    6
#define LED_FINE2    7
#define RECAL_BTN    8

// 10mm RGB LED brightness (0-255). Full brightness is harsh; ~60 is visible but soft.
#define RGB_BRIGHTNESS  60

// ============================================================
// EEPROM Layout
// ============================================================

#define EEPROM_MAGIC_ADDR     0     // 1 byte: magic sentinel
#define EEPROM_MODE_ADDR      1     // 1 byte: control mode
#define EEPROM_KEYS_ADDR      2     // 13 bytes: key mappings
#define EEPROM_DZ_ADDR        15    // 4 bytes: dead zones (J1c, J1f, J2c, J2f)
#define EEPROM_CENTER_ADDR    19    // 8 bytes: centers (J1x, J1y, J2x, J2y) x 2 bytes each

#define EEPROM_MAGIC_VALUE    0xB5  // Bump this to force factory reset on flash

#define NUM_KEYS  13

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
  K_J2_BTN,
  K_LOCK,       // Sent on lock state change (key = locked, release = unlocked)
  K_J1_FINE_LED, // Sent when J1 fine mode toggles on (released when off)
  K_J2_FINE_LED  // Sent when J2 fine mode toggles on (released when off)
};

static const char DEFAULT_KEYS[NUM_KEYS] PROGMEM = {
  'd', 'a',   // J1 coarse left/right (swapped — joystick mounted reversed)
  'e', 'q',   // J1 fine left/right (swapped to match)
  'f',        // J1 button (fine toggle)
  'j', 'l',   // J2 coarse left/right
  'u', 'o',   // J2 fine left/right
  ';',        // J2 button (fine toggle)
  '[',        // Lock state: press=locked, release=unlocked
  '1',        // J1 fine LED: press=fine on, release=fine off
  '2'         // J2 fine LED: press=fine on, release=fine off
};

char keys[NUM_KEYS];

// ============================================================
// Tuning Constants
// ============================================================

// Dead zone defaults (overwritten by auto-cal on every boot)
#define DEFAULT_DZ_COARSE   150
#define DEFAULT_CENTER      512

// Timing
#define COARSE_REPEAT_FAST_MS   30    // Max speed at full deflection
#define COARSE_REPEAT_SLOW_MS   200   // Min speed at dead zone edge
#define FINE_REPEAT_MS          200   // Fixed repeat rate in fine mode
#define BUTTON_DEBOUNCE_MS      200   // Button debounce
#define CAL_SAMPLE_MS           2000  // Duration of noise sampling
#define CAL_SETTLE_MS           3000  // Pre-cal settle time
#define DZ_COARSE_MARGIN        25    // Added to coarse noise peak
#define DZ_COARSE_FLOOR         100   // Minimum coarse dead zone

// EMA filter: alpha = 1/EMA_DIVISOR. Higher = more smoothing.
// Using fixed-point x256 for precision without floats.
#define EMA_SHIFT  4   // 2^4 = 16, so alpha ~= 1/16 = 0.0625

// ============================================================
// Joystick State Machine
// ============================================================

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
  int16_t dzCoarse;

  // EMA filter state (fixed-point x16 via shift)
  int32_t emaX;
  bool emaInitialized;

  // Timing
  unsigned long lastKeystroke;
  unsigned long lastBtnPress;

  // Fine mode: toggled by short button press
  bool fineMode;

  // Button state
  bool btnWasHeld;
};

// Two joystick instances
Joystick joy[2];

// ============================================================
// Global State
// ============================================================

uint8_t controlMode = 0;   // 0 = proportional, 1 = discrete toggle
bool locked = false;
bool prevLocked = false;   // Track lock transitions for keystroke reporting

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
void handleButton(uint8_t idx);
void checkRecalButton();
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
  pinMode(RECAL_BTN, INPUT_PULLUP);
  pinMode(LED_LOCK_RED, OUTPUT);
  pinMode(LED_LOCK_GRN, OUTPUT);
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

  // Lock LED shows current switch state after cal
  locked = (digitalRead(LOCK_SWITCH) == LOW);
  prevLocked = locked;
  setLockRed(locked);
  setLockGrn(!locked);

  // Announce initial state to host — only if locked (host assumes unlocked + coarse by default)
  delay(200);  // Brief pause for USB enumeration to settle
  if (locked) {
    sendKey(keys[K_LOCK]);
  }

  Serial.println(F("AXLE_CONTROLLER_READY v2.2"));
  printHelp();
}

// ============================================================
// Main Loop
// ============================================================

void loop() {
  handleSerial();

  // Lock switch — send keystroke on transitions only
  locked = (digitalRead(LOCK_SWITCH) == LOW);
  setLockRed(locked);
  setLockGrn(!locked);
  if (locked != prevLocked) {
    prevLocked = locked;
    sendKey(keys[K_LOCK]);  // Tap lock key — host toggles lock state
  }

  // Check recal button (works even when locked)
  checkRecalButton();

  if (locked) {
    digitalWrite(LED_FINE1, LOW);
    digitalWrite(LED_FINE2, LOW);
    joy[0].fineMode = false;
    joy[1].fineMode = false;
    return;
  }

  // Handle joystick buttons: short press = toggle fine/coarse
  handleButton(0);
  handleButton(1);

  // Process each joystick
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
  j.dzCoarse = DEFAULT_DZ_COARSE;
  j.emaX = 0;
  j.emaInitialized = false;
  j.lastKeystroke = 0;
  j.lastBtnPress = 0;
  j.fineMode = false;
  j.btnWasHeld = false;
}

// ============================================================
// Input Pipeline: Read + Filter + Deflection
// ============================================================

// Returns filtered analog value (0-1023 range)
int16_t readFiltered(uint8_t idx) {
  Joystick &j = joy[idx];
  int16_t raw = analogRead(j.pinX);

  if (!j.emaInitialized) {
    j.emaX = (int32_t)raw << EMA_SHIFT;
    j.emaInitialized = true;
  } else {
    j.emaX = j.emaX - (j.emaX >> EMA_SHIFT) + raw;
  }

  return (int16_t)(j.emaX >> EMA_SHIFT);
}

// ============================================================
// State Machine: Process One Joystick
// ============================================================

void processJoystick(uint8_t idx) {
  Joystick &j = joy[idx];
  unsigned long now = millis();

  // --- Read and filter X axis only (Y/twist axis disabled) ---
  int16_t filtX = readFiltered(idx);

  // --- Compute deflection from calibrated center ---
  int16_t deflectX = filtX - j.centerX;
  int16_t absDeflectX = abs(deflectX);

  // --- Movement: send keystrokes based on X deflection ---
  if (absDeflectX > j.dzCoarse) {
    // Pick keys based on fine mode toggle
    int16_t magnitude = absDeflectX;
    if (magnitude > 512) magnitude = 512;

    int repeatMs;
    if (j.fineMode) {
      repeatMs = FINE_REPEAT_MS;  // Fixed rate in fine mode
    } else {
      repeatMs = map(magnitude, j.dzCoarse, 512, COARSE_REPEAT_SLOW_MS, COARSE_REPEAT_FAST_MS);
      if (repeatMs < COARSE_REPEAT_FAST_MS) repeatMs = COARSE_REPEAT_FAST_MS;
      if (repeatMs > COARSE_REPEAT_SLOW_MS) repeatMs = COARSE_REPEAT_SLOW_MS;
    }

    if (now - j.lastKeystroke >= (unsigned long)repeatMs) {
      char key;
      if (j.fineMode) {
        key = (deflectX > 0) ? keys[j.kRightFine] : keys[j.kLeftFine];
      } else {
        key = (deflectX > 0) ? keys[j.kRightCoarse] : keys[j.kLeftCoarse];
      }
      sendKey(key);
      j.lastKeystroke = now;
    }
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

// Helper: set 10mm RGB LED using PWM for dimming
void setLockRed(bool on)   { analogWrite(LED_LOCK_RED, on ? RGB_BRIGHTNESS : 0); }
void setLockGrn(bool on)   { analogWrite(LED_LOCK_GRN, on ? RGB_BRIGHTNESS : 0); }

void allLedsOff() {
  digitalWrite(LED_FINE1, LOW);
  digitalWrite(LED_FINE2, LOW);
  setLockRed(false);
  setLockGrn(false);
}

void runStartupCal() {
  Serial.println(F("Auto-cal: hands off joysticks..."));

  // Alternating blue LEDs during settle (3s, 600ms per side)
  // 10mm LED stays off — calm, professional heartbeat.
  bool leftOn = true;
  unsigned long settleStart = millis();
  while (millis() - settleStart < CAL_SETTLE_MS) {
    digitalWrite(LED_FINE1, leftOn ? HIGH : LOW);
    digitalWrite(LED_FINE2, leftOn ? LOW : HIGH);
    leftOn = !leftOn;
    delay(600);
  }

  // Sample both joysticks (X axis only, twist/Y axis disabled)
  // Blues keep alternating during sampling (same rhythm)
  for (uint8_t idx = 0; idx < 2; idx++) {
    Joystick &j = joy[idx];
    long sumX = 0;
    int16_t minX = 1023, maxX = 0;
    const int samples = 200;  // 2 seconds at 10ms interval

    for (int i = 0; i < samples; i++) {
      int16_t rx = analogRead(j.pinX);
      sumX += rx;
      if (rx < minX) minX = rx;
      if (rx > maxX) maxX = rx;

      // Alternate blues every 60 samples (~600ms)
      if (i % 60 == 0) {
        leftOn = !leftOn;
        digitalWrite(LED_FINE1, leftOn ? HIGH : LOW);
        digitalWrite(LED_FINE2, leftOn ? LOW : HIGH);
      }
      delay(10);
    }

    j.centerX = sumX / samples;

    int16_t noiseX = max(abs(maxX - j.centerX), abs(minX - j.centerX));
    j.dzCoarse = max((int16_t)(noiseX + DZ_COARSE_MARGIN), (int16_t)DZ_COARSE_FLOOR);

    // Initialize EMA with center
    j.emaX = (int32_t)j.centerX << EMA_SHIFT;
    j.emaInitialized = true;

    Serial.print(F("  J"));
    Serial.print(idx + 1);
    Serial.print(F(": cX="));
    Serial.print(j.centerX);
    Serial.print(F(" noiseX="));
    Serial.print(noiseX);
    Serial.print(F(" dzC="));
    Serial.println(j.dzCoarse);
  }

  saveDeadzones();

  // Cal complete: blues off, brief pause, then lock LED lights up
  allLedsOff();
  delay(200);

  Serial.println(F("Cal done."));
}

// ============================================================
// Calibration: Manual (hold button 3s)
// ============================================================

void handleButton(uint8_t idx) {
  Joystick &j = joy[idx];
  bool pressed = (digitalRead(j.pinBtn) == LOW);
  unsigned long now = millis();

  if (!pressed && j.btnWasHeld && (now - j.lastBtnPress >= BUTTON_DEBOUNCE_MS)) {
    // Button just released — toggle fine/coarse
    j.lastBtnPress = now;
    j.fineMode = !j.fineMode;
    digitalWrite(j.pinLed, j.fineMode ? HIGH : LOW);

    // Tell host: send fine-LED key when fine on, button key when fine off
    if (j.fineMode) {
      sendKey(keys[(idx == 0) ? K_J1_FINE_LED : K_J2_FINE_LED]);
    } else {
      sendKey(keys[j.kBtn]);
    }
  }

  j.btnWasHeld = pressed;
}

void checkRecalButton() {
  static bool recalWasPressed = false;
  bool pressed = (digitalRead(RECAL_BTN) == LOW);

  if (pressed && !recalWasPressed) {
    // Button just pressed — run full recal (same as startup)
    Serial.println(F("RECAL triggered"));
    runStartupCal();
    // Wait for button release
    while (digitalRead(RECAL_BTN) == LOW) delay(10);
  }

  recalWasPressed = pressed;
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

  long sumX = 0;
  int16_t minX = 1023, maxX = 0;
  int samples = 0;

  unsigned long start = millis();
  while (millis() - start < CAL_SAMPLE_MS) {
    int16_t rx = analogRead(j.pinX);
    sumX += rx;
    samples++;
    if (rx < minX) minX = rx;
    if (rx > maxX) maxX = rx;
    digitalWrite(j.pinLed, (millis() / 80) % 2 ? HIGH : LOW);
    delay(5);
  }

  j.centerX = sumX / samples;

  int16_t noiseX = max(abs(maxX - j.centerX), abs(minX - j.centerX));
  j.dzCoarse = max((int16_t)(noiseX + DZ_COARSE_MARGIN), (int16_t)DZ_COARSE_FLOOR);

  // Re-seed EMA with new center
  j.emaX = (int32_t)j.centerX << EMA_SHIFT;

  saveDeadzones();

  // Solid LED to confirm
  digitalWrite(j.pinLed, HIGH);
  delay(1000);
  digitalWrite(j.pinLed, LOW);

  Serial.print(F("  cX="));
  Serial.print(j.centerX);
  Serial.print(F(" dzC="));
  Serial.println(j.dzCoarse);
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
    j.dzCoarse = EEPROM.read(EEPROM_DZ_ADDR + idx);
    if (j.dzCoarse < 10 || j.dzCoarse > 250) j.dzCoarse = DEFAULT_DZ_COARSE;

    // Load center X (2 bytes, big-endian)
    uint8_t cBase = idx * 2;
    j.centerX = (EEPROM.read(EEPROM_CENTER_ADDR + cBase) << 8) |
                 EEPROM.read(EEPROM_CENTER_ADDR + cBase + 1);
    if (j.centerX < 0 || j.centerX > 1023) j.centerX = DEFAULT_CENTER;
  }
}

void saveDefaults() {
  EEPROM.update(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
  EEPROM.update(EEPROM_MODE_ADDR, 0);
  for (uint8_t i = 0; i < NUM_KEYS; i++) {
    EEPROM.update(EEPROM_KEYS_ADDR + i, pgm_read_byte(&DEFAULT_KEYS[i]));
  }
  for (uint8_t i = 0; i < 2; i++) {
    EEPROM.update(EEPROM_DZ_ADDR + i, DEFAULT_DZ_COARSE);
  }
  for (uint8_t i = 0; i < 4; i++) {
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
    EEPROM.update(EEPROM_DZ_ADDR + idx, j.dzCoarse);

    uint8_t cBase = idx * 2;
    EEPROM.update(EEPROM_CENTER_ADDR + cBase, highByte(j.centerX));
    EEPROM.update(EEPROM_CENTER_ADDR + cBase + 1, lowByte(j.centerX));
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
  else if (strcmp(cmd, "CAL") == 0 || strcmp(cmd, "CALIBRATE") == 0) {
    runStartupCal();
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
  else if (strcmp(slot, "LOCK") == 0) idx = K_LOCK;
  else if (strcmp(slot, "F1LED") == 0) idx = K_J1_FINE_LED;
  else if (strcmp(slot, "F2LED") == 0) idx = K_J2_FINE_LED;

  if (idx < 0) {
    Serial.println(F("Slots: J1LC J1RC J1LF J1RF J1B J2LC J2RC J2LF J2RF J2B LOCK F1LED F2LED"));
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
  int jn = 0, coarse = 0;
  if (sscanf(args, "%d %d", &jn, &coarse) != 2 || (jn != 1 && jn != 2)) {
    Serial.println(F("Usage: DZ <1|2> <value>"));
    return;
  }
  if (coarse < 10 || coarse > 250) {
    Serial.println(F("Range: 10-250"));
    return;
  }

  Joystick &j = joy[jn - 1];
  j.dzCoarse = coarse;
  saveDeadzones();
  Serial.print(F("J"));
  Serial.print(jn);
  Serial.print(F(" dzC="));
  Serial.println(coarse);
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
  Serial.println(F("DZ           - Show dead zones"));
  Serial.println(F("DZ <1|2> <V> - Set dead zone"));
  Serial.println(F("CAL        - Calibrate both"));
  Serial.println(F("CAL <1|2>  - Calibrate one"));
  Serial.println(F("RAW        - Analog readings"));
  Serial.println(F("DEFAULTS   - Factory reset"));
  Serial.println(F("Recal btn (D8) = calibrate both"));
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
    Serial.print(F(" cX="));
    Serial.print(j.centerX);
    Serial.print(F(" mode="));
    Serial.println(j.fineMode ? F("FINE") : F("COARSE"));
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
  Serial.println(F("\nState:"));
  for (uint8_t idx = 0; idx < 2; idx++) {
    Joystick &j = joy[idx];
    int16_t filtX = j.emaX >> EMA_SHIFT;
    Serial.print(F("  J"));
    Serial.print(idx + 1);
    Serial.print(F(": mode="));
    Serial.print(j.fineMode ? F("FINE") : F("COARSE"));
    Serial.print(F("  filtX="));
    Serial.print(filtX);
    Serial.print(F("("));
    Serial.print(filtX - j.centerX);
    Serial.println(F(")"));
  }
}
