/*
 * CV Axle Controller - Production Firmware (Arduino Pro Micro)
 *
 * Native USB HID keyboard emulation — no bridge script needed.
 * The computer sees this as a standard keyboard.
 *
 * Supports two control modes (user-configurable, stored in EEPROM):
 *   1. Proportional mode (default): deflection distance = keystroke speed
 *   2. Discrete toggle mode: button toggles between coarse/fine
 *
 * Joystick input:
 *   - X-axis: coarse left/right movement
 *   - Twist/Y-axis: fine left/right movement
 *   - When fine axis is active, coarse axis is locked (twist-locks-X)
 *
 * Wiring (Pro Micro pinout):
 *   Joystick 1: VRx→A0, VRy→A1 (or twist), SW→D2 (INPUT_PULLUP)
 *   Joystick 2: VRx→A2, VRy→A3 (or twist), SW→D3 (INPUT_PULLUP)
 *   Lock switch: D4 (INPUT_PULLUP, toggle switch to GND)
 *   Lock LED (green): D5
 *   Fine LED 1 (blue): D6
 *   Fine LED 2 (blue): D7
 *
 * Default key mappings (configurable via serial commands):
 *   Joy1 coarse left/right: A / D
 *   Joy1 fine left/right:   Q / E
 *   Joy2 coarse left/right: J / L
 *   Joy2 fine left/right:   U / O
 *   Joy1 button:            1
 *   Joy2 button:            2
 *
 * Dead zone auto-calibration:
 *   Hold joystick button for 3 seconds to calibrate that joystick.
 *   LED blinks fast, then samples resting noise for 2 seconds.
 *   Sets dead zone to noise peak + margin, saves to EEPROM.
 *   Can also calibrate via serial: CAL 1 or CAL 2
 */

#include <Keyboard.h>
#include <EEPROM.h>

// === Pin Definitions ===
// Joystick 1
const int JOY1_X = A0;
const int JOY1_Y = A1;      // Twist barrel on JH-D400X-R4, Y-axis on thumbstick
const int JOY1_BTN = 2;

// Joystick 2
const int JOY2_X = A2;
const int JOY2_Y = A3;
const int JOY2_BTN = 3;

// Lock switch & LEDs
const int LOCK_SWITCH = 4;
const int LED_LOCK = 5;
const int LED_FINE1 = 6;
const int LED_FINE2 = 7;

// === EEPROM Addresses ===
const int EEPROM_MAGIC = 0;           // Magic byte to detect first run
const int EEPROM_MODE = 1;            // 0 = proportional, 1 = discrete toggle
const int EEPROM_KEYS_START = 2;      // 10 bytes for key mappings
// Key mapping order in EEPROM (starting at EEPROM_KEYS_START):
//   0: JOY1_LEFT_COARSE
//   1: JOY1_RIGHT_COARSE
//   2: JOY1_LEFT_FINE
//   3: JOY1_RIGHT_FINE
//   4: JOY1_BTN
//   5: JOY2_LEFT_COARSE
//   6: JOY2_RIGHT_COARSE
//   7: JOY2_LEFT_FINE
//   8: JOY2_RIGHT_FINE
//   9: JOY2_BTN
const int EEPROM_DEADZONE_START = 12; // 4 bytes: J1 coarse, J1 fine, J2 coarse, J2 fine
const int EEPROM_CENTER_START = 16;   // 4 bytes: J1 X center (2 bytes), J2 X center (2 bytes)

const byte EEPROM_MAGIC_VALUE = 0xAD; // Bumped from 0xAC to force re-init with new fields
const int NUM_KEYS = 10;

// === Key Mapping Indices ===
enum KeyIndex {
  J1_LEFT_COARSE = 0,
  J1_RIGHT_COARSE,
  J1_LEFT_FINE,
  J1_RIGHT_FINE,
  J1_BTN,
  J2_LEFT_COARSE,
  J2_RIGHT_COARSE,
  J2_LEFT_FINE,
  J2_RIGHT_FINE,
  J2_BTN
};

