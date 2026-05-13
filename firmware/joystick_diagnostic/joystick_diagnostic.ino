/*
 * Joystick Diagnostic - Scan ALL analog pins
 *
 * Reads every analog-capable pin on the ATmega32U4 (Pro Micro)
 * to help identify which physical pin maps to which analog channel.
 *
 * Connect your pot's middle prong to any pin and turn it —
 * the one that changes is your actual analog pin.
 */

const int BTN1 = 2;
const int BTN2 = 3;

void setup() {
  Serial.begin(115200);
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);

  delay(1000);
  Serial.println("=== FULL PIN SCAN ===");
  Serial.println("Turn pot - find which value changes");
  Serial.println("====================================");
  delay(500);
}

void loop() {
  Serial.print("A0:");
  Serial.print(analogRead(A0));
  Serial.print("  A1:");
  Serial.print(analogRead(A1));
  Serial.print("  A2:");
  Serial.print(analogRead(A2));
  Serial.print("  A3:");
  Serial.print(analogRead(A3));
  Serial.print("  D4:");
  Serial.print(analogRead(4));
  Serial.print("  D6:");
  Serial.print(analogRead(6));
  Serial.print("  D8:");
  Serial.print(analogRead(8));
  Serial.print("  D9:");
  Serial.print(analogRead(9));
  Serial.print("  D10:");
  Serial.print(analogRead(10));
  Serial.print("  D12:");
  Serial.print(analogRead(12));

  Serial.print("  | B1:");
  Serial.print(digitalRead(BTN1) == LOW ? "YES" : "-");
  Serial.print(" B2:");
  Serial.println(digitalRead(BTN2) == LOW ? "YES" : "-");

  delay(300);
}
