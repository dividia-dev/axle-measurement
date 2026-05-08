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

const byte EEPROM_MAGIC_VALUE = 0xAC; // "AC" for Axle Controller
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
  '1',        // Joy1 button
  'j', 'l',   // Joy2 coarse left/right
  'u', 'o',   // Joy2 fine left/right
  '2'         // Joy2 button
};

// Active key mappings (loaded from EEPROM)
char keys[NUM_KEYS];

// === Tuning Parameters ===
const int DEADZONE_COARSE = 80;
const int DEADZONE_FINE = 60;
const int CENTER = 512;

// Repeat rate control (milliseconds between keystrokes)
const int REPEAT_FAST = 30;
const int REPEAT_SLOW = 200;
const int REPEAT_FINE = 150;

// Fine axis engagement threshold
const int FINE_ACTIVE_THRESHOLD = 40;

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

  // Process each joystick
  processJoystick(
    JOY1_X, JOY1_Y, JOY1_BTN, LED_FINE1,
    lastRepeat1, lastBtn1Press, discreteFineMode1,
    J1_LEFT_COARSE, J1_RIGHT_COARSE, J1_LEFT_FINE, J1_RIGHT_FINE, J1_BTN
  );

  processJoystick(
    JOY2_X, JOY2_Y, JOY2_BTN, LED_FINE2,
    lastRepeat2, lastBtn2Press, discreteFineMode2,
    J2_LEFT_COARSE, J2_RIGHT_COARSE, J2_LEFT_FINE, J2_RIGHT_FINE, J2_BTN
  );
}

// === Joystick Processing ===
void processJoystick(
  int pinX, int pinY, int pinBtn, int ledFine,
  unsigned long &lastRepeat, unsigned long &lastBtnPress, bool &discreteFine,
  int keyLeftCoarse, int keyRightCoarse, int keyLeftFine, int keyRightFine, int keyBtn
) {
  int rawX = analogRead(pinX);
  int rawY = analogRead(pinY);
  bool btnPressed = (digitalRead(pinBtn) == LOW);

  int deflectX = rawX - CENTER;
  int deflectY = rawY - CENTER;

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
    deadzone = DEADZONE_FINE;
    repeatRate = REPEAT_FINE;
    keyLeft = keyLeftFine;
    keyRight = keyRightFine;
  } else {
    deflection = deflectX;
    deadzone = DEADZONE_COARSE;
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

// === EEPROM Settings ===
void loadSettings() {
  if (EEPROM.read(EEPROM_MAGIC) != EEPROM_MAGIC_VALUE) {
    // First run — write defaults
    saveDefaults();
  }

  controlMode = EEPROM.read(EEPROM_MODE);
  for (int i = 0; i < NUM_KEYS; i++) {
    keys[i] = EEPROM.read(EEPROM_KEYS_START + i);
  }
}

void saveDefaults() {
  EEPROM.write(EEPROM_MAGIC, EEPROM_MAGIC_VALUE);
  EEPROM.write(EEPROM_MODE, 0);  // Proportional mode
  for (int i = 0; i < NUM_KEYS; i++) {
    EEPROM.write(EEPROM_KEYS_START + i, DEFAULT_KEYS[i]);
  }
}

void saveSettings() {
  EEPROM.write(EEPROM_MODE, controlMode);
  for (int i = 0; i < NUM_KEYS; i++) {
    EEPROM.write(EEPROM_KEYS_START + i, keys[i]);
  }
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
  else if (cmd == "DEFAULTS") {
    saveDefaults();
    loadSettings();
    Serial.println("All settings reset to defaults.");
    printKeys();
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

void printHelp() {
  Serial.println();
  Serial.println("=== CV Axle Controller ===");
  Serial.println("Commands (via Serial Monitor):");
  Serial.println("  HELP       - Show this help");
  Serial.println("  STATUS     - Show current mode and lock state");
  Serial.println("  KEYS       - Show current key mappings");
  Serial.println("  MODE PROPORTIONAL - Deflection = speed (default)");
  Serial.println("  MODE DISCRETE     - Button toggles coarse/fine");
  Serial.println("  SET <SLOT> <KEY>  - Remap a key");
  Serial.println("    Slots: J1LC J1RC J1LF J1RF J1B J2LC J2RC J2LF J2RF J2B");
  Serial.println("    Example: SET J1LC w");
  Serial.println("  DEFAULTS   - Reset all settings to factory");
  Serial.println();
}

void printStatus() {
  Serial.println();
  Serial.print("Mode: ");
  Serial.println(controlMode == 0 ? "PROPORTIONAL" : "DISCRETE TOGGLE");
  Serial.print("Lock: ");
  Serial.println(locked ? "LOCKED" : "ACTIVE");
  Serial.println();
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