// Default key mappings
const char DEFAULT_KEYS[NUM_KEYS] = {
  'a', 'd',   // Joy1 coarse left/right
  'q', 'e',   // Joy1 fine left/right
  'f',        // Joy1 button (toggles fine/coarse in web app)
  'j', 'l',   // Joy2 coarse left/right
  'u', 'o',   // Joy2 fine left/right
  '2'         // Joy2 button
};

// Active key mappings (loaded from EEPROM)
char keys[NUM_KEYS];

// === Tuning Parameters ===
// Default dead zones (used on first run, then EEPROM values take over)
const int DEFAULT_DEADZONE_COARSE = 80;
const int DEFAULT_DEADZONE_FINE = 60;
const int DEFAULT_CENTER = 512;

// Active dead zone values (loaded from EEPROM)
int deadzone1Coarse = DEFAULT_DEADZONE_COARSE;
int deadzone1Fine = DEFAULT_DEADZONE_FINE;
int deadzone2Coarse = DEFAULT_DEADZONE_COARSE;
int deadzone2Fine = DEFAULT_DEADZONE_FINE;
int center1 = DEFAULT_CENTER;
int center2 = DEFAULT_CENTER;

// Repeat rate control (milliseconds between keystrokes)
const int REPEAT_FAST = 30;
const int REPEAT_SLOW = 200;
const int REPEAT_FINE = 150;

// Fine axis engagement threshold
const int FINE_ACTIVE_THRESHOLD = 40;

// Dead zone calibration margin (added to measured noise peak)
const int DEADZONE_MARGIN = 15;

// === State ===
unsigned long lastRepeat1 = 0;
unsigned long lastRepeat2 = 0;
bool locked = false;
byte controlMode = 0;           // 0 = proportional, 1 = discrete toggle
bool discreteFineMode1 = false; // For discrete toggle mode
bool discreteFineMode2 = false;

// Button debounce
unsigned long lastBtn1Press = 0;
unsigned long lastBtn2Press = 0;
const unsigned long DEBOUNCE_MS = 200;

// Calibration state
const unsigned long CAL_HOLD_MS = 3000;   // Hold button 3 sec to start
const unsigned long CAL_SAMPLE_MS = 2000; // Sample noise for 2 sec
unsigned long btn1HoldStart = 0;
unsigned long btn2HoldStart = 0;
bool btn1WasPressed = false;
bool btn2WasPressed = false;

// Serial config command buffer (fixed size, no heap allocation)
char serialBuffer[32];
byte serialPos = 0;

// === Setup ===
void setup() {
  Serial.begin(115200);

  // Joystick buttons
  pinMode(JOY1_BTN, INPUT_PULLUP);
  pinMode(JOY2_BTN, INPUT_PULLUP);

  // Lock switch
  pinMode(LOCK_SWITCH, INPUT_PULLUP);

  // LEDs
  pinMode(LED_LOCK, OUTPUT);
  pinMode(LED_FINE1, OUTPUT);
  pinMode(LED_FINE2, OUTPUT);

  // Load settings from EEPROM
  loadSettings();

  // Initialize USB keyboard
  Keyboard.begin();

  // Startup indicator - blink lock LED 3 times
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_LOCK, HIGH);
    delay(100);
    digitalWrite(LED_LOCK, LOW);
    delay(100);
  }

  Serial.println(F("AXLE_CONTROLLER_READY"));
  printHelp();
}

// === Main Loop ===
void loop() {
  // Check for serial configuration commands
  handleSerial();

  // Check lock switch
  locked = (digitalRead(LOCK_SWITCH) == LOW);
  digitalWrite(LED_LOCK, locked ? LOW : HIGH);

  if (locked) {
    digitalWrite(LED_FINE1, LOW);
    digitalWrite(LED_FINE2, LOW);
    return;
  }

  // Check for calibration hold (button held 3 seconds)
  checkCalibrationHold(JOY1_BTN, btn1WasPressed, btn1HoldStart,
                       JOY1_X, JOY1_Y, LED_FINE1, 1);
  checkCalibrationHold(JOY2_BTN, btn2WasPressed, btn2HoldStart,
                       JOY2_X, JOY2_Y, LED_FINE2, 2);

  // Process each joystick
  processJoystick(
    JOY1_X, JOY1_Y, JOY1_BTN, LED_FINE1,
    lastRepeat1, lastBtn1Press, discreteFineMode1,
    J1_LEFT_COARSE, J1_RIGHT_COARSE, J1_LEFT_FINE, J1_RIGHT_FINE, J1_BTN,
    deadzone1Coarse, deadzone1Fine, center1
  );

  processJoystick(
    JOY2_X, JOY2_Y, JOY2_BTN, LED_FINE2,
    lastRepeat2, lastBtn2Press, discreteFineMode2,
    J2_LEFT_COARSE, J2_RIGHT_COARSE, J2_LEFT_FINE, J2_RIGHT_FINE, J2_BTN,
    deadzone2Coarse, deadzone2Fine, center2
  );
}

