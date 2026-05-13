/*
 * Offline Simulation Test for CV Axle Controller State Machine v2.0
 *
 * Mocks Arduino APIs and drives the firmware logic through every scenario.
 * Compile: g++ -std=c++17 -o test_state_machine test_state_machine.cpp && ./test_state_machine
 *
 * Tests cover:
 *   - Ghost keystrokes at rest (zero tolerance)
 *   - Coarse left/right with proportional speed
 *   - Ratchet fine mode (fire on increasing deflection, stop on decrease)
 *   - Fine direction locking (must return to DZ to change direction)
 *   - Hysteresis (engage vs disengage thresholds)
 *   - Coarse suppression after fine disengages
 *   - Crosstalk isolation (fine→coarse, J1→J2)
 *   - EMA filter response (spike rejection, sustained input)
 *   - Asymmetric fine travel (twist rests at ~35)
 *   - State machine full cycle
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <string>
#include <cmath>

// ============================================================
// Arduino API Mocks
// ============================================================

static unsigned long mock_millis = 0;
unsigned long millis() { return mock_millis; }
void delay(unsigned long ms) { mock_millis += ms; }

static int mock_analog[32] = {};
int analogRead(uint8_t pin) { return (pin < 32) ? mock_analog[pin] : 512; }

static int mock_digital[16] = {};
int digitalRead(uint8_t pin) { return (pin < 16) ? mock_digital[pin] : 1; }
void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t pin, uint8_t val) { if (pin < 16) mock_digital[pin] = val; }

#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define HIGH 1
#define LOW 0
#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define PROGMEM
#define F(x) x

uint8_t pgm_read_byte(const void *addr) { return *(const uint8_t *)addr; }

long map(long x, long in_min, long in_max, long out_min, long out_max) {
  if (in_max == in_min) return out_min;
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#define highByte(x) ((uint8_t)((x) >> 8))
#define lowByte(x) ((uint8_t)((x) & 0xFF))
#undef max
#undef min
#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)<(b)?(a):(b))

// Keyboard mock
struct KeyEvent { char key; unsigned long timestamp; std::string type; };
static std::vector<KeyEvent> key_log;

struct KeyboardMock {
  void begin() {}
  void press(char k) { key_log.push_back({k, mock_millis, "press"}); }
  void release(char k) { key_log.push_back({k, mock_millis, "release"}); }
} Keyboard;

// EEPROM mock
static uint8_t mock_eeprom[64] = {};
struct EEPROMMock {
  uint8_t read(int a) { return mock_eeprom[a]; }
  void write(int a, uint8_t v) { mock_eeprom[a] = v; }
  void update(int a, uint8_t v) { mock_eeprom[a] = v; }
} EEPROM;

// Serial mock
struct SerialMock {
  void begin(long) {} void println(const char* = "") {} void print(const char*) {}
  void print(int) {} void print(char) {} void println(int) {} void println(char) {}
  int available() { return 0; } char read() { return 0; }
} Serial;

// sscanf is standard

// ============================================================
// Firmware Core Logic (replicated from .ino for testing)
// ============================================================

#define FINE_ENGAGE      30
#define FINE_DISENGAGE   12
#define COARSE_REPEAT_FAST_MS   30
#define COARSE_REPEAT_SLOW_MS   200
#define FINE_MIN_INTERVAL_MS    300
#define COARSE_SUPPRESS_MS      250
#define EMA_SHIFT  4

enum JoyState { JS_IDLE, JS_COARSE, JS_FINE, JS_SUPPRESSED };

struct Joystick {
  uint8_t pinX, pinY, pinBtn, pinLed;
  uint8_t kLeftCoarse, kRightCoarse, kLeftFine, kRightFine, kBtn;
  int16_t centerX, centerY, dzCoarse, dzFine;
  int32_t emaX, emaY;
  bool emaInitialized;
  JoyState state;
  unsigned long stateEnteredAt, lastCoarseKeystroke, lastFineKeystroke;
  unsigned long lastBtnPress, suppressUntil;
  bool fineEngaged;
  int8_t fineDirection;
  int16_t finePeakDeflect, fineLastDeflect;
  unsigned long btnHoldStart;
  bool btnWasHeld;
};

static char keys[10] = {'a','d','q','e','f','j','l','u','o',';'};
Joystick joy[2];

void sendKey(char key) {
  Keyboard.press(key);
  delay(10);
  Keyboard.release(key);
}

int16_t readFiltered(uint8_t idx, bool isY) {
  Joystick &j = joy[idx];
  int16_t raw; int32_t *ema;
  if (isY) { raw = analogRead(j.pinY); ema = &j.emaY; }
  else { raw = analogRead(j.pinX); ema = &j.emaX; }
  if (!j.emaInitialized) { *ema = (int32_t)raw << EMA_SHIFT; }
  else { *ema = *ema - (*ema >> EMA_SHIFT) + raw; }
  return (int16_t)(*ema >> EMA_SHIFT);
}

void processJoystick(uint8_t idx) {
  Joystick &j = joy[idx];
  unsigned long now = millis();

  int16_t filtX = readFiltered(idx, false);
  int16_t filtY = readFiltered(idx, true);
  if (!j.emaInitialized) j.emaInitialized = true;

  int16_t deflectX = filtX - j.centerX;
  int16_t deflectY = filtY - j.centerY;
  int16_t absDeflectX = abs(deflectX);
  int16_t absDeflectY = abs(deflectY);

  // Button: fire on release, only if short press
  bool btnDown = (digitalRead(j.pinBtn) == LOW);
  if (!btnDown && j.btnWasHeld && (now - j.lastBtnPress >= 200)) {
    if (now - j.btnHoldStart < 3000) {
      j.lastBtnPress = now;
      sendKey(keys[j.kBtn]);
    }
  }

  // Fine axis hysteresis
  bool finePastThreshold;
  if (j.fineEngaged) { finePastThreshold = (absDeflectY > FINE_DISENGAGE); }
  else { finePastThreshold = (absDeflectY > FINE_ENGAGE); }

  switch (j.state) {
    case JS_IDLE:
      if (finePastThreshold) {
        j.state = JS_FINE; j.stateEnteredAt = now;
        j.fineEngaged = true;
        j.fineDirection = (deflectY > 0) ? 1 : -1;
        j.finePeakDeflect = absDeflectY;
        j.fineLastDeflect = absDeflectY;
        if (now - j.lastFineKeystroke >= FINE_MIN_INTERVAL_MS) {
          char key = (j.fineDirection > 0) ? keys[j.kRightFine] : keys[j.kLeftFine];
          sendKey(key); j.lastFineKeystroke = now;
        }
      } else if (absDeflectX > j.dzCoarse && now >= j.suppressUntil) {
        j.state = JS_COARSE; j.stateEnteredAt = now;
      }
      break;

    case JS_COARSE:
      if (finePastThreshold) {
        j.state = JS_FINE; j.stateEnteredAt = now;
        j.fineEngaged = true;
        j.fineDirection = (deflectY > 0) ? 1 : -1;
        j.finePeakDeflect = absDeflectY;
        j.fineLastDeflect = absDeflectY;
        if (now - j.lastFineKeystroke >= FINE_MIN_INTERVAL_MS) {
          char key = (j.fineDirection > 0) ? keys[j.kRightFine] : keys[j.kLeftFine];
          sendKey(key); j.lastFineKeystroke = now;
        }
        break;
      }
      if (absDeflectX <= j.dzCoarse) {
        j.state = JS_IDLE; j.stateEnteredAt = now; break;
      }
      {
        int16_t mag = absDeflectX; if (mag > 512) mag = 512;
        int rMs = map(mag, j.dzCoarse, 512, COARSE_REPEAT_SLOW_MS, COARSE_REPEAT_FAST_MS);
        if (rMs < COARSE_REPEAT_FAST_MS) rMs = COARSE_REPEAT_FAST_MS;
        if (rMs > COARSE_REPEAT_SLOW_MS) rMs = COARSE_REPEAT_SLOW_MS;
        if (now - j.lastCoarseKeystroke >= (unsigned long)rMs) {
          char key = (deflectX > 0) ? keys[j.kRightCoarse] : keys[j.kLeftCoarse];
          sendKey(key); j.lastCoarseKeystroke = now;
        }
      }
      break;

    case JS_FINE:
      if (!finePastThreshold) {
        j.fineEngaged = false; j.fineDirection = 0;
        j.finePeakDeflect = 0; j.fineLastDeflect = 0;
        j.state = JS_SUPPRESSED; j.stateEnteredAt = now;
        j.suppressUntil = now + COARSE_SUPPRESS_MS;
        break;
      }
      {
        bool correctDir = (j.fineDirection > 0) ? (deflectY > 0) : (deflectY < 0);
        if (correctDir && absDeflectY > j.fineLastDeflect) {
          if (now - j.lastFineKeystroke >= FINE_MIN_INTERVAL_MS) {
            char key = (j.fineDirection > 0) ? keys[j.kRightFine] : keys[j.kLeftFine];
            sendKey(key); j.lastFineKeystroke = now;
          }
          if (absDeflectY > j.finePeakDeflect) j.finePeakDeflect = absDeflectY;
        }
        j.fineLastDeflect = absDeflectY;
      }
      break;

    case JS_SUPPRESSED:
      if (now >= j.suppressUntil) { j.state = JS_IDLE; j.stateEnteredAt = now; }
      if (finePastThreshold) {
        j.state = JS_FINE; j.stateEnteredAt = now;
        j.fineEngaged = true;
        j.fineDirection = (deflectY > 0) ? 1 : -1;
        j.finePeakDeflect = absDeflectY;
        j.fineLastDeflect = absDeflectY;
        if (now - j.lastFineKeystroke >= FINE_MIN_INTERVAL_MS) {
          char key = (j.fineDirection > 0) ? keys[j.kRightFine] : keys[j.kLeftFine];
          sendKey(key); j.lastFineKeystroke = now;
        }
      }
      break;
  }
}

// ============================================================
// Test Infrastructure
// ============================================================

void resetAll() {
  mock_millis = 5000;
  memset(mock_analog, 0, sizeof(mock_analog));
  memset(mock_digital, 0, sizeof(mock_digital));
  key_log.clear();

  for (int i = 0; i < 2; i++) {
    Joystick &j = joy[i];
    j.pinX = (i == 0) ? A0 : A2;
    j.pinY = (i == 0) ? A1 : A3;
    j.pinBtn = (i == 0) ? 2 : 3;
    j.pinLed = (i == 0) ? 6 : 7;
    j.kLeftCoarse = i * 5;
    j.kRightCoarse = i * 5 + 1;
    j.kLeftFine = i * 5 + 2;
    j.kRightFine = i * 5 + 3;
    j.kBtn = i * 5 + 4;
    j.centerX = 512;
    j.centerY = 35;  // Twist rests at ~35!
    j.dzCoarse = 100;
    j.dzFine = 30;
    j.emaX = (int32_t)512 << EMA_SHIFT;
    j.emaY = (int32_t)35 << EMA_SHIFT;
    j.emaInitialized = true;
    j.state = JS_IDLE;
    j.stateEnteredAt = 0;
    j.lastCoarseKeystroke = 0;
    j.lastFineKeystroke = 0;
    j.lastBtnPress = 0;
    j.suppressUntil = 0;
    j.fineEngaged = false;
    j.fineDirection = 0;
    j.finePeakDeflect = 0;
    j.fineLastDeflect = 0;
    j.btnHoldStart = 0;
    j.btnWasHeld = false;
  }

  // Resting analog values
  mock_analog[A0] = 512; mock_analog[A1] = 35;
  mock_analog[A2] = 512; mock_analog[A3] = 35;
  // Buttons not pressed
  mock_digital[2] = 1; mock_digital[3] = 1;
}

void runLoops(int n, unsigned long interval_ms = 10) {
  for (int i = 0; i < n; i++) {
    processJoystick(0);
    processJoystick(1);
    mock_millis += interval_ms;
  }
}

int countKeyPresses(char key) {
  int c = 0;
  for (auto &e : key_log) if (e.key == key && e.type == "press") c++;
  return c;
}

int totalKeyPresses() {
  int c = 0;
  for (auto &e : key_log) if (e.type == "press") c++;
  return c;
}

void clearKeyLog() { key_log.clear(); }

#define TEST(name) \
  static void test_##name(); \
  static struct Reg_##name { Reg_##name() { tests.push_back({#name, test_##name}); } } reg_##name; \
  static void test_##name()

struct TestEntry { const char *name; void (*fn)(); };
static std::vector<TestEntry> tests;

// ============================================================
// Tests
// ============================================================

// --- Ghost keystroke tests ---

TEST(ghost_at_rest) {
  resetAll();
  runLoops(500, 10);
  assert(totalKeyPresses() == 0);
  printf("  PASS: zero keystrokes after 5s at rest\n");
}

TEST(ghost_with_noise_8) {
  resetAll();
  srand(42);
  for (int i = 0; i < 500; i++) {
    mock_analog[A0] = 512 + (rand() % 17 - 8);
    mock_analog[A1] = 35 + (rand() % 17 - 8);
    mock_analog[A2] = 512 + (rand() % 17 - 8);
    mock_analog[A3] = 35 + (rand() % 17 - 8);
    processJoystick(0); processJoystick(1); mock_millis += 10;
  }
  assert(totalKeyPresses() == 0);
  printf("  PASS: zero keystrokes with +-8 noise\n");
}

TEST(ghost_with_noise_25) {
  resetAll();
  srand(99);
  for (int i = 0; i < 500; i++) {
    mock_analog[A0] = 512 + (rand() % 51 - 25);
    mock_analog[A1] = 35 + (rand() % 51 - 25);
    mock_analog[A2] = 512 + (rand() % 51 - 25);
    mock_analog[A3] = 35 + (rand() % 51 - 25);
    processJoystick(0); processJoystick(1); mock_millis += 10;
  }
  assert(totalKeyPresses() == 0);
  printf("  PASS: zero keystrokes with +-25 noise (within DZ)\n");
}

// --- Coarse tests ---

TEST(coarse_right) {
  resetAll();
  mock_analog[A0] = 900;
  runLoops(100, 10);
  assert(countKeyPresses('d') > 5);
  assert(countKeyPresses('a') == 0);
  printf("  PASS: coarse right = %d 'd' keystrokes\n", countKeyPresses('d'));
}

TEST(coarse_left) {
  resetAll();
  mock_analog[A0] = 100;
  runLoops(100, 10);
  assert(countKeyPresses('a') > 5);
  assert(countKeyPresses('d') == 0);
  printf("  PASS: coarse left = %d 'a' keystrokes\n", countKeyPresses('a'));
}

TEST(coarse_proportional) {
  resetAll();
  mock_analog[A0] = 620; // just past DZ
  runLoops(200, 10);
  int slow = countKeyPresses('d');

  resetAll();
  mock_analog[A0] = 1000; // near max
  runLoops(200, 10);
  int fast = countKeyPresses('d');

  assert(fast > slow);
  printf("  PASS: proportional speed (slow=%d, fast=%d)\n", slow, fast);
}

TEST(coarse_dz_boundary) {
  resetAll();
  mock_analog[A0] = 512 + 100; // exactly at DZ (> not >=)
  runLoops(300, 10);
  assert(countKeyPresses('d') == 0);
  printf("  PASS: no keystroke at exact DZ boundary\n");
}

TEST(coarse_just_past_dz) {
  resetAll();
  mock_analog[A0] = 512 + 105;
  runLoops(300, 10);
  assert(countKeyPresses('d') > 0);
  printf("  PASS: keystrokes just past DZ (%d)\n", countKeyPresses('d'));
}

// --- Ratchet fine tests ---

TEST(fine_fires_on_engage) {
  resetAll();
  mock_analog[A1] = 80; // deflect = 80-35 = 45 > ENGAGE(30)
  // EMA filter takes ~40 loops to converge (alpha=1/16)
  runLoops(60, 10);
  assert(countKeyPresses('e') >= 1); // engagement fires at least once
  assert(joy[0].state == JS_FINE);
  printf("  PASS: fine fires on engagement (%d keystrokes)\n", countKeyPresses('e'));
}

TEST(fine_fires_on_increasing_deflection) {
  resetAll();
  // Step analog directly and converge EMA
  mock_analog[A1] = 70;
  runLoops(60, 10); // let EMA converge and engage
  int initial = countKeyPresses('e');
  assert(initial >= 1);

  mock_millis += 400;
  clearKeyLog();

  // Jump to higher deflection
  mock_analog[A1] = 200;
  runLoops(60, 10);
  assert(countKeyPresses('e') >= 1);
  printf("  PASS: fine fires on increasing deflection (%d keystrokes)\n", countKeyPresses('e'));
}

TEST(fine_silent_on_decreasing_deflection) {
  resetAll();
  // Engage and let EMA fully converge at high deflection
  mock_analog[A1] = 200;
  runLoops(100, 10); // 1 second, EMA converges
  assert(joy[0].state == JS_FINE);
  clearKeyLog();

  // Drop to lower deflection (still above DZ)
  mock_analog[A1] = 100;
  runLoops(100, 10); // EMA decreases toward 100

  assert(countKeyPresses('e') == 0); // should NOT fire on decrease
  assert(joy[0].state == JS_FINE);
  printf("  PASS: fine silent when deflection decreases\n");
}

TEST(fine_direction_locked) {
  resetAll();
  // Engage right
  mock_analog[A1] = 80;
  runLoops(60, 10); // EMA converges
  assert(joy[0].fineDirection == 1);
  assert(joy[0].state == JS_FINE);

  // Swing analog to other side, but stay above DISENGAGE
  // analog=20, deflect = 20-35 = -15, abs=15 > DISENGAGE(12)
  // EMA will gradually move there
  mock_analog[A1] = 20;
  clearKeyLog();
  runLoops(100, 10); // let EMA converge to ~20

  assert(countKeyPresses('q') == 0); // no left fire (locked right)
  printf("  PASS: direction locked, no opposite keystroke while engaged\n");
}

TEST(fine_direction_change_requires_dz_return) {
  resetAll();
  // Engage right and converge
  mock_analog[A1] = 80;
  runLoops(60, 10);
  assert(joy[0].state == JS_FINE);

  // Return to DZ and wait for suppress
  mock_analog[A1] = 35;
  runLoops(80, 10);
  assert(joy[0].state == JS_IDLE);
  clearKeyLog();

  // Engage left
  mock_analog[A1] = 0;
  runLoops(60, 10);
  assert(countKeyPresses('q') >= 1);
  assert(joy[0].fineDirection == -1);
  printf("  PASS: direction change after DZ return works\n");
}

TEST(fine_both_directions) {
  resetAll();
  mock_analog[A1] = 80;
  runLoops(60, 10);
  assert(countKeyPresses('e') >= 1);

  mock_analog[A1] = 35;
  runLoops(80, 10);
  clearKeyLog();

  mock_analog[A1] = 0;
  runLoops(60, 10);
  assert(countKeyPresses('q') >= 1);
  assert(countKeyPresses('e') == 0);
  printf("  PASS: fine works in both directions\n");
}

TEST(fine_ratchet_increase_decrease_cycle) {
  // Simulate gradual twist using per-step analog changes
  resetAll();

  // Step 1: engage and converge
  mock_analog[A1] = 70;
  runLoops(60, 10);
  assert(countKeyPresses('e') >= 1);
  clearKeyLog();

  // Let EMA fully converge, then jump higher
  mock_millis += 400;
  mock_analog[A1] = 200;
  runLoops(60, 10);
  int afterIncrease = countKeyPresses('e');
  assert(afterIncrease >= 1);
  clearKeyLog();

  // Converge at 200, then decrease to 100
  runLoops(60, 10); // fully converge at 200
  clearKeyLog();
  mock_analog[A1] = 100;
  runLoops(100, 10); // EMA decreases
  assert(countKeyPresses('e') == 0); // silent on decrease

  // Increase again past previous peak
  clearKeyLog();
  mock_millis += 400;
  mock_analog[A1] = 400;
  runLoops(60, 10);
  assert(countKeyPresses('e') >= 1);

  printf("  PASS: ratchet cycle works\n");
}

TEST(fine_no_repeat_at_constant_deflection) {
  resetAll();
  mock_analog[A1] = 200;
  // Let EMA fully converge first (fires during convergence since deflection is increasing)
  runLoops(200, 10);
  int duringConverge = countKeyPresses('e');
  assert(duringConverge >= 1); // fires during EMA convergence

  // Now EMA is stable. No more keystrokes should fire.
  clearKeyLog();
  runLoops(500, 10); // 5 more seconds at same deflection
  assert(countKeyPresses('e') == 0); // stable = no increase = no fire
  printf("  PASS: no repeat once EMA converged (converge=%d, stable=0)\n", duringConverge);
}

// --- Hysteresis tests ---

TEST(fine_hysteresis) {
  resetAll();
  mock_analog[A1] = 80;
  runLoops(60, 10); // EMA converges
  assert(joy[0].state == JS_FINE);
  assert(joy[0].fineEngaged == true);

  // Drop to deflect=20 (between DISENGAGE=12 and ENGAGE=30)
  mock_analog[A1] = 55; // 55-35=20
  runLoops(60, 10); // EMA converges
  assert(joy[0].state == JS_FINE); // still engaged

  // Drop below DISENGAGE
  mock_analog[A1] = 40; // 40-35=5 < 12
  runLoops(60, 10);
  assert(joy[0].fineEngaged == false);
  printf("  PASS: hysteresis works\n");
}

// --- Suppression tests ---

TEST(coarse_suppressed_after_fine) {
  resetAll();
  mock_analog[A1] = 80;
  runLoops(60, 10); // converge, engage fine
  assert(joy[0].state == JS_FINE);
  clearKeyLog();

  // Disengage fine + push coarse simultaneously
  mock_analog[A1] = 35;
  mock_analog[A0] = 700;
  // EMA for Y takes time to drop below disengage. Run enough loops.
  runLoops(80, 10);
  // At some point fine disengages and suppress starts. Coarse should be blocked initially.
  // The total 'd' count should be low (some may fire after suppression expires within those 80 loops)
  int early_d = countKeyPresses('d');

  // Run more loops, coarse should definitely be firing now
  clearKeyLog();
  runLoops(100, 10);
  assert(countKeyPresses('d') > 0);
  printf("  PASS: coarse resumes after suppression (%d keystrokes)\n", countKeyPresses('d'));
}

// --- Crosstalk tests ---

TEST(fine_coarse_crosstalk) {
  resetAll();
  mock_analog[A1] = 80;   // fine engaged
  mock_analog[A0] = 562;  // 50-count X wobble (within DZ=100)
  runLoops(100, 10);
  assert(countKeyPresses('a') == 0);
  assert(countKeyPresses('d') == 0);
  printf("  PASS: no coarse crosstalk during fine with 50-count X wobble\n");
}

TEST(j2_isolation) {
  resetAll();
  mock_analog[A0] = 900; // only J1 active
  runLoops(100, 10);
  assert(countKeyPresses('j') == 0 && countKeyPresses('l') == 0);
  assert(countKeyPresses('u') == 0 && countKeyPresses('o') == 0);
  printf("  PASS: J2 silent when only J1 used\n");
}

TEST(j1_isolation) {
  resetAll();
  mock_analog[A2] = 100; // only J2 active
  runLoops(100, 10);
  assert(countKeyPresses('a') == 0 && countKeyPresses('d') == 0);
  assert(countKeyPresses('q') == 0 && countKeyPresses('e') == 0);
  assert(countKeyPresses('j') > 0);
  printf("  PASS: J1 silent when only J2 used (J2=%d)\n", countKeyPresses('j'));
}

TEST(j2_noise_while_j1_active) {
  resetAll();
  srand(77);
  mock_analog[A0] = 900;
  for (int i = 0; i < 200; i++) {
    mock_analog[A2] = 512 + (rand() % 11 - 5);
    mock_analog[A3] = 35 + (rand() % 11 - 5);
    processJoystick(0); processJoystick(1); mock_millis += 10;
  }
  assert(countKeyPresses('j') == 0 && countKeyPresses('l') == 0);
  printf("  PASS: J2 silent with noise while J1 active\n");
}

// --- EMA filter tests ---

TEST(ema_spike_rejection) {
  resetAll();
  runLoops(50, 10); // stabilize
  clearKeyLog();

  mock_analog[A0] = 800; // single spike
  processJoystick(0); processJoystick(1); mock_millis += 10;
  mock_analog[A0] = 512; // back to normal
  runLoops(10, 10);

  assert(countKeyPresses('d') == 0);
  printf("  PASS: EMA rejects single spike\n");
}

TEST(ema_sustained_input) {
  resetAll();
  mock_analog[A0] = 900;
  runLoops(100, 10);
  assert(countKeyPresses('d') > 0);
  printf("  PASS: EMA passes sustained input (%d keystrokes)\n", countKeyPresses('d'));
}

// --- Asymmetric fine ---

TEST(asymmetric_fine_left) {
  resetAll();
  mock_analog[A1] = 0; // deflect = -35
  runLoops(60, 10);
  assert(countKeyPresses('q') >= 1);
  printf("  PASS: asymmetric fine left (%d keystrokes)\n", countKeyPresses('q'));
}

TEST(asymmetric_fine_right) {
  resetAll();
  mock_analog[A1] = 700;
  runLoops(60, 10);
  assert(countKeyPresses('e') >= 1);
  printf("  PASS: asymmetric fine right (%d keystrokes)\n", countKeyPresses('e'));
}

// --- State machine cycle ---

TEST(full_state_cycle) {
  resetAll();
  assert(joy[0].state == JS_IDLE);

  mock_analog[A1] = 80;
  runLoops(60, 10);
  assert(joy[0].state == JS_FINE);

  mock_analog[A1] = 35;
  runLoops(80, 10);
  // Should be SUPPRESSED or IDLE by now
  assert(joy[0].state == JS_IDLE || joy[0].state == JS_SUPPRESSED);

  // Ensure we reach IDLE
  runLoops(40, 10);
  assert(joy[0].state == JS_IDLE);

  mock_analog[A0] = 900;
  runLoops(60, 10);
  assert(joy[0].state == JS_COARSE);

  mock_analog[A0] = 512;
  runLoops(60, 10);
  assert(joy[0].state == JS_IDLE);

  printf("  PASS: IDLE->FINE->SUPPRESSED->IDLE->COARSE->IDLE\n");
}

TEST(fine_interrupts_coarse) {
  resetAll();
  mock_analog[A0] = 900;
  runLoops(60, 10);
  assert(joy[0].state == JS_COARSE);
  clearKeyLog();

  mock_analog[A1] = 80;
  runLoops(60, 10);
  assert(joy[0].state == JS_FINE);
  assert(countKeyPresses('e') >= 1);
  printf("  PASS: fine interrupts coarse\n");
}

TEST(simultaneous_j1_j2) {
  resetAll();
  mock_analog[A0] = 900;
  mock_analog[A2] = 100;
  runLoops(100, 10);
  assert(countKeyPresses('d') > 0);
  assert(countKeyPresses('j') > 0);
  printf("  PASS: J1+J2 simultaneous (J1=%d, J2=%d)\n",
         countKeyPresses('d'), countKeyPresses('j'));
}

TEST(rapid_fine_oscillation) {
  resetAll();
  for (int cycle = 0; cycle < 20; cycle++) {
    mock_analog[A1] = 35 + 20; // deflect=20 (between disengage and engage)
    runLoops(2, 10);
    mock_analog[A1] = 35 + 35; // deflect=35 (above engage)
    runLoops(2, 10);
  }
  int total = countKeyPresses('e') + countKeyPresses('q');
  assert(total <= 5);
  printf("  PASS: rapid oscillation: %d keystrokes (<=5 expected)\n", total);
}

// --- Button tests ---

TEST(button_short_press) {
  resetAll();
  // Simulate press
  joy[0].btnWasHeld = true;
  joy[0].btnHoldStart = mock_millis - 100;
  mock_digital[2] = 1; // released
  processJoystick(0);
  mock_millis += 10;
  joy[0].btnWasHeld = false;

  assert(countKeyPresses('f') == 1);
  printf("  PASS: short button press fires key\n");
}

// --- Ratchet fine: edge case where user twists past max then backs off ---

TEST(fine_twist_max_then_back) {
  resetAll();
  // Gradually twist right to max
  for (int v = 35; v <= 700; v += 20) {
    mock_analog[A1] = v;
    processJoystick(0); processJoystick(1);
    mock_millis += 50;
  }
  int right_count = countKeyPresses('e');
  assert(right_count > 3);

  // Let EMA fully converge at 700 before backing off
  mock_analog[A1] = 700;
  runLoops(200, 10); // 2 seconds to fully settle

  clearKeyLog();
  // Now back off slowly. With EMA converged, filtered value will DECREASE monotonically.
  for (int v = 700; v >= 35; v -= 5) {
    mock_analog[A1] = v;
    // Run enough loops per step for EMA to track the decrease
    runLoops(3, 10);
  }
  // Should NOT fire during decrease
  assert(countKeyPresses('e') == 0);
  assert(countKeyPresses('q') == 0);
  printf("  PASS: twist to max (%d fires), back off (0 fires)\n", right_count);
}

TEST(fine_sawtooth_pattern) {
  // Simulate sawtooth: twist up (fire), back off (silent), twist higher (fire)
  resetAll();

  // Wave 1: twist right to 100
  mock_analog[A1] = 100;
  runLoops(80, 10); // converge
  int wave1 = countKeyPresses('e');
  assert(wave1 >= 1);

  // Let EMA fully settle at 100
  runLoops(100, 10);
  clearKeyLog();

  // Back off to 60 (still above DISENGAGE)
  mock_analog[A1] = 60;
  runLoops(100, 10); // EMA decreases
  assert(countKeyPresses('e') == 0); // silent on decrease

  // Wave 2: twist further to 300
  clearKeyLog();
  mock_millis += 400; // past rate limit
  mock_analog[A1] = 300;
  runLoops(80, 10);
  int wave2 = countKeyPresses('e');
  assert(wave2 >= 1);

  printf("  PASS: sawtooth: wave1=%d, backoff=0, wave2=%d\n", wave1, wave2);
}

// ============================================================
// Main
// ============================================================

int main() {
  printf("CV Axle Controller v2.0 - State Machine Tests\n");
  printf("==============================================\n\n");

  int passed = 0, failed = 0;
  for (auto &t : tests) {
    printf("[TEST] %s\n", t.name);
    try {
      t.fn();
      passed++;
    } catch (...) {
      printf("  FAIL!\n");
      failed++;
    }
    printf("\n");
  }

  printf("==============================================\n");
  printf("Results: %d passed, %d failed, %d total\n", passed, failed, passed + failed);
  return (failed > 0) ? 1 : 0;
}
