/*
 * CV Axle Controller — Board Test (Auto-Cycle)
 *
 * Auto-advances through each pin every 20 seconds.
 * Serial monitor shows which terminal to probe.
 * LED on Pro Micro blinks to confirm it's running.
 *
 * No input needed — just watch serial and probe.
 */

// All test pins
const int pins[]  = {2,    3,    4,    5,    6,    7,    8,    9};
const int apins[] = {A0,   A2};

// How long to hold each pin (ms)
#define HOLD_MS 20000

void setup() {
  Serial.begin(115200);
  delay(2000);

  // All pins LOW
  for (int i = 0; i < 8; i++) {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  }

  Serial.println(F(""));
  Serial.println(F("=========================================="));
  Serial.println(F("  CV AXLE CONTROLLER — AUTO BOARD TEST"));
  Serial.println(F("=========================================="));
  Serial.println(F(""));
  Serial.println(F("Multimeter: DC VOLTS, black probe on GND."));
  Serial.println(F("Each pin held HIGH for 20 seconds."));
  Serial.println(F("Starting in 5 seconds..."));
  Serial.println(F(""));
  delay(5000);

  // ── TEST 1: D2 ──
  Serial.println(F("────────────────────────────────────────"));
  Serial.println(F("TEST 1/12: D2 — J1 BUTTON"));
  Serial.println(F("  PROBE: LEFT green block, BOTTOM screw (6th from top)"));
  Serial.println(F("  Should read ~5V"));
  Serial.println(F("────────────────────────────────────────"));
  digitalWrite(2, HIGH);
  delay(HOLD_MS);
  digitalWrite(2, LOW);

  // ── TEST 2: D3 ──
  Serial.println(F("────────────────────────────────────────"));
  Serial.println(F("TEST 2/12: D3 — J2 BUTTON"));
  Serial.println(F("  PROBE: RIGHT blue block, BOTTOM screw (6th from top)"));
  Serial.println(F("  Should read ~5V"));
  Serial.println(F("────────────────────────────────────────"));
  digitalWrite(3, HIGH);
  delay(HOLD_MS);
  digitalWrite(3, LOW);

  // ── TEST 3: D4 ──
  Serial.println(F("────────────────────────────────────────"));
  Serial.println(F("TEST 3/12: D4 — LOCK TOGGLE SIGNAL"));
  Serial.println(F("  PROBE: RIGHT blue block, 2nd from BOTTOM (5th from top)"));
  Serial.println(F("  Should read ~5V"));
  Serial.println(F("────────────────────────────────────────"));
  digitalWrite(4, HIGH);
  delay(HOLD_MS);
  digitalWrite(4, LOW);

  // ── TEST 4: D5 ──
  Serial.println(F("────────────────────────────────────────"));
  Serial.println(F("TEST 4/12: D5 — RGB LED RED"));
  Serial.println(F("  PROBE: 3-pin RGB header, pin 1 (RED)"));
  Serial.println(F("  Should read ~5V"));
  Serial.println(F("────────────────────────────────────────"));
  digitalWrite(5, HIGH);
  delay(HOLD_MS);
  digitalWrite(5, LOW);

  // ── TEST 5: D6 ──
  Serial.println(F("────────────────────────────────────────"));
  Serial.println(F("TEST 5/12: D6 — FINE LED 1 (through R1 220 ohm)"));
  Serial.println(F("  PROBE: LEFT green block, TOP screw (1st from top)"));
  Serial.println(F("  Should read ~4-5V (voltage drop across resistor)"));
  Serial.println(F("────────────────────────────────────────"));
  digitalWrite(6, HIGH);
  delay(HOLD_MS);
  digitalWrite(6, LOW);

  // ── TEST 6: D7 ──
  Serial.println(F("────────────────────────────────────────"));
  Serial.println(F("TEST 6/12: D7 — FINE LED 2 (through R2 220 ohm)"));
  Serial.println(F("  PROBE: RIGHT blue block, TOP screw (1st from top)"));
  Serial.println(F("  Should read ~4-5V (voltage drop across resistor)"));
  Serial.println(F("────────────────────────────────────────"));
  digitalWrite(7, HIGH);
  delay(HOLD_MS);
  digitalWrite(7, LOW);

  // ── TEST 7: D8 ──
  Serial.println(F("────────────────────────────────────────"));
  Serial.println(F("TEST 7/12: D8 — RECAL BUTTON SIGNAL"));
  Serial.println(F("  PROBE: RIGHT blue block, 3rd from BOTTOM (4th from top)"));
  Serial.println(F("  Should read ~5V"));
  Serial.println(F("────────────────────────────────────────"));
  digitalWrite(8, HIGH);
  delay(HOLD_MS);
  digitalWrite(8, LOW);

  // ── TEST 8: D9 ──
  Serial.println(F("────────────────────────────────────────"));
  Serial.println(F("TEST 8/12: D9 — RGB LED GREEN"));
  Serial.println(F("  PROBE: 3-pin RGB header, pin 2 (GREEN)"));
  Serial.println(F("  Should read ~5V"));
  Serial.println(F("────────────────────────────────────────"));
  digitalWrite(9, HIGH);
  delay(HOLD_MS);
  digitalWrite(9, LOW);

  // ── TEST 9: A0 ──
  Serial.println(F("────────────────────────────────────────"));
  Serial.println(F("TEST 9/12: A0 — J1 POT WIPER"));
  Serial.println(F("  PROBE: LEFT green block, 3rd from top"));
  Serial.println(F("  Should read ~5V"));
  Serial.println(F("────────────────────────────────────────"));
  pinMode(A0, OUTPUT);
  digitalWrite(A0, HIGH);
  delay(HOLD_MS);
  digitalWrite(A0, LOW);
  pinMode(A0, INPUT);

  // ── TEST 10: A2 ──
  Serial.println(F("────────────────────────────────────────"));
  Serial.println(F("TEST 10/12: A2 — J2 POT WIPER"));
  Serial.println(F("  PROBE: RIGHT blue block, 3rd from top"));
  Serial.println(F("  Should read ~5V"));
  Serial.println(F("────────────────────────────────────────"));
  pinMode(A2, OUTPUT);
  digitalWrite(A2, HIGH);
  delay(HOLD_MS);
  digitalWrite(A2, LOW);
  pinMode(A2, INPUT);

  // ── TEST 11: VCC ──
  Serial.println(F("────────────────────────────────────────"));
  Serial.println(F("TEST 11/12: VCC BUS (always powered)"));
  Serial.println(F("  PROBE: LEFT green block, 4th from top — should read ~5V"));
  Serial.println(F("  PROBE: RIGHT blue block, 4th from top — should read ~5V"));
  Serial.println(F("────────────────────────────────────────"));
  delay(HOLD_MS);

  // ── TEST 12: GND ──
  Serial.println(F("────────────────────────────────────────"));
  Serial.println(F("TEST 12/12: GND CONTINUITY"));
  Serial.println(F("  Switch multimeter to CONTINUITY (beep mode)"));
  Serial.println(F("  Touch one probe to Pro Micro GND."));
  Serial.println(F("  Each of these should beep:"));
  Serial.println(F("    LEFT green block, 2nd from top   (FL1 GND)"));
  Serial.println(F("    LEFT green block, 5th from top   (J1 GND)"));
  Serial.println(F("    RIGHT blue block, 2nd from top   (FL2 GND)"));
  Serial.println(F("    RIGHT blue block, 5th from top   (J2 GND)"));
  Serial.println(F("    3-pin RGB header, pin 3           (RGB GND)"));
  Serial.println(F("────────────────────────────────────────"));
  delay(30000);

  // ── DONE ──
  Serial.println(F(""));
  Serial.println(F("=========================================="));
  Serial.println(F("  BOARD TEST COMPLETE"));
  Serial.println(F("=========================================="));
  Serial.println(F(""));
  Serial.println(F("TERMINAL MAP (USB-C up):"));
  Serial.println(F(""));
  Serial.println(F("LEFT GREEN (top to bottom):     RIGHT BLUE (top to bottom):"));
  Serial.println(F("  1. FL1+ (D6 via R1)             1. FL2+ (D7 via R2)"));
  Serial.println(F("  2. FL1- (GND)                   2. FL2- (GND)"));
  Serial.println(F("  3. J1 Pot Wiper (A0)            3. J2 Pot Wiper (A2)"));
  Serial.println(F("  4. J1 Pot VCC (5V)              4. J2 Pot VCC (5V)"));
  Serial.println(F("  5. J1 GND                       5. J2 GND"));
  Serial.println(F("  6. J1 Button (D2)               6. J2 Button (D3)"));
  Serial.println(F(""));
  Serial.println(F("RGB HEADER: 1=Red(D5) 2=Green(D9) 3=GND"));
  Serial.println(F(""));
  Serial.println(F("Board is ready for components."));
}

void loop() {
  // Blink onboard LED to show test is done
  static bool on = false;
  on = !on;
  // Pro Micro LED is on pin 17 (RXLED)
  pinMode(17, OUTPUT);
  digitalWrite(17, on ? LOW : HIGH);
  delay(1000);
}