// === Joystick Processing ===
void processJoystick(
  int pinX, int pinY, int pinBtn, int ledFine,
  unsigned long &lastRepeat, unsigned long &lastBtnPress, bool &discreteFine,
  int keyLeftCoarse, int keyRightCoarse, int keyLeftFine, int keyRightFine, int keyBtn,
  int dzCoarse, int dzFine, int centerVal
) {
  int rawX = analogRead(pinX);
  int rawY = analogRead(pinY);
  bool btnPressed = (digitalRead(pinBtn) == LOW);

  int deflectX = rawX - centerVal;
  int deflectY = rawY - centerVal;

  // Handle button press
  unsigned long now = millis();
  if (btnPressed && (now - lastBtnPress >= DEBOUNCE_MS)) {
    lastBtnPress = now;

    if (controlMode == 1) {
      // Discrete toggle mode: button switches coarse/fine
      discreteFine = !discreteFine;
    } else {
      // Proportional mode: button sends mapped keystroke
      Keyboard.press(keys[keyBtn]);
      delay(50);
      Keyboard.release(keys[keyBtn]);
    }
  }

  // Determine fine mode engagement
  bool fineActive;
  if (controlMode == 1) {
    // Discrete toggle: use button state
    fineActive = discreteFine;
  } else {
    // Proportional: fine axis engagement (twist-locks-X)
    fineActive = (abs(deflectY) > FINE_ACTIVE_THRESHOLD);
  }
  digitalWrite(ledFine, fineActive ? HIGH : LOW);

  // Select axis, deadzone, and repeat rate
  int deflection;
  int deadzone;
  int repeatRate;
  int keyLeft, keyRight;

  if (fineActive) {
    if (controlMode == 1) {
      // Discrete mode fine: still use X-axis but with fine deadzone and rate
      deflection = deflectX;
    } else {
      // Proportional mode fine: use Y/twist axis
      deflection = deflectY;
    }
    deadzone = dzFine;
    repeatRate = REPEAT_FINE;
    keyLeft = keyLeftFine;
    keyRight = keyRightFine;
  } else {
    deflection = deflectX;
    deadzone = dzCoarse;
    keyLeft = keyLeftCoarse;
    keyRight = keyRightCoarse;

    // Proportional speed mapping
    int magnitude = abs(deflection);
    if (magnitude > deadzone) {
      repeatRate = map(magnitude, deadzone, 512, REPEAT_SLOW, REPEAT_FAST);
      repeatRate = constrain(repeatRate, REPEAT_FAST, REPEAT_SLOW);
    } else {
      return;
    }
  }

  // Check deadzone
  if (abs(deflection) <= deadzone) {
    return;
  }

  // Determine direction and send keystroke
  char key = (deflection > 0) ? keys[keyRight] : keys[keyLeft];

  now = millis();
  if (now - lastRepeat >= (unsigned long)repeatRate) {
    lastRepeat = now;
    Keyboard.press(key);
    delay(10);
    Keyboard.release(key);
  }
}

// === Dead Zone Auto-Calibration ===
void checkCalibrationHold(int pinBtn, bool &wasPressed, unsigned long &holdStart,
                          int pinX, int pinY, int ledFine, int joyNum) {
  bool pressed = (digitalRead(pinBtn) == LOW);

  if (pressed && !wasPressed) {
    // Button just pressed, start timing
    holdStart = millis();
  } else if (pressed && wasPressed) {
    // Button still held, check duration
    if (millis() - holdStart >= CAL_HOLD_MS) {
      // Held long enough — run calibration
      runCalibration(pinX, pinY, ledFine, joyNum);
      holdStart = 0;
      // Wait for button release to avoid triggering normal button action
      while (digitalRead(pinBtn) == LOW) delay(10);
    }
  }

  wasPressed = pressed;
}

