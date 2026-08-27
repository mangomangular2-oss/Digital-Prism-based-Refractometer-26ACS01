/* =====================================================================
   Portable prism refractometer  --  DIRECT DRIVE version
   Prism mounted straight onto the 28BYJ-48 output shaft (no gear stage).

   Mode 1 (rocker HIGH): single refractive-index measurement
   Mode 2 (rocker LOW) : calibration + concentration measurement

   Libraries (Library Manager):
     "LiquidCrystal I2C" by Frank de Brabander
     "Keypad" by Mark Stanley / Alexander Brevig
   ===================================================================== */

#include <Arduino.h>
#include <math.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

/* --------------------------- pin map --------------------------------
   D0,D1   Serial
   D2      MEASURE button   (INPUT_PULLUP, pressed = LOW)
   D3      mode rocker      (INPUT_PULLUP, LOW = concentration mode)
   D4-D7   keypad rows
   D8-D11  stepper coils
   D12,D13 keypad cols 0,1
   A0      LDR
   A1      keypad col 2
   A2      FREE
   A3      FREE  <- reserve for DS18B20 temperature sensor
   A4,A5   I2C (LCD)
   -------------------------------------------------------------------- */

#define COIL1        8
#define COIL2        9
#define COIL3       10
#define COIL4       11
#define LDR_PIN     A0
#define SWITCH_PIN   3
#define BUTTON_PIN   2

LiquidCrystal_I2C lcd(0x27, 16, 2);   // try 0x3F if the screen stays blank

const byte ROWS = 4, COLS = 3;
char keyMap[ROWS][COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};
byte rowPins[ROWS] = { 4, 5, 6, 7 };
byte colPins[COLS] = { 12, 13, A1 };
Keypad keypad = Keypad(makeKeymap(keyMap), rowPins, colPins, ROWS, COLS);

/* ------------------------ mechanical constants ----------------------
   DIRECT DRIVE: prism angle == motor output shaft angle.

   The 28BYJ-48's internal reduction is 63.68395:1, NOT 64:1, so a full
   output revolution is 32 * 2 * 63.68395 = 4075.77 half-steps, not 4096.
   VERIFY THIS ON YOUR UNIT: command 10 revolutions and see how far a mark
   on the prism drifts, then adjust.
   -------------------------------------------------------------------- */
const float STEPS_PER_REV     = 4076.0;
const float DEG_PER_STEP      = 360.0 / STEPS_PER_REV;      // ~0.08832 deg
const long  STEPS_PER_REV_L   = 4076L;
const long  MAX_TOTAL_STEPS   = 2L * STEPS_PER_REV_L;       // "two turns" budget

const unsigned int STEP_DELAY_US = 1500;
const float N_PRISM        = 1.500;    // measure/verify this
const float PRISM_APEX_DEG = 45.0;

/* ------------------------ optical constants ------------------------- */
const int  RISE_MARGIN     = 60;    // counts above dark that count as "light"
const int  MIN_CONTRAST    = 80;
const int  COARSE_STEP     = 4;
const int  BACKUP_STEPS    = 16;    // one back-off increment in stage 4b
const long MAX_BACKUP      = 400L;  // give up if the cell is THIS slow
const long SURVEY_LIMIT    = 120L;  // how far to look for a dark reference
const int  FINE_BACKUP     = 24;    // small, bounded run-up to the profiled edge
const int  LDR_RECOVER_MS  = 350;   // full settle after a reversal
const int  LDR_SETTLE_MS   = 60;    // settle per step during the fine creep
const byte PROFILE_MAX     = 48;    // samples recorded across the edge

int profile[PROFILE_MAX];

/* ------------------------ calibration store ------------------------- */
const byte MAX_CAL = 8;
const byte MIN_CAL = 3;

struct CalPoint { float conc; float n; };
CalPoint cal[MAX_CAL];
byte  calCount = 0;
float fitSlope = 0, fitIntercept = 0, fitR2 = 0, fitMaxErr = 0;
bool  fitValid = false;

