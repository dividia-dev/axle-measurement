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

// Serial config command buffer
String serialBuffer = "";

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

  Serial.println("AXLE_CONTROLLER_READY");
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
  Serial.print("CALIBRATING Joystick ");
  Serial.println(joyNum);
  Serial.println("  Release joystick and don't touch...");

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

  Serial.print("  Center: ");
  Serial.println(centerX);
  Serial.print("  Noise X: ");
  Serial.print(noiseX);
  Serial.print("  Noise Y: ");
  Serial.println(noiseY);
  Serial.print("  Dead zone coarse: ");
  Serial.println(dzCoarse);
  Serial.print("  Dead zone fine: ");
  Serial.println(dzFine);
  Serial.print("  Samples: ");
  Serial.println(samples);
  Serial.println("CALIBRATION COMPLETE");
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
      if (serialBuffer.length() > 0) {
        processCommand(serialBuffer);
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
    }
  }
}

void processCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "HELP") {
    printHelp();
  }
  else if (cmd == "STATUS") {
    printStatus();
  }
  else if (cmd == "KEYS") {
    printKeys();
  }
  else if (cmd == "MODE PROPORTIONAL" || cmd == "MODE 0") {
    controlMode = 0;
    saveSettings();
    Serial.println("Mode set to PROPORTIONAL");
  }
  else if (cmd == "MODE DISCRETE" || cmd == "MODE 1") {
    controlMode = 1;
    saveSettings();
    Serial.println("Mode set to DISCRETE TOGGLE");
  }
  else if (cmd.startsWith("SET ")) {
    handleSetKey(cmd.substring(4));
  }
  else if (cmd == "DEADZONE" || cmd == "DZ") {
    printDeadzones();
  }
  else if (cmd.startsWith("DZ ") || cmd.startsWith("DEADZONE ")) {
    String args = cmd.startsWith("DZ ") ? cmd.substring(3) : cmd.substring(9);
    handleSetDeadzone(args);
  }
  else if (cmd == "CALIBRATE 1" || cmd == "CAL 1") {
    runCalibration(JOY1_X, JOY1_Y, LED_FINE1, 1);
  }
  else if (cmd == "CALIBRATE 2" || cmd == "CAL 2") {
    runCalibration(JOY2_X, JOY2_Y, LED_FINE2, 2);
  }
  else if (cmd == "RAW") {
    printRawValues();
  }
  else if (cmd == "DEFAULTS") {
    saveDefaults();
    loadSettings();
    Serial.println("All settings reset to defaults (including dead zones).");
    printKeys();
    printDeadzones();
  }
  else {
    Serial.println("Unknown command. Type HELP for commands.");
  }
}

void handleSetKey(String args) {
  // Format: SET <SLOT> <KEY>
  // Example: SET J1LC w  (sets joystick 1 left coarse to 'w')
  args.trim();
  int spaceIdx = args.indexOf(' ');
  if (spaceIdx < 0) {
    Serial.println("Usage: SET <SLOT> <KEY>");
    Serial.println("Slots: J1LC J1RC J1LF J1RF J1B J2LC J2RC J2LF J2RF J2B");
    return;
  }

  String slot = args.substring(0, spaceIdx);
  String keyStr = args.substring(spaceIdx + 1);
  keyStr.trim();
  keyStr.toLowerCase();

  if (keyStr.length() != 1) {
    Serial.println("Key must be a single character.");
    return;
  }

  char newKey = keyStr.charAt(0);
  int idx = -1;

  if (slot == "J1LC") idx = J1_LEFT_COARSE;
  else if (slot == "J1RC") idx = J1_RIGHT_COARSE;
  else if (slot == "J1LF") idx = J1_LEFT_FINE;
  else if (slot == "J1RF") idx = J1_RIGHT_FINE;
  else if (slot == "J1B")  idx = J1_BTN;
  else if (slot == "J2LC") idx = J2_LEFT_COARSE;
  else if (slot == "J2RC") idx = J2_RIGHT_COARSE;
  else if (slot == "J2LF") idx = J2_LEFT_FINE;
  else if (slot == "J2RF") idx = J2_RIGHT_FINE;
  else if (slot == "J2B")  idx = J2_BTN;

  if (idx < 0) {
    Serial.println("Unknown slot. Use: J1LC J1RC J1LF J1RF J1B J2LC J2RC J2LF J2RF J2B");
    return;
  }

  keys[idx] = newKey;
  saveSettings();
  Serial.print("Set ");
  Serial.print(slot);
  Serial.print(" to '");
  Serial.print(newKey);
  Serial.println("'");
}

