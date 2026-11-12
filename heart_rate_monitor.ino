#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "MAX30105.h"
#include "heartRate.h"

MAX30105 sensor;
LiquidCrystal_I2C lcd(0x27, 16, 2);   // try 0x3F if the screen stays blank

const int FINGER_LIMIT = 10000;   // IR below this = no finger
const int GRACE = 1500;           // ms of lost contact we tolerate
const int WARMUP_TIME = 10000;    // ms of recording before displaying
const float ALPHA = 0.25;         // EMA speed: small = smooth, large = fast
const int PRINT_EVERY = 10;       // print every 10th reading (wider plot)
const int MAX_BEATS = 40;         // max beats stored during warm-up
const int LCD_EVERY = 500;        // update the LCD every 500 ms

float warmReadings[MAX_BEATS];    // BPM values recorded during warm-up
int warmCount = 0;                // how many are stored
bool warmingUp = true;            // true = still in the 10 s warm-up
long fingerStartTime = 0;         // when the finger first touched (0 = not yet)

int count = 0;                    // good beats since warm-up ended
int badBeats = 0;                 // wrong beats in a row
long lastBeatTime = 0;
long lastFingerTime = 0;
float average = 0;                // the smoothed BPM
int printCounter = 0;
long lastLCDTime = 0;

// Start everything over (finger removed)
void resetAll() {
  warmingUp = true;
  warmCount = 0;
  fingerStartTime = 0;
  count = 0;
  badBeats = 0;
  lastBeatTime = 0;
  average = 0;
}

// Called once when the 10 s warm-up is done
void finishWarmup() {
  // 1st pass: plain average of everything recorded
  float sum = 0;
  for (int i = 0; i < warmCount; i++) {
    sum += warmReadings[i];
  }
  float firstAverage = sum / warmCount;

  // 2nd pass: average again, ignoring beats far from the first average
  sum = 0;
  int n = 0;
  for (int i = 0; i < warmCount; i++) {
    if (fabs(warmReadings[i] - firstAverage) < 20) {
      sum += warmReadings[i];
      n++;
    }
  }

  if (n > 0) average = sum / n;
  else average = firstAverage;

  warmingUp = false;
  count = 3;       // start checking "close to average" right away
  badBeats = 0;
}

// Write text on one LCD row, padded with spaces to erase old text
void showLine(int row, const char* text) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", text);
  lcd.setCursor(0, row);
  lcd.print(buf);
}

// Decide what the LCD should say
void updateLCD(long now) {
  char line[17];

  if (now - lastFingerTime > GRACE) {
    showLine(1, "Place finger");
  }
  else if (warmingUp) {
    long left = (WARMUP_TIME - (now - fingerStartTime) + 999) / 1000;
    if (left < 0) left = 0;
    snprintf(line, sizeof(line), "Warm-up: %lds", left);
    showLine(1, line);
  }
  else {
    snprintf(line, sizeof(line), "BPM: %d", (int)(average + 0.5));
    showLine(1, line);
  }
}

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  showLine(0, "Heart Rate");
  showLine(1, "Starting...");

  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("Sensor not found. Check wiring.");
    showLine(1, "Sensor not found");
    while (1);
  }

  sensor.setup(0x1F, 4, 2, 400, 411, 16384);
  sensor.setPulseAmplitudeRed(0);  // red off, IR stays on

  lastFingerTime = -(GRACE + 1);   // start in the "no finger" state
}

void loop() {
  long ir = sensor.getIR();
  long now = millis();

  // Remember when a finger is on the sensor
  if (ir >= FINGER_LIMIT) {
    lastFingerTime = now;
    if (fingerStartTime == 0) fingerStartTime = now;  // warm-up timer starts
  }

  // 1. Finger gone for more than 1.5 seconds? Start over.
  if (now - lastFingerTime > GRACE) {
    resetAll();
  }
  else {
    // 2. Warm-up finished? (10 s passed and at least 3 beats recorded)
    if (warmingUp && now - fingerStartTime >= WARMUP_TIME && warmCount >= 3) {
      finishWarmup();
    }

    // 3. Finger on and a beat was detected
    if (ir >= FINGER_LIMIT && checkForBeat(ir)) {

      if (lastBeatTime != 0) {
        float bpm = 60000.0 / (now - lastBeatTime);
        bool realistic = (bpm > 40 && bpm < 180);

        if (warmingUp) {
          // WARM-UP: just record believable beats, show nothing
          if (realistic && warmCount < MAX_BEATS) {
            warmReadings[warmCount] = bpm;
            warmCount++;
          }
        }
        else {
          // NORMAL: check the beat, then update the EMA
          bool closeToAverage = (count < 3 || fabs(bpm - average) < 20);

          if (realistic && closeToAverage) {
            badBeats = 0;
            average = average + ALPHA * (bpm - average);  // EMA
            if (count < 3) count++;
          } else {
            badBeats++;
            if (badBeats >= 3) {
              count = 0;     // accept bigger changes for the next 3 beats
              badBeats = 0;  // keep showing the current BPM
            }
          }
        }
      }

      lastBeatTime = now;
    }
  }

  // 4. Update the LCD (only twice a second so it doesn't slow the sensor)
  if (now - lastLCDTime >= LCD_EVERY) {
    lastLCDTime = now;
    updateLCD(now);
  }

  // 5. Print for the Serial Plotter (BPM shows 0 during warm-up)
  printCounter++;
  if (printCounter >= PRINT_EVERY) {
    printCounter = 0;

    float shown = warmingUp ? 0 : average;

    Serial.print("IR:");
    Serial.print(ir);
    Serial.print(",BPM:");
    Serial.println(shown);
  }
}