/* ------------------------ motion state ------------------------------
   position is PURE physical bookkeeping: it tracks the rotor, one count
   per half-step, and is never negated or reset mid-motion. Travel is
   always computed as a difference against homePos.
   -------------------------------------------------------------------- */
long position     = 0;
long homePos      = 0;
long stepBudget   = 0;
bool budgetBlown  = false;

/* ------------------------ UI state ---------------------------------- */
enum AppState {
  ST_SINGLE_IDLE, ST_SINGLE_RESULT,
  ST_CAL_INPUT,   ST_CAL_MENU,    ST_CAL_PROMPT,  ST_CAL_RESULT,
  ST_TEST_IDLE,   ST_TEST_RESULT
};
AppState state = ST_SINGLE_IDLE;
bool concMode = false;

char  entryBuf[9];
byte  entryLen = 0;
float pendingConc = 0;

int lastButton = HIGH;
unsigned long lastButtonMs = 0;
const unsigned long DEBOUNCE_MS = 40;

enum MeasResult {
  M_OK, M_ERR_NO_DARK, M_ERR_NO_BRIGHT, M_ERR_NO_EDGE, M_ERR_RANGE, M_ERR_BUDGET
};

/* ===================================================================== */
/*  Low level                                                            */
/* ===================================================================== */

void Switch_function(int x) {
  switch (x & 7) {
    case 0: digitalWrite(COIL1, LOW);  digitalWrite(COIL2, LOW);  digitalWrite(COIL3, LOW);  digitalWrite(COIL4, HIGH); break;
    case 1: digitalWrite(COIL1, LOW);  digitalWrite(COIL2, LOW);  digitalWrite(COIL3, HIGH); digitalWrite(COIL4, HIGH); break;
    case 2: digitalWrite(COIL1, LOW);  digitalWrite(COIL2, LOW);  digitalWrite(COIL3, HIGH); digitalWrite(COIL4, LOW);  break;
    case 3: digitalWrite(COIL1, LOW);  digitalWrite(COIL2, HIGH); digitalWrite(COIL3, HIGH); digitalWrite(COIL4, LOW);  break;
    case 4: digitalWrite(COIL1, LOW);  digitalWrite(COIL2, HIGH); digitalWrite(COIL3, LOW);  digitalWrite(COIL4, LOW);  break;
    case 5: digitalWrite(COIL1, HIGH); digitalWrite(COIL2, HIGH); digitalWrite(COIL3, LOW);  digitalWrite(COIL4, LOW);  break;
    case 6: digitalWrite(COIL1, HIGH); digitalWrite(COIL2, LOW);  digitalWrite(COIL3, LOW);  digitalWrite(COIL4, LOW);  break;
    case 7: digitalWrite(COIL1, HIGH); digitalWrite(COIL2, LOW);  digitalWrite(COIL3, LOW);  digitalWrite(COIL4, HIGH); break;
  }
}

void coilsOff() {
  digitalWrite(COIL1, LOW); digitalWrite(COIL2, LOW);
  digitalWrite(COIL3, LOW); digitalWrite(COIL4, LOW);
}

/* Positive argument -> position decreases. Kept from the original sketch so
   "forward" still means the direction that sweeps toward loss of TIR.
   The step budget is enforced HERE, so no loop anywhere can run away.     */
void stepper(long steps) {
  long dir = (steps >= 0) ? -1 : 1;
  long n   = (steps >= 0) ?  steps : -steps;
  for (long i = 0; i < n; i++) {
    if (stepBudget <= 0) { budgetBlown = true; return; }
    stepBudget--;
    position += dir;
    Switch_function((int)(((position % 8) + 8) % 8));
    delayMicroseconds(STEP_DELAY_US);
  }
}

/* stepper(s) changes position by -s, so s = position - target */
void moveTo(long target) { stepper(position - target); }

/* Park at home unconditionally, even after the budget has been spent. */
void goHome() {
  stepBudget = MAX_TOTAL_STEPS * 2;
  moveTo(homePos);
  coilsOff();
}

int readLDR() {
  long s = 0;
  for (byte i = 0; i < 8; i++) { s += analogRead(LDR_PIN); delayMicroseconds(200); }
  return (int)(s >> 3);
}