void runCalibration(int pinX, int pinY, int ledFine, int joyNum) {
  Serial.print(F("CAL Joy"));
  Serial.println(joyNum);
  Serial.println(F("  Don't touch..."));

  // Fast blink LED to indicate calibration mode
  for (int i = 0; i < 6; i++) {
    digitalWrite(ledFine, HIGH);
    delay(100);
    digitalWrite(ledFine, LOW);
    delay(100);
  }

  // Wait a moment for the operator to release the joystick
  delay(500);

  // Sample the resting position and noise for CAL_SAMPLE_MS
  int minX = 1023, maxX = 0;
  int minY = 1023, maxY = 0;
  long sumX = 0, sumY = 0;
  int samples = 0;

  unsigned long start = millis();
  while (millis() - start < CAL_SAMPLE_MS) {
    int x = analogRead(pinX);
    int y = analogRead(pinY);

    sumX += x;
    sumY += y;
    samples++;

    if (x < minX) minX = x;
    if (x > maxX) maxX = x;
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;

    // Blink LED during sampling
    digitalWrite(ledFine, (millis() / 80) % 2 ? HIGH : LOW);
    delay(5);
  }

  // Calculate center point (average of all samples)
  int centerX = sumX / samples;
  int centerY = sumY / samples;

  // Calculate noise range (max deflection from center at rest)
  int noiseX = max(abs(maxX - centerX), abs(minX - centerX));
  int noiseY = max(abs(maxY - centerY), abs(minY - centerY));

  // Dead zone = noise peak + margin
  int dzCoarse = noiseX + DEADZONE_MARGIN;
  int dzFine = noiseY + DEADZONE_MARGIN;

  // Minimum dead zones (safety floor)
  if (dzCoarse < 20) dzCoarse = 20;
  if (dzFine < 20) dzFine = 20;

  // Store results
  if (joyNum == 1) {
    center1 = centerX;
    deadzone1Coarse = dzCoarse;
    deadzone1Fine = dzFine;
  } else {
    center2 = centerX;
    deadzone2Coarse = dzCoarse;
    deadzone2Fine = dzFine;
  }

  saveDeadzones();

  // Solid LED for 1 second to confirm
  digitalWrite(ledFine, HIGH);
  delay(1000);
  digitalWrite(ledFine, LOW);

  Serial.print(F("  Center="));
  Serial.print(centerX);
  Serial.print(F(" NoiseX="));
  Serial.print(noiseX);
  Serial.print(F(" NoiseY="));
  Serial.println(noiseY);
  Serial.print(F("  DZ c="));
  Serial.print(dzCoarse);
  Serial.print(F(" f="));
  Serial.println(dzFine);
  Serial.println(F("CAL DONE"));
}

// === EEPROM Settings ===
void loadSettings() {
  if (EEPROM.read(EEPROM_MAGIC) != EEPROM_MAGIC_VALUE) {
    // First run or magic changed — write defaults
    saveDefaults();
  }

  controlMode = EEPROM.read(EEPROM_MODE);
  for (int i = 0; i < NUM_KEYS; i++) {
    keys[i] = EEPROM.read(EEPROM_KEYS_START + i);
  }

  // Load dead zones
  deadzone1Coarse = EEPROM.read(EEPROM_DEADZONE_START);
  deadzone1Fine = EEPROM.read(EEPROM_DEADZONE_START + 1);
  deadzone2Coarse = EEPROM.read(EEPROM_DEADZONE_START + 2);
  deadzone2Fine = EEPROM.read(EEPROM_DEADZONE_START + 3);

  // Load centers (2 bytes each, high byte then low byte)
  center1 = (EEPROM.read(EEPROM_CENTER_START) << 8) | EEPROM.read(EEPROM_CENTER_START + 1);
  center2 = (EEPROM.read(EEPROM_CENTER_START + 2) << 8) | EEPROM.read(EEPROM_CENTER_START + 3);

  // Sanity checks
  if (deadzone1Coarse < 10 || deadzone1Coarse > 250) deadzone1Coarse = DEFAULT_DEADZONE_COARSE;
  if (deadzone1Fine < 10 || deadzone1Fine > 250) deadzone1Fine = DEFAULT_DEADZONE_FINE;
  if (deadzone2Coarse < 10 || deadzone2Coarse > 250) deadzone2Coarse = DEFAULT_DEADZONE_COARSE;
  if (deadzone2Fine < 10 || deadzone2Fine > 250) deadzone2Fine = DEFAULT_DEADZONE_FINE;
  if (center1 < 100 || center1 > 900) center1 = DEFAULT_CENTER;
  if (center2 < 100 || center2 > 900) center2 = DEFAULT_CENTER;
}

