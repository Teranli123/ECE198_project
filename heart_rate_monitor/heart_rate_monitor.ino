#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

MAX30105 sensor;

//initialization for multicolored LED 
int age = 18;
const int heart_rate_max = (220 - age);   // test value for max heart rate 
const int redpin = 13; 
const int greenpin = 12; 
const int bluepin = 11; 

const int FINGER_LIMIT = 10000;   // IR limit, value below this means that there is no finger detected
const int GRACE = 1500;           // 1500 ms of time we tolerate your finger/body loosing connection with the sensor for
const int WARMUP_TIME = 10000;    // ms of recording before displaying, smoothen out first displayed values
const float ALPHA = 0.25;         // EMA (exponential moving average) adjustment factor
const int MAX_BEATS = 40;         // max beats stored during warm-up
const int PRINT_EVERY = 500;      // print to Serial Monitor every 500 ms

float warmReadings[MAX_BEATS];    // BPM values recorded during warm-up
int warmCount = 0;                // how many are stored
bool warmingUp = true;            // true = still in the 10 s warm-up
long fingerStartTime = 0;         // when the finger first touched (0 = not yet)

int count = 0;                    // good beats since warm-up ended
int badBeats = 0;                 // wrong beats in a row
long lastBeatTime = 0;
long lastFingerTime = 0;
float average = 0;                // the smoothed BPM
long lastPrintTime = 0;

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

void set_LED_color(int heart_rate_max, float average) {
  analogWrite(bluepin, 0);
  analogWrite(greenpin, 0); 
  analogWrite(redpin, 0);
  if (average < 40) {
    return;                     //no finger, LED isnt lit up
  }
  if (average <= (heart_rate_max*0.6) && average >= (heart_rate_max*0.4)) {
    analogWrite(bluepin, 200);                            // 40-60% zone, medium-intensity, BLUE LIGHT 
  }else if(average < (heart_rate_max*0.4) && average >= 40) {
    analogWrite(greenpin, 200);                           // low-intensity zone, GREEN LIGHT 
  }else {
    analogWrite(redpin, 200);                             // high-intensity zone, not recommended, RED LIGHT 
  }
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

void setup() {
  Serial.begin(115200);

  pinMode(redpin, OUTPUT);
  pinMode(greenpin, OUTPUT);
  pinMode(bluepin, OUTPUT);

  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("Sensor not found. Check wiring.");
    while (1);
  }

  sensor.setup(0x1F, 4, 2, 400, 411, 16384);
  sensor.setPulseAmplitudeRed(0);  // red off, IR stays on

  lastFingerTime = -(GRACE + 1);   // start in the "no finger" state
  Serial.println("Place your finger on the sensor.");
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
        bool realistic = (bpm > 40 && bpm < 220);

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

  set_LED_color(heart_rate_max, average);

  // 4. Print to the Serial Monitor
  if (now - lastPrintTime >= PRINT_EVERY) {
    lastPrintTime = now;

    if (now - lastFingerTime > GRACE) {
      Serial.println("No finger");
    }
    else if (warmingUp) {
      long left = (WARMUP_TIME - (now - fingerStartTime) + 999) / 1000;
      if (left < 0) left = 0;
      Serial.print("Warm-up: ");
      Serial.print(left);
      Serial.println("s left");
    }
    else {
      Serial.print("BPM: ");
      Serial.print(average, 0);   // 0 decimals
      Serial.print("   (IR: ");
      Serial.print(ir);
      Serial.println(")");
    }
  }
}