/* ===================================================================== */
/*  Sub-step edge location                                               */
/*  Fits a line to the 20-80% portion of the falling edge and solves for  */
/*  the midpoint crossing. Returns a FRACTIONAL index into profile[].     */
/* ===================================================================== */

float findEdgeIndex(byte n, int dark, int bright, int threshold, bool *ok) {
  *ok = false;
  if (n < 2) return 0;

  float contrast = (float)(bright - dark);
  int hi = dark + (int)(0.80 * contrast);
  int lo = dark + (int)(0.20 * contrast);

  float sx = 0, sy = 0, sxx = 0, sxy = 0;
  byte m = 0;
  for (byte i = 0; i < n; i++) {
    if (profile[i] >= lo && profile[i] <= hi) {
      sx  += i;
      sy  += profile[i];
      sxx += (float)i * i;
      sxy += (float)i * profile[i];
      m++;
    }
  }

  if (m >= 3) {
    float d = (float)m * sxx - sx * sx;
    if (fabs(d) > 1e-6) {
      float a = ((float)m * sxy - sx * sy) / d;      // slope, counts per step
      float b = (sy - a * sx) / m;
      if (a < -0.5) {                                // must actually be falling
        *ok = true;
        return (threshold - b) / a;
      }
    }
  }

  /* Fallback: straight interpolation between the two bracketing samples. */
  for (byte i = 1; i < n; i++) {
    if (profile[i - 1] >= threshold && profile[i] < threshold) {
      float den = (float)(profile[i - 1] - profile[i]);
      *ok = true;
      if (den > 0.5) return (float)(i - 1) + (float)(profile[i - 1] - threshold) / den;
      return (float)i;
    }
  }
  return 0;
}

/* ===================================================================== */
/*  Measurement                                                          */
/* ===================================================================== */

