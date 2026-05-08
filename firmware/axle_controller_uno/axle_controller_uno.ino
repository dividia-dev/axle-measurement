/*
 * CV Axle Controller - Prototype Firmware (Arduino Uno)
 *
 * Reads 2 thumbstick modules and sends movement commands over serial.
 * A Python bridge script on the host computer converts serial commands
 * to keystrokes.
 *
 * When ported to Pro Micro (ATmega32U4), Serial.println() calls
 * become Keyboard.press()/Keyboard.release() calls.
 *
 * Wiring:
 *   Joystick 1: VRx→A0, VRy→A1, SW→D2 (INPUT_PULLUP)
 *   Joystick 2: VRx→A2, VRy→A3, SW→D3 (INPUT_PULLUP)
 *   Lock switch: D4 (INPUT_PULLUP, toggle switch to GND)
 *   Lock LED (green): D5
 *   Fine LED 1 (blue): D6
 *   Fine LED 2 (blue): D7
 */

// === Pin Definitions ===
// Joystick 1
const int JOY1_X = A0;      // Coarse left/right
const int JOY1_Y = A1;      // Fine left/right (stand-in for twist barrel)
const int JOY1_BTN = 2;

// Joystick 2
const int JOY2_X = A2;      // Coarse left/right
const int JOY2_Y = A3;      // Fine left/right (stand-in for twist barrel)
const int JOY2_BTN = 3;

// Lock switch & LEDs
const int LOCK_SWITCH = 4;
const int LED_LOCK = 5;      // Green - lit when controller is ACTIVE (unlocked)
const int LED_FINE1 = 6;     // Blue - lit when joystick 1 fine mode active
const int LED_FINE2 = 7;     // Blue - lit when joystick 2 fine mode active

// === Tuning Parameters ===
const int DEADZONE_COARSE = 80;    // Coarse axis deadzone (center is ~512, so ±80)
const int DEADZONE_FINE = 60;      // Fine axis deadzone (more sensitive)
const int CENTER = 512;            // ADC midpoint

// Repeat rate control (milliseconds between keystrokes)
const int REPEAT_FAST = 30;        // Max speed (full deflection)
const int REPEAT_SLOW = 200;       // Min speed (just past deadzone)
const int REPEAT_FINE = 150;       // Fine mode repeat rate (steady, slow)

// Fine axis threshold - when fine input detected, lock coarse axis
const int FINE_ACTIVE_THRESHOLD = 40;  // How far fine axis must move to engage

// === State ===
unsigned long lastRepeat1 = 0;
unsigned long lastRepeat2 = 0;
bool locked = false;

void setup() {
  Serial.begin(115200);

  // Joystick buttons - internal pullup, pressed = LOW
  pinMode(JOY1_BTN, INPUT_PULLUP);
  pinMode(JOY2_BTN, INPUT_PULLUP);

  // Lock switch - internal pullup, engaged = LOW
  pinMode(LOCK_SWITCH, INPUT_PULLUP);

  // LEDs
  pinMode(LED_LOCK, OUTPUT);
  pinMode(LED_FINE1, OUTPUT);
  pinMode(LED_FINE2, OUTPUT);

  // Startup indicator - blink lock LED
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_LOCK, HIGH);
    delay(100);
    digitalWrite(LED_LOCK, LOW);
    delay(100);
  }

  Serial.println("AXLE_CONTROLLER_READY");
}

void loop() {
  // Check lock switch
  locked = (digitalRead(LOCK_SWITCH) == LOW);
  digitalWrite(LED_LOCK, locked ? LOW : HIGH);  // Green on = active/unlocked

  if (locked) {
    // All LEDs off except lock indicator (already off)
    digitalWrite(LED_FINE1, LOW);
    digitalWrite(LED_FINE2, LOW);
    return;
  }

  // Process each joystick independently
  processJoystick(1, JOY1_X, JOY1_Y, JOY1_BTN, LED_FINE1, lastRepeat1);
  processJoystick(2, JOY2_X, JOY2_Y, JOY2_BTN, LED_FINE2, lastRepeat2);
}

void processJoystick(int id, int pinX, int pinY, int pinBtn, int ledFine, unsigned long &lastRepeat) {
  int rawX = analogRead(pinX);
  int rawY = analogRead(pinY);
  bool btnPressed = (digitalRead(pinBtn) == LOW);

  // Calculate deflection from center
  int deflectX = rawX - CENTER;  // Positive = right, negative = left
  int deflectY = rawY - CENTER;

  // Determine if fine axis is active (twist-locks-X equivalent)
  bool fineActive = (abs(deflectY) > FINE_ACTIVE_THRESHOLD);
  digitalWrite(ledFine, fineActive ? HIGH : LOW);

  // Choose which axis to use
  int deflection;
  int deadzone;
  int repeatRate;
  String mode;

  if (fineActive) {
    // Fine mode: use Y-axis, ignore X
    deflection = deflectY;
    deadzone = DEADZONE_FINE;
    repeatRate = REPEAT_FINE;
    mode = "FINE";
  } else {
    // Coarse mode: use X-axis
    deflection = deflectX;
    deadzone = DEADZONE_COARSE;
    mode = "COARSE";

    // Proportional speed: map deflection magnitude to repeat rate
    int magnitude = abs(deflection);
    if (magnitude > deadzone) {
      // Map from deadzone..512 to REPEAT_SLOW..REPEAT_FAST
      repeatRate = map(magnitude, deadzone, 512, REPEAT_SLOW, REPEAT_FAST);
      repeatRate = constrain(repeatRate, REPEAT_FAST, REPEAT_SLOW);
    } else {
      return;  // Inside deadzone, no movement
    }
  }

  // Check if past deadzone
  if (abs(deflection) <= deadzone) {
    return;
  }

  // Determine direction
  String direction = (deflection > 0) ? "RIGHT" : "LEFT";

  // Rate-limited output
  unsigned long now = millis();
  if (now - lastRepeat >= (unsigned long)repeatRate) {
    lastRepeat = now;

    // Output format: JOY<id>_<DIRECTION>_<MODE>
    // e.g., JOY1_LEFT_COARSE, JOY2_RIGHT_FINE
    Serial.println("JOY" + String(id) + "_" + direction + "_" + mode);
  }

  // Button press (momentary)
  if (btnPressed) {
    Serial.println("JOY" + String(id) + "_BTN");
    delay(200);  // Simple debounce
  }
}