void saveDefaults() {
  EEPROM.write(EEPROM_MAGIC, EEPROM_MAGIC_VALUE);
  EEPROM.write(EEPROM_MODE, 0);  // Proportional mode
  for (int i = 0; i < NUM_KEYS; i++) {
    EEPROM.write(EEPROM_KEYS_START + i, DEFAULT_KEYS[i]);
  }
  // Default dead zones
  EEPROM.write(EEPROM_DEADZONE_START, DEFAULT_DEADZONE_COARSE);
  EEPROM.write(EEPROM_DEADZONE_START + 1, DEFAULT_DEADZONE_FINE);
  EEPROM.write(EEPROM_DEADZONE_START + 2, DEFAULT_DEADZONE_COARSE);
  EEPROM.write(EEPROM_DEADZONE_START + 3, DEFAULT_DEADZONE_FINE);
  // Default centers
  EEPROM.write(EEPROM_CENTER_START, highByte(DEFAULT_CENTER));
  EEPROM.write(EEPROM_CENTER_START + 1, lowByte(DEFAULT_CENTER));
  EEPROM.write(EEPROM_CENTER_START + 2, highByte(DEFAULT_CENTER));
  EEPROM.write(EEPROM_CENTER_START + 3, lowByte(DEFAULT_CENTER));
}

void saveSettings() {
  EEPROM.write(EEPROM_MODE, controlMode);
  for (int i = 0; i < NUM_KEYS; i++) {
    EEPROM.write(EEPROM_KEYS_START + i, keys[i]);
  }
}

void saveDeadzones() {
  EEPROM.write(EEPROM_DEADZONE_START, deadzone1Coarse);
  EEPROM.write(EEPROM_DEADZONE_START + 1, deadzone1Fine);
  EEPROM.write(EEPROM_DEADZONE_START + 2, deadzone2Coarse);
  EEPROM.write(EEPROM_DEADZONE_START + 3, deadzone2Fine);
  EEPROM.write(EEPROM_CENTER_START, highByte(center1));
  EEPROM.write(EEPROM_CENTER_START + 1, lowByte(center1));
  EEPROM.write(EEPROM_CENTER_START + 2, highByte(center2));
  EEPROM.write(EEPROM_CENTER_START + 3, lowByte(center2));
}

// === Serial Configuration Interface ===
void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialPos > 0) {
        serialBuffer[serialPos] = '\0';
        processCommand(serialBuffer);
        serialPos = 0;
      }
    } else if (serialPos < sizeof(serialBuffer) - 1) {
      serialBuffer[serialPos++] = c;
    }
  }
}

// Convert char buffer to uppercase in place
void toUpper(char *s) {
  for (; *s; s++) {
    if (*s >= 'a' && *s <= 'z') *s -= 32;
  }
}