MeasResult measureRefractive(float *nOut, float *angleOut) {
  goHome();
  delay(200);

  stepBudget  = MAX_TOTAL_STEPS;      // safeguard: two output revolutions
  budgetBlown = false;

  /* 1. Learn the dark reference. Back off (negative direction) until the
        reading stops falling. This works whether we start inside the TIR
        band or outside it, and it means every approach to the edge comes
        from the same side so backlash is taken up identically.
        No absolute light level is assumed anywhere.                       */
  int  v0 = readLDR();
  int  darkLevel = v0, maxSeen = v0;
  long back = 0;
  bool sawFall = false;
  int  flat = 0;
  while (back < MAX_BACKUP && !budgetBlown) {
    stepper(-COARSE_STEP);
    back += COARSE_STEP;
    delay(LDR_SETTLE_MS);
    int v = readLDR();
    if (v > maxSeen)   maxSeen   = v;
    if (v < darkLevel) darkLevel = v;
    if (maxSeen - darkLevel >= MIN_CONTRAST) sawFall = true;
    if (v <= darkLevel + RISE_MARGIN / 2) flat++; else flat = 0;
    if (sawFall && flat >= 3)               break;   // clearly out of the band
    if (!sawFall && back >= SURVEY_LIMIT)   break;   // looks like we began dark
  }
  if (budgetBlown) { goHome(); return M_ERR_NO_DARK; }
  delay(LDR_RECOVER_MS);
  if (readLDR() < darkLevel) darkLevel = readLDR();

  /* 2. Coarse sweep forward until the light rises clearly above dark.
        The trip point is relative, so it works at any LED brightness.     */
  int trip = darkLevel + RISE_MARGIN;
  while (readLDR() < trip && !budgetBlown) stepper(COARSE_STEP);
  if (budgetBlown) { goHome(); return M_ERR_NO_BRIGHT; }

  /* 3. Move deeper into the band and let the cell settle, so brightLevel
        is the true plateau rather than a lagging value.                   */
  stepper(COARSE_STEP * 3);
  delay(LDR_RECOVER_MS);
  int brightLevel = readLDR();
  if (brightLevel - darkLevel < MIN_CONTRAST) { goHome(); return M_ERR_NO_EDGE; }
  int threshold = (brightLevel + darkLevel) / 2;

  /* 4a. Fast coarse sweep to the far edge. The LDR lags at this speed, so
         this only gets us close.                                          */
  while (readLDR() >= threshold && !budgetBlown) stepper(COARSE_STEP);
  if (budgetBlown) { goHome(); return M_ERR_NO_EDGE; }

  /* 4b. Reverse back into the band until the cell genuinely reads bright
         again. A slow photocell lags tens of steps behind at coarse speed,
         so this MUST be a loop -- a fixed back-off silently fails.        */
  long backed = 0;
  while (backed < MAX_BACKUP && !budgetBlown) {
    stepper(-BACKUP_STEPS);
    backed += BACKUP_STEPS;
    delay(LDR_RECOVER_MS);
    if (readLDR() >= threshold) break;
  }
  if (readLDR() < threshold || budgetBlown) { goHome(); return M_ERR_NO_EDGE; }

  /* 4c. Settled coarse approach. Waiting at every step removes the lag, so
         this ends within COARSE_STEP of the true edge.                     */
  long guard4 = 0;
  bool nearEdge = false;
  while (guard4 < backed + 6L * COARSE_STEP && !budgetBlown) {
    stepper(COARSE_STEP);
    guard4 += COARSE_STEP;
    delay(LDR_SETTLE_MS);
    if (readLDR() < threshold) { nearEdge = true; break; }
  }
  if (!nearEdge || budgetBlown) { goHome(); return budgetBlown ? M_ERR_BUDGET : M_ERR_NO_EDGE; }

  /* 4d. Small bounded run-up so the profile always fits in PROFILE_MAX. */
  long fineBack = FINE_BACKUP;
  stepper(-fineBack);
  delay(LDR_RECOVER_MS);
  while (readLDR() < threshold && fineBack < (long)PROFILE_MAX - 10 && !budgetBlown) {
    stepper(-8); fineBack += 8; delay(LDR_RECOVER_MS);
  }
  if (readLDR() < threshold || budgetBlown) { goHome(); return M_ERR_NO_EDGE; }

  /* 4c. Creep forward one step at a time, recording the whole profile.
         profile[i] is taken at position (creepStart - i).                 */
  long creepStart = position;
  int  lowCut     = darkLevel + (brightLevel - darkLevel) / 5;   // 20%
  byte np = 0;
  bool crossed = false;

  while (np < PROFILE_MAX) {
    if (np > 0) { stepper(1); if (budgetBlown) break; }
    delay(LDR_SETTLE_MS);
    profile[np] = readLDR();
    np++;
    if (profile[np - 1] < lowCut) { crossed = true; break; }
  }
  if (!crossed || budgetBlown) { goHome(); return budgetBlown ? M_ERR_BUDGET : M_ERR_NO_EDGE; }

  /* Dump the profile so you can check how many steps the transition spans.
     If it spans 1 step, interpolation buys you nothing and you need gears. */
  Serial.print(F("profile:"));
  for (byte i = 0; i < np; i++) { Serial.print(' '); Serial.print(profile[i]); }
  Serial.println();

  bool edgeOk;
  float idx = findEdgeIndex(np, darkLevel, brightLevel, threshold, &edgeOk);
  if (!edgeOk) { goHome(); return M_ERR_NO_EDGE; }

  /* 5. Geometry, in fractional steps. */
  float edgePos    = (float)creepStart - idx;
  float sweepSteps = (float)homePos - edgePos;              // positive
  sweepSteps = fmod(sweepSteps, STEPS_PER_REV);
  if (sweepSteps < 0) sweepSteps += STEPS_PER_REV;

  float bigAngle = sweepSteps * DEG_PER_STEP;               // 0 .. 360 deg
  float incident = 90.0 - bigAngle;
  int   caseNum  = (incident < 0) ? 1 : 2;
  incident = fabs(incident);

  float s = sin(incident * DEG_TO_RAD) / N_PRISM;
  if (s > 1.0 || s < -1.0) { goHome(); return M_ERR_RANGE; }

  float thetaR = asin(s);
  float apex   = PRISM_APEX_DEG * DEG_TO_RAD;
  float n      = N_PRISM * sin((caseNum == 1) ? (apex - thetaR) : (apex + thetaR));

  *angleOut = bigAngle;
  *nOut     = n;

  Serial.print(F("steps=")); Serial.print(sweepSteps, 3);
  Serial.print(F("  angle=")); Serial.print(bigAngle, 4);
  Serial.print(F("  incident=")); Serial.print(incident, 4);
  Serial.print(F("  case=")); Serial.print(caseNum);
  Serial.print(F("  dark=")); Serial.print(darkLevel);
  Serial.print(F("  bright=")); Serial.print(brightLevel);
  Serial.print(F("  n=")); Serial.println(n, 5);

  goHome();
  return M_OK;
}

