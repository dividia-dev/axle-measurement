/*
 * Single Joystick Test — Interactive
 *
 * Waits for serial input between each step.
 * Type any character and press Enter to proceed.
 *
 *   A0 = Coarse (X-axis, left/right)
 *   A1 = Fine   (Y-axis, twist barrel)
 *   D2 = Button (click stick down)
 */

const int PIN_COARSE = A0;
const int PIN_FINE   = A1;
const int PIN_BTN    = 2;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN, INPUT_PULLUP);
  delay(1500);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  LEFT JOYSTICK TEST (Interactive)");
  Serial.println("========================================");
  Serial.println();
}

void loop() {
  // --- Step 1: Center ---
  Serial.println("[1/4] CENTER TEST");
  Serial.println("  Leave the joystick centered (hands off).");
  waitForUser();

  // Take several readings and average
  long sumC = 0, sumF = 0;
  for (int i = 0; i < 20; i++) {
    sumC += analogRead(PIN_COARSE);
    sumF += analogRead(PIN_FINE);
    delay(10);
  }
  int centerCoarse = sumC / 20;
  int centerFine   = sumF / 20;

  Serial.print("  Coarse (A0) center: ");
  Serial.print(centerCoarse);
  Serial.println(inRange(centerCoarse, 400, 624) ? "  [OK]" : "  [!!] Expected ~512");
  Serial.print("  Fine   (A1) center: ");
  Serial.print(centerFine);
  // Fine/twist may rest at any position, so just report it
  Serial.println(inRange(centerFine, 300, 724) ? "  [OK]" : "  [NOTE] Twist may rest off-center, that's OK");
  Serial.println();

  // --- Step 2: Coarse sweep ---
  Serial.println("[2/4] COARSE AXIS TEST");
  Serial.println("  Push the stick LEFT and RIGHT a few times.");
  Serial.println("  Then send any key when done.");
  int coarseMin = 1023, coarseMax = 0;
  waitWhileSampling(PIN_COARSE, coarseMin, coarseMax);
  int coarseRange = coarseMax - coarseMin;
  Serial.print("  Range: ");
  Serial.print(coarseMin);
  Serial.print(" — ");
  Serial.print(coarseMax);
  Serial.print("  (span: ");
  Serial.print(coarseRange);
  Serial.print(")");
  Serial.println(coarseRange > 600 ? "  [OK]" : "  [!!] Push harder or check wiring");
  Serial.println();

  // --- Step 3: Fine sweep ---
  Serial.println("[3/4] FINE AXIS TEST");
  Serial.println("  Twist the barrel LEFT and RIGHT.");
  Serial.println("  Then send any key when done.");
  int fineMin = 1023, fineMax = 0;
  waitWhileSampling(PIN_FINE, fineMin, fineMax);
  int fineRange = fineMax - fineMin;
  Serial.print("  Range: ");
  Serial.print(fineMin);
  Serial.print(" — ");
  Serial.print(fineMax);
  Serial.print("  (span: ");
  Serial.print(fineRange);
  Serial.print(")");
  Serial.println(fineRange > 400 ? "  [OK]" : "  [!!] Twist harder or check wiring");
  Serial.println();

  // --- Step 4: Button ---
  Serial.println("[4/4] BUTTON TEST");
  Serial.println("  Click the stick down and hold it...");
  Serial.println("  Then send any key when done.");
  bool pressed = false;
  while (!Serial.available()) {
    if (digitalRead(PIN_BTN) == LOW) pressed = true;
    delay(20);
  }
  flushSerial();
  Serial.print("  Button: ");
  Serial.println(pressed ? "[OK] Press detected!" : "[!!] No press detected");
  Serial.println();

  // --- Summary ---
  bool centerOK = inRange(centerCoarse, 400, 624);
  bool coarseOK = coarseRange > 600;
  bool fineOK   = fineRange > 400;

  Serial.println("========================================");
  Serial.println("  RESULTS SUMMARY");
  Serial.println("========================================");
  printResult("Center readings", centerOK);
  printResult("Coarse axis (A0)", coarseOK);
  printResult("Fine axis   (A1)", fineOK);
  printResult("Button      (D2)", pressed);
  Serial.println();

  if (centerOK && coarseOK && fineOK && pressed) {
    Serial.println("  >>> ALL PASS <<<");
  } else {
    Serial.println("  >>> SOME TESTS FAILED <<<");
  }
  Serial.println("========================================");
  Serial.println();
  Serial.println("Send any key to run again, or unplug.");
  waitForUser();
}

// --- Helpers ---

void waitForUser() {
  Serial.println("  >> Send any key to continue <<");
  while (!Serial.available()) delay(20);
  flushSerial();
}

void waitWhileSampling(int pin, int &minVal, int &maxVal) {
  while (!Serial.available()) {
    int v = analogRead(pin);
    if (v < minVal) minVal = v;
    if (v > maxVal) maxVal = v;
    delay(10);
  }
  flushSerial();
}

void flushSerial() {
  while (Serial.available()) Serial.read();
}

bool inRange(int val, int lo, int hi) {
  return val >= lo && val <= hi;
}

void printResult(const char* name, bool pass) {
  Serial.print("  ");
  Serial.print(name);
  Serial.print(": ");
  Serial.println(pass ? "PASS" : "FAIL");
}
