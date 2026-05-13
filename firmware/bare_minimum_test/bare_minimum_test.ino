/*
 * Dual joystick diagnostic — reads all 4 axes and both buttons
 */

void setup() {
  Serial.begin(115200);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  delay(1000);
}

void loop() {
  int c1 = analogRead(A0);
  int f1 = analogRead(A1);
  int c2 = analogRead(A2);
  int f2 = analogRead(A3);
  int b1 = digitalRead(2);
  int b2 = digitalRead(3);

  Serial.print("J1 C:");
  Serial.print(c1);
  Serial.print(" F:");
  Serial.print(f1);
  Serial.print(" B:");
  Serial.print(b1 == LOW ? "YES" : "-");
  Serial.print("  |  J2 C:");
  Serial.print(c2);
  Serial.print(" F:");
  Serial.print(f2);
  Serial.print(" B:");
  Serial.println(b2 == LOW ? "YES" : "-");

  delay(300);
}