/* ===================================================================== */
/*  Least squares:  n = slope * conc + intercept                         */
/* ===================================================================== */

bool fitLine() {
  fitValid = false;
  if (calCount < 2) return false;

  float mx = 0, my = 0;
  for (byte i = 0; i < calCount; i++) { mx += cal[i].conc; my += cal[i].n; }
  mx /= calCount; my /= calCount;

  float sxx = 0, sxy = 0, syy = 0;
  for (byte i = 0; i < calCount; i++) {
    float dx = cal[i].conc - mx;
    float dy = cal[i].n    - my;
    sxx += dx * dx; sxy += dx * dy; syy += dy * dy;
  }
  if (sxx < 1e-12) return false;

  fitSlope     = sxy / sxx;
  fitIntercept = my - fitSlope * mx;
  fitR2        = (syy > 1e-12) ? (sxy * sxy) / (sxx * syy) : 1.0;
  fitValid     = (fabs(fitSlope) > 1e-9);

  /* Worst residual expressed in concentration units. Far more informative
     than R2, which stays above 0.99 even for a badly wrong point.         */
  fitMaxErr = 0;
  if (fitValid) {
    for (byte i = 0; i < calCount; i++) {
      float e = fabs(cal[i].n - (fitSlope * cal[i].conc + fitIntercept)) / fabs(fitSlope);
      if (e > fitMaxErr) fitMaxErr = e;
    }
  }
  return fitValid;
}

float concFromN(float n) { return (n - fitIntercept) / fitSlope; }

void calRange(float *lo, float *hi) {
  *lo = cal[0].conc; *hi = cal[0].conc;
  for (byte i = 1; i < calCount; i++) {
    if (cal[i].conc < *lo) *lo = cal[i].conc;
    if (cal[i].conc > *hi) *hi = cal[i].conc;
  }
}

/* ===================================================================== */
/*  Display / input helpers                                              */
/* ===================================================================== */

void lcd2(const char *l1, const char *l2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(l1);
  lcd.setCursor(0, 1); lcd.print(l2);
}

void showError(MeasResult r) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(F("MEASURE FAILED"));
  lcd.setCursor(0, 1);
  switch (r) {
    case M_ERR_NO_DARK:   lcd.print(F("Always bright"));   break;
    case M_ERR_NO_BRIGHT: lcd.print(F("No TIR found"));    break;
    case M_ERR_NO_EDGE:   lcd.print(F("Edge unclear"));    break;
    case M_ERR_RANGE:     lcd.print(F("n out of range"));  break;
    case M_ERR_BUDGET:    lcd.print(F("2 turns, no hit")); break;
    default:              lcd.print(F("Unknown"));         break;
  }
  delay(2500);
}

void showEntry() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Std ")); lcd.print(calCount + 1); lcd.print(F(" conc:"));
  lcd.setCursor(0, 1);
  entryBuf[entryLen] = '\0';
  if (entryLen) {
    lcd.print(entryBuf);
    lcd.setCursor(12, 1); lcd.print(F("#=OK"));
  } else {
    lcd.print('_');
    lcd.setCursor(10, 1); lcd.print(F("#=menu"));
  }
}

/* 3x4 keypad has no A-D column, so these four commands live on a menu
   reached by pressing # on an empty entry, or # from the test screen. */
void showMenu() {
  lcd2("1 Fit&test 2 Del", "3 Reset   # Back");
}