void processCommand(char *raw) {
  // Trim leading/trailing spaces
  while (*raw == ' ') raw++;
  char *end = raw + strlen(raw) - 1;
  while (end > raw && *end == ' ') *end-- = '\0';

  // Work on uppercase copy for matching
  char cmd[32];
  strncpy(cmd, raw, sizeof(cmd) - 1);
  cmd[sizeof(cmd) - 1] = '\0';
  toUpper(cmd);

  if (strcmp(cmd, "HELP") == 0) {
    printHelp();
  }
  else if (strcmp(cmd, "STATUS") == 0) {
    printStatus();
  }
  else if (strcmp(cmd, "KEYS") == 0) {
    printKeys();
  }
  else if (strcmp(cmd, "MODE PROPORTIONAL") == 0 || strcmp(cmd, "MODE 0") == 0) {
    controlMode = 0;
    saveSettings();
    Serial.println(F("Mode set to PROPORTIONAL"));
  }
  else if (strcmp(cmd, "MODE DISCRETE") == 0 || strcmp(cmd, "MODE 1") == 0) {
    controlMode = 1;
    saveSettings();
    Serial.println(F("Mode set to DISCRETE TOGGLE"));
  }
  else if (strncmp(cmd, "SET ", 4) == 0) {
    handleSetKey(cmd + 4);
  }
  else if (strcmp(cmd, "DEADZONE") == 0 || strcmp(cmd, "DZ") == 0) {
    printDeadzones();
  }
  else if (strncmp(cmd, "DZ ", 3) == 0) {
    handleSetDeadzone(cmd + 3);
  }
  else if (strncmp(cmd, "DEADZONE ", 9) == 0) {
    handleSetDeadzone(cmd + 9);
  }
  else if (strcmp(cmd, "CALIBRATE 1") == 0 || strcmp(cmd, "CAL 1") == 0) {
    runCalibration(JOY1_X, JOY1_Y, LED_FINE1, 1);
  }
  else if (strcmp(cmd, "CALIBRATE 2") == 0 || strcmp(cmd, "CAL 2") == 0) {
    runCalibration(JOY2_X, JOY2_Y, LED_FINE2, 2);
  }
  else if (strcmp(cmd, "RAW") == 0) {
    printRawValues();
  }
  else if (strcmp(cmd, "DEFAULTS") == 0) {
    saveDefaults();
    loadSettings();
    Serial.println(F("All settings reset to defaults."));
    printKeys();
    printDeadzones();
  }
  else {
    Serial.println(F("Unknown command. Type HELP."));
  }
}

void handleSetKey(const char *args) {
  // Format: SET <SLOT> <KEY>
  // Find space between slot and key
  const char *sp = strchr(args, ' ');
  if (!sp) {
    Serial.println(F("Usage: SET <SLOT> <KEY>"));
    return;
  }

  // Extract slot (up to 4 chars)
  char slot[5];
  int slotLen = sp - args;
  if (slotLen > 4) slotLen = 4;
  strncpy(slot, args, slotLen);
  slot[slotLen] = '\0';
  toUpper(slot);

  // Extract key (single char after space)
  const char *keyPtr = sp + 1;
  while (*keyPtr == ' ') keyPtr++;
  if (*keyPtr == '\0' || *(keyPtr + 1) != '\0') {
    Serial.println(F("Key must be a single character."));
    return;
  }

  char newKey = *keyPtr;
  if (newKey >= 'A' && newKey <= 'Z') newKey += 32; // lowercase

  int idx = -1;
  if (strcmp(slot, "J1LC") == 0) idx = J1_LEFT_COARSE;
  else if (strcmp(slot, "J1RC") == 0) idx = J1_RIGHT_COARSE;
  else if (strcmp(slot, "J1LF") == 0) idx = J1_LEFT_FINE;
  else if (strcmp(slot, "J1RF") == 0) idx = J1_RIGHT_FINE;
  else if (strcmp(slot, "J1B") == 0)  idx = J1_BTN;
  else if (strcmp(slot, "J2LC") == 0) idx = J2_LEFT_COARSE;
  else if (strcmp(slot, "J2RC") == 0) idx = J2_RIGHT_COARSE;
  else if (strcmp(slot, "J2LF") == 0) idx = J2_LEFT_FINE;
  else if (strcmp(slot, "J2RF") == 0) idx = J2_RIGHT_FINE;
  else if (strcmp(slot, "J2B") == 0)  idx = J2_BTN;

  if (idx < 0) {
    Serial.println(F("Unknown slot. Use: J1LC J1RC J1LF J1RF J1B J2LC J2RC J2LF J2RF J2B"));
    return;
  }

  keys[idx] = newKey;
  saveSettings();
  Serial.print(F("Set "));
  Serial.print(slot);
  Serial.print(F(" to '"));
  Serial.print(newKey);
  Serial.println('\'');
}