void handleSetDeadzone(String args) {
  // Format: DZ <JOY> <COARSE> <FINE>
  // Example: DZ 1 60 40
  args.trim();
  int sp1 = args.indexOf(' ');
  if (sp1 < 0) {
    Serial.println("Usage: DZ <1|2> <coarse> <fine>");
    Serial.println("  Example: DZ 1 60 40");
    return;
  }
  int joyNum = args.substring(0, sp1).toInt();
  String rest = args.substring(sp1 + 1);
  rest.trim();
  int sp2 = rest.indexOf(' ');
  if (sp2 < 0 || (joyNum != 1 && joyNum != 2)) {
    Serial.println("Usage: DZ <1|2> <coarse> <fine>");
    return;
  }
  int coarse = rest.substring(0, sp2).toInt();
  int fine = rest.substring(sp2 + 1).toInt();

  if (coarse < 10 || coarse > 250 || fine < 10 || fine > 250) {
    Serial.println("Values must be 10-250.");
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
  Serial.print("Joystick ");
  Serial.print(joyNum);
  Serial.print(" dead zones set: coarse=");
  Serial.print(coarse);
  Serial.print(" fine=");
  Serial.println(fine);
}

void printDeadzones() {
  Serial.println();
  Serial.println("Dead Zone Settings:");
  Serial.println("------------------------------");
  Serial.print("  Joy1 coarse: ");
  Serial.print(deadzone1Coarse);
  Serial.print("  fine: ");
  Serial.print(deadzone1Fine);
  Serial.print("  center: ");
  Serial.println(center1);
  Serial.print("  Joy2 coarse: ");
  Serial.print(deadzone2Coarse);
  Serial.print("  fine: ");
  Serial.print(deadzone2Fine);
  Serial.print("  center: ");
  Serial.println(center2);
  Serial.println();
}

void printRawValues() {
  Serial.println("Raw analog values (10 readings):");
  for (int i = 0; i < 10; i++) {
    Serial.print("  J1 X=");
    Serial.print(analogRead(JOY1_X));
    Serial.print(" Y=");
    Serial.print(analogRead(JOY1_Y));
    Serial.print("  J2 X=");
    Serial.print(analogRead(JOY2_X));
    Serial.print(" Y=");
    Serial.println(analogRead(JOY2_Y));
    delay(100);
  }
}

void printHelp() {
  Serial.println();
  Serial.println("=== CV Axle Controller ===");
  Serial.println("Commands (via Serial Monitor):");
  Serial.println("  HELP       - Show this help");
  Serial.println("  STATUS     - Show current mode, lock, and dead zones");
  Serial.println("  KEYS       - Show current key mappings");
  Serial.println("  MODE PROPORTIONAL - Deflection = speed (default)");
  Serial.println("  MODE DISCRETE     - Button toggles coarse/fine");
  Serial.println("  SET <SLOT> <KEY>  - Remap a key");
  Serial.println("    Slots: J1LC J1RC J1LF J1RF J1B J2LC J2RC J2LF J2RF J2B");
  Serial.println("    Example: SET J1LC w");
  Serial.println("  DZ             - Show dead zone values");
  Serial.println("  DZ <1|2> <C> <F> - Set dead zones manually");
  Serial.println("    Example: DZ 1 60 40");
  Serial.println("  CAL <1|2>      - Auto-calibrate joystick dead zone");
  Serial.println("  RAW            - Show raw analog readings");
  Serial.println("  DEFAULTS       - Reset all settings to factory");
  Serial.println();
  Serial.println("Auto-calibrate: hold joystick button 3 sec");
  Serial.println();
}

void printStatus() {
  Serial.println();
  Serial.print("Mode: ");
  Serial.println(controlMode == 0 ? "PROPORTIONAL" : "DISCRETE TOGGLE");
  Serial.print("Lock: ");
  Serial.println(locked ? "LOCKED" : "ACTIVE");
  printDeadzones();
}

void printKeys() {
  Serial.println();
  Serial.println("Current Key Mappings:");
  Serial.println("------------------------------");
  const char* labels[] = {
    "Joy1 Left Coarse  (J1LC)",
    "Joy1 Right Coarse (J1RC)",
    "Joy1 Left Fine    (J1LF)",
    "Joy1 Right Fine   (J1RF)",
    "Joy1 Button       (J1B) ",
    "Joy2 Left Coarse  (J2LC)",
    "Joy2 Right Coarse (J2RC)",
    "Joy2 Left Fine    (J2LF)",
    "Joy2 Right Fine   (J2RF)",
    "Joy2 Button       (J2B) "
  };
  for (int i = 0; i < NUM_KEYS; i++) {
    Serial.print("  ");
    Serial.print(labels[i]);
    Serial.print(" → '");
    Serial.print(keys[i]);
    Serial.println("'");
  }
  Serial.println();
}