bool buttonPressed() {
  int raw = digitalRead(BUTTON_PIN);
  if (raw != lastButton && (millis() - lastButtonMs) > DEBOUNCE_MS) {
    lastButton   = raw;
    lastButtonMs = millis();
    if (raw == LOW) return true;
  }
  return false;
}

void enterMode(bool toConc) {
  concMode = toConc;
  entryLen = 0;
  if (!toConc) {
    state = ST_SINGLE_IDLE;
    lcd2("SINGLE MODE", "Press MEASURE");
  } else if (fitValid && calCount >= MIN_CAL) {
    state = ST_TEST_IDLE;
    lcd2("TEST MODE", "MEASURE  #=menu");
  } else {
    state = ST_CAL_INPUT;
    showEntry();
  }
}

/* ===================================================================== */

void setup() {
  Serial.begin(9600);
  pinMode(COIL1, OUTPUT); pinMode(COIL2, OUTPUT);
  pinMode(COIL3, OUTPUT); pinMode(COIL4, OUTPUT);
  coilsOff();
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  keypad.setHoldTime(600);        // hold * for 0.6 s to clear the entry
  lcd2("Refractometer", "warming up...");
  delay(2000);

  position = 0;
  homePos  = 0;          // assumes the prism sits at mechanical zero at power-on
  concMode = (digitalRead(SWITCH_PIN) == LOW);
  enterMode(concMode);
}