void handleSetDeadzone(const char *args) {
  // Format: DZ <JOY> <COARSE> <FINE>
  // Parse three space-separated integers
  int joyNum = 0, coarse = 0, fine = 0;
  if (sscanf(args, "%d %d %d", &joyNum, &coarse, &fine) != 3) {
    Serial.println(F("Usage: DZ <1|2> <coarse> <fine>"));
    return;
  }
  if (joyNum != 1 && joyNum != 2) {
    Serial.println(F("Usage: DZ <1|2> <coarse> <fine>"));
    return;
  }
  if (coarse < 10 || coarse > 250 || fine < 10 || fine > 250) {
    Serial.println(F("Values must be 10-250."));
    return;
  }

  if (joyNum == 1) {
    deadzone1Coarse = coarse;
    deadzone1Fine = fine;
  } else {
    deadzone2Coarse = coarse;
    deadzone2Fine = fine;
  }
  saveDeadzones();
  Serial.print(F("Joy"));
  Serial.print(joyNum);
  Serial.print(F(" DZ: coarse="));
  Serial.print(coarse);
  Serial.print(F(" fine="));
  Serial.println(fine);
}

void printDeadzones() {
  Serial.println();
  Serial.println(F("Dead Zones:"));
  Serial.print(F("  Joy1 c="));
  Serial.print(deadzone1Coarse);
  Serial.print(F(" f="));
  Serial.print(deadzone1Fine);
  Serial.print(F(" center="));
  Serial.println(center1);
  Serial.print(F("  Joy2 c="));
  Serial.print(deadzone2Coarse);
  Serial.print(F(" f="));
  Serial.print(deadzone2Fine);
  Serial.print(F(" center="));
  Serial.println(center2);
}

void printRawValues() {
  Serial.println(F("Raw (10 readings):"));
  for (int i = 0; i < 10; i++) {
    Serial.print(F("  J1 X="));
    Serial.print(analogRead(JOY1_X));
    Serial.print(F(" Y="));
    Serial.print(analogRead(JOY1_Y));
    Serial.print(F("  J2 X="));
    Serial.print(analogRead(JOY2_X));
    Serial.print(F(" Y="));
    Serial.println(analogRead(JOY2_Y));
    delay(100);
  }
}

void printHelp() {
  Serial.println();
  Serial.println(F("=== CV Axle Controller ==="));
  Serial.println(F("HELP    STATUS    KEYS"));
  Serial.println(F("MODE PROPORTIONAL|DISCRETE"));
  Serial.println(F("SET <SLOT> <KEY>"));
  Serial.println(F("  J1LC J1RC J1LF J1RF J1B"));
  Serial.println(F("  J2LC J2RC J2LF J2RF J2B"));
  Serial.println(F("DZ           - Show dead zones"));
  Serial.println(F("DZ <1|2> C F - Set dead zones"));
  Serial.println(F("CAL <1|2>    - Auto-calibrate"));
  Serial.println(F("RAW          - Analog readings"));
  Serial.println(F("DEFAULTS     - Factory reset"));
  Serial.println(F("Hold button 3s = auto-cal"));
}

void printStatus() {
  Serial.print(F("Mode: "));
  Serial.println(controlMode == 0 ? F("PROPORTIONAL") : F("DISCRETE"));
  Serial.print(F("Lock: "));
  Serial.println(locked ? F("LOCKED") : F("ACTIVE"));
  printDeadzones();
}

void printKeys() {
  Serial.println(F("\nKey Mappings:"));
  // Print each key inline to avoid storing label array in RAM
  Serial.print(F("  J1LC='"));Serial.print(keys[0]);
  Serial.print(F("' J1RC='"));Serial.print(keys[1]);
  Serial.print(F("' J1LF='"));Serial.print(keys[2]);
  Serial.print(F("' J1RF='"));Serial.println(keys[3]);Serial.print('\'');
  Serial.print(F("  J1B='"));Serial.print(keys[4]);
  Serial.print(F("' J2LC='"));Serial.print(keys[5]);
  Serial.print(F("' J2RC='"));Serial.print(keys[6]);
  Serial.print(F("' J2LF='"));Serial.println(keys[7]);Serial.print('\'');
  Serial.print(F("  J2RF='"));Serial.print(keys[8]);
  Serial.print(F("' J2B='"));Serial.print(keys[9]);
  Serial.println('\'');
}
