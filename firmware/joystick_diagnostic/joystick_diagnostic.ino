/*
 * Joystick Diagnostic - Identify axes and test wiring
 *
 * Upload this first to verify your joystick wiring before
 * running the main controller firmware.
 *
 * Open Serial Monitor at 115200 baud and move the joystick.
 * It prints raw analog values so you can identify:
 *   - Which pin is X (changes when you push left/right)
 *   - Which pin is Y (changes when you push up/down)
 *   - Center values (should be ~512)
 *   - Button press (if wired)
 *
 * Wiring (same as main firmware):
 *   Joystick 1: pot1 middle→A0, pot2 middle→A1, btn→D2
 *   Joystick 2: pot1 middle→A2, pot2 middle→A3, btn→D3
 *   All pot outer prongs: one side to 5V, other to GND
 */

const int PIN_A0 = A0;
const int PIN_A1 = A1;
const int PIN_A2 = A2;
const int PIN_A3 = A3;
const int BTN1 = 2;
const int BTN2 = 3;

void setup() {
  Serial.begin(115200);
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);

  Serial.println("=== Joystick Diagnostic ===");
  Serial.println("Move each joystick and watch which values change.");
  Serial.println("Format: A0 | A1 | A2 | A3 | BTN1 | BTN2");
  Serial.println("Center should read ~512. Full range 0-1023.");
  Serial.println("==========================================");
  delay(1000);
}

void loop() {
  int a0 = analogRead(PIN_A0);
  int a1 = analogRead(PIN_A1);
  int a2 = analogRead(PIN_A2);
  int a3 = analogRead(PIN_A3);
  int b1 = digitalRead(BTN1);
  int b2 = digitalRead(BTN2);

  Serial.print("A0:");
  Serial.print(a0);
  Serial.print("\t| A1:");
  Serial.print(a1);
  Serial.print("\t| A2:");
  Serial.print(a2);
  Serial.print("\t| A3:");
  Serial.print(a3);
  Serial.print("\t| BTN1:");
  Serial.print(b1 == LOW ? "PRESSED" : "-");
  Serial.print("\t| BTN2:");
  Serial.println(b2 == LOW ? "PRESSED" : "-");

  delay(200);  // Print ~5 times per second
}