void loop() {
  static bool lastRocker = false;
  static unsigned long rockerMs = 0;
  bool rocker = (digitalRead(SWITCH_PIN) == LOW);
  if (rocker != lastRocker && millis() - rockerMs > DEBOUNCE_MS) {
    lastRocker = rocker;
    rockerMs   = millis();
    enterMode(rocker);
    return;
  }

  /* k    = a key that was just pressed
     kHold = a key that has just been held down past the hold time         */
  char k = 0, kHold = 0;
  if (keypad.getKeys()) {
    for (byte i = 0; i < LIST_MAX; i++) {
      if (keypad.key[i].stateChanged) {
        if      (keypad.key[i].kstate == PRESSED) k     = keypad.key[i].kchar;
        else if (keypad.key[i].kstate == HOLD)    kHold = keypad.key[i].kchar;
      }
    }
  }

  bool  btn = buttonPressed();
  float n, angle;
  char  line[17];

  switch (state) {

    /* ---------------- MODE 1 : single measurement ------------------ */
    case ST_SINGLE_IDLE:
      if (btn) {
        lcd2("Measuring...", "please wait");
        MeasResult r = measureRefractive(&n, &angle);
        if (r != M_OK) { showError(r); lcd2("SINGLE MODE", "Press MEASURE"); }
        else {
          lcd.clear();
          lcd.setCursor(0, 0); lcd.print(F("n = ")); lcd.print(n, 4);
          lcd.setCursor(0, 1); lcd.print(F("ang ")); lcd.print(angle, 2);
          lcd.print((char)223);
          state = ST_SINGLE_RESULT;
        }
      }
      break;

    case ST_SINGLE_RESULT:
      if (btn || k) { state = ST_SINGLE_IDLE; lcd2("SINGLE MODE", "Press MEASURE"); }
      break;

    /* ---------------- MODE 2 : concentration ----------------------- */
    case ST_CAL_INPUT:
      if (kHold == '*') { entryLen = 0; showEntry(); break; }   // hold * = clear
      if (k) {
        if (k >= '0' && k <= '9') {
          if (entryLen < 7) { entryBuf[entryLen++] = k; showEntry(); }
        } else if (k == '*') {
          bool hasDot = false;
          for (byte i = 0; i < entryLen; i++) if (entryBuf[i] == '.') hasDot = true;
          if (!hasDot && entryLen < 7) { entryBuf[entryLen++] = '.'; showEntry(); }
        } else if (k == '#') {
          if (entryLen == 0) { state = ST_CAL_MENU; showMenu(); break; }
          if (calCount >= MAX_CAL) {
            lcd2("Store full", "# then 1 to fit"); delay(1800); showEntry(); break;
          }
          entryBuf[entryLen] = '\0';
          pendingConc = atof(entryBuf);
          entryLen = 0;
          state = ST_CAL_PROMPT;
          lcd.clear();
          lcd.setCursor(0, 0);  lcd.print(F("Clean+load std"));
          lcd.setCursor(0, 1);  lcd.print(F("C=")); lcd.print(pendingConc, 3);
          lcd.setCursor(10, 1); lcd.print(F("*=bck"));
        }
      }
      break;

    case ST_CAL_MENU:
      if (k == '1') {                                   // fit and enter test mode
        if (calCount >= MIN_CAL && fitLine()) {
          lcd.clear();
          lcd.setCursor(0, 0); lcd.print(F("Cal done  n=")); lcd.print(calCount);
          lcd.setCursor(0, 1);
          lcd.print(F("R2=")); lcd.print(fitR2, 4);
          lcd.print(F(" e=")); lcd.print(fitMaxErr, 2);
          delay(2500);
          state = ST_TEST_IDLE;
          lcd2("TEST MODE", "MEASURE  #=menu");
        } else {
          snprintf(line, sizeof(line), "Need %d points", MIN_CAL);
          lcd2("Not enough data", line); delay(1800); showMenu();
        }
      } else if (k == '2') {                            // delete last point
        if (calCount) { calCount--; fitLine(); lcd2("Last point", "deleted"); }
        else          { lcd2("Nothing to", "delete"); }
        delay(1300); showMenu();
      } else if (k == '3') {                            // wipe calibration
        calCount = 0; fitValid = false; entryLen = 0;
        lcd2("Calibration", "cleared"); delay(1400);
        state = ST_CAL_INPUT; showEntry();
      } else if (k) {                                   // anything else = back
        state = ST_CAL_INPUT; showEntry();
      }
      break;

    case ST_CAL_PROMPT:
      if (k == '*') { state = ST_CAL_INPUT; showEntry(); break; }
      if (btn) {
        lcd2("Measuring...", "please wait");
        MeasResult r = measureRefractive(&n, &angle);
        if (r != M_OK) { showError(r); state = ST_CAL_INPUT; showEntry(); }
        else {
          cal[calCount].conc = pendingConc;
          cal[calCount].n    = n;
          calCount++;
          fitLine();
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(F("P")); lcd.print(calCount);
          lcd.print(F(" n=")); lcd.print(n, 4);
          lcd.setCursor(0, 1);
          if (calCount >= 3 && fitValid) {
            lcd.print(F("R2=")); lcd.print(fitR2, 4);
            lcd.print(F(" e=")); lcd.print(fitMaxErr, 2);
          }
          else                           { lcd.print(F("stored")); }
          state = ST_CAL_RESULT;
        }
      }
      break;

    case ST_CAL_RESULT:
      if (btn || k) { state = ST_CAL_INPUT; showEntry(); }
      break;

    case ST_TEST_IDLE:
      if (k == '#') { state = ST_CAL_MENU; showMenu(); break; }
      if (btn) {
        lcd2("Measuring...", "please wait");
        MeasResult r = measureRefractive(&n, &angle);
        if (r != M_OK) { showError(r); lcd2("TEST MODE", "MEASURE  #=menu"); }
        else {
          float c = concFromN(n);
          float lo, hi; calRange(&lo, &hi);
          lcd.clear();
          lcd.setCursor(0, 0); lcd.print(F("C = ")); lcd.print(c, 3);
          if (c < lo || c > hi) lcd.print(F(" !"));
          lcd.setCursor(0, 1); lcd.print(F("n=")); lcd.print(n, 4);
          lcd.print(F(" R2=")); lcd.print(fitR2, 3);
          Serial.print(F("conc=")); Serial.println(c, 5);
          state = ST_TEST_RESULT;
        }
      }
      break;

    case ST_TEST_RESULT:
      if (k == '#') { state = ST_CAL_MENU; showMenu(); break; }
      if (btn || k) { state = ST_TEST_IDLE; lcd2("TEST MODE", "MEASURE  #=menu"); }
      break;
  }
}
