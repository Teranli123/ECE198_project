#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

// ---------- Accelerometer ----------
const uint8_t PX = A0, PY = A1, PZ = A2;

// Calibrate these zero-g counts while the sensor is flat on your desk
float ffx0 = 337, ffy0 = 337, ffz0 = 337;
const float COUNTS_PER_G = 163.7;

// EMA smoothing factor for the accelerometer (0.0 to 1.0)
const float ALPHA2 = 0.2;
float emaX = 0, emaY = 0, emaZ = 0;
bool firstRead = true;

// ---------- Heart rate sensor ----------
MAX30105 sensor;

// ---------- Multicolor LED ----------
int age = 18;
const int heart_rate_max = (220 - age);
const int redpin = 13;
const int greenpin = 12;
const int bluepin = 11;

const int FINGER_LIMIT = 10000;   // IR below this = no finger
const int GRACE = 1500;           // ms of lost contact we tolerate
const int WARMUP_TIME = 10000;    // ms of recording before displaying
const float ALPHA = 0.25;         // heart rate EMA factor
const int MAX_BEATS = 40;         // max beats stored during warm-up

//general vars

float warmReadings[MAX_BEATS];
int warmCount = 0;
bool warmingUp = true;
long fingerStartTime = 0;

int rep_count = 0;
int confidence_count = 0;
int badBeats = 0;
long lastBeatTime = 0;
long lastFingerTime = 0;
float average = 0;

unsigned int exercise_mode{0}; // 0 for squats, 1 for bicep curls

// Squat rep counter. Tune the thresholds from the Serial Plotter:
// standing should sit below SQUAT_LOWER, the bottom of a squat above SQUAT_UPPER.
const float SQUAT_UPPER = 0.65;   // g, signal rises above this = at the bottom
const float SQUAT_LOWER = 0.35;   // g, signal falls below this = back standing
const long MIN_REP_MS = 800;      // ignore "reps" faster than this (bumps, jitter)
bool squatDown = false;
long lastRepTime = 0;

// Bicep curl rep counter.
// Sensor on the forearm. With the arm hanging, gravity points along -x.
// Curling the forearm up rotates gravity toward +x. We track the angle between
// the gravity vector and the -x axis: about 0 deg at the bottom, about 140-180 deg
// at the top. Using the angle (not raw x) means the y/z tilt of the strap and the
// exact g scaling don't matter. Tune the two angles from the CurlAngle plot.
const int   CURL_AXIS = 0;          // 0 = x, 1 = y, 2 = z: axis gravity runs along at the bottom
const float CURL_SIGN = -1.0;       // -1 = gravity points along the NEGATIVE axis at the bottom
const float CURL_TOP_DEG = 120.0;   // angle above this = at the top of the curl
const float CURL_BOTTOM_DEG = 45.0; // angle below this = back at the bottom
const float CURL_MIN_G = 0.5;       // ignore samples while shaking (|a| too small to trust)
bool curlUp = false;
float curlAngle = 0;

// Angle in degrees between the measured gravity vector and the "bottom" direction
float gravityAngleFromStart(float ax, float ay, float az) {
  float mag = sqrt(ax * ax + ay * ay + az * az);
  if (mag < CURL_MIN_G) return -1;  // not trustworthy, caller keeps the old angle
  float a = (CURL_AXIS == 0) ? ax : (CURL_AXIS == 1) ? ay : az;
  float c = constrain(CURL_SIGN * a / mag, -1.0, 1.0);
  return acos(c) * 180.0 / PI;
}

// Start everything over (finger removed)
void resetAll() {
  warmingUp = true;
  warmCount = 0;
  fingerStartTime = 0;
  confidence_count = 0;
  badBeats = 0;
  lastBeatTime = 0;
  average = 0;
}

// Pick the LED color from the BPM zone
void set_LED_color(int hrMax, float bpm) {
  int r = 0, g = 0, b = 0;

  if (bpm >= 40) {                          // below 40 = no finger, LED off
    if (bpm < hrMax * 0.4)       g = 200;   // low zone: GREEN
    else if (bpm <= hrMax * 0.6) b = 200;   // medium zone: BLUE
    else                         r = 200;   // high zone: RED
  }

  analogWrite(redpin, r);
  analogWrite(greenpin, g);
  analogWrite(bluepin, b);
}

// Called once when the 10 s warm-up is done
void finishWarmup() {
  float sum = 0;
  for (int i = 0; i < warmCount; i++) {
    sum += warmReadings[i];
  }
  float firstAverage = sum / warmCount;

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
  confidence_count = 3;
  badBeats = 0;
}

int readAvg(uint8_t pin) {
  analogRead(pin);  // throwaway read to let multiplexer settle
  int sum = 0;
  for (int i = 0; i < 4; i++) sum += analogRead(pin);
  return sum / 4;
}

//to reduce chatter

bool hysteresis(bool state, float reading, float uthreshold, float lthreshold){ 
  if(state && (reading < lthreshold)){return false;}
  else if(!state && (reading > uthreshold)){return true;}
  return state; // stay in the middle
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);   // UNO R4: wait up to 3 s for the monitor

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
}

void loop() {
  // getIR() waits for the next sensor sample, so this loop runs at ~100 Hz
  long ir = sensor.getIR();
  long now = millis();

  // ---------- Heart rate ----------
  if (ir >= FINGER_LIMIT) {
    lastFingerTime = now;
    if (fingerStartTime == 0) fingerStartTime = now;
  }

  if (now - lastFingerTime > GRACE) {
    resetAll();
  }
  else {
    if (warmingUp && now - fingerStartTime >= WARMUP_TIME && warmCount >= 3) {
      finishWarmup();
    }

    if (ir >= FINGER_LIMIT && checkForBeat(ir)) {

      if (lastBeatTime != 0) {
        float bpm = 60000.0 / (now - lastBeatTime);
        bool realistic = (bpm > 40 && bpm < 220);

        if (warmingUp) {
          if (realistic && warmCount < MAX_BEATS) {
            warmReadings[warmCount] = bpm;
            warmCount++;
          }
        }
        else {
          bool closeToAverage = (confidence_count < 3 || fabs(bpm - average) < 20);

          if (realistic && closeToAverage) {
            badBeats = 0;
            average = average + ALPHA * (bpm - average);
            if (confidence_count < 3) confidence_count++;
          } else {
            badBeats++;
            if (badBeats >= 3) {
              confidence_count = 0;
              badBeats = 0;
            }
          }
        }
      }

      lastBeatTime = now;
    }
  }

  // ---------- Accelerometer ----------
  float rawX = (readAvg(PX) - ffx0) / COUNTS_PER_G;
  float rawY = (readAvg(PY) - ffy0) / COUNTS_PER_G;
  float rawZ = (readAvg(PZ) - ffz0) / COUNTS_PER_G;

  if (firstRead) {
    emaX = rawX;
    emaY = rawY;
    emaZ = rawZ;
    firstRead = false;
  } else {
    emaX = (ALPHA2 * rawX) + ((1.0 - ALPHA2) * emaX);
    emaY = (ALPHA2 * rawY) + ((1.0 - ALPHA2) * emaY);
    emaZ = (ALPHA2 * rawZ) + ((1.0 - ALPHA2) * emaZ);
  }


  //rep couter logic

  if(exercise_mode == 0){
    bool wasDown = squatDown;
    squatDown = hysteresis(squatDown, emaZ, SQUAT_UPPER, SQUAT_LOWER);

    // detect down to up is one full rep
    if(wasDown && !squatDown && now - lastRepTime > MIN_REP_MS){
      rep_count++;
      lastRepTime = now;
    }
  }
  else if(exercise_mode == 1){
    float ang = gravityAngleFromStart(emaX, emaY, emaZ);
    if (ang >= 0) curlAngle = ang;

    bool wasUp = curlUp;
    curlUp = hysteresis(curlUp, curlAngle, CURL_TOP_DEG, CURL_BOTTOM_DEG);

    // bottom -> top -> back to bottom is one full rep
    if(wasUp && !curlUp && now - lastRepTime > MIN_REP_MS){
      rep_count++;
      lastRepTime = now;
    }
  }

  // ---------- LED ----------
  set_LED_color(heart_rate_max, average);

  // ---------- Print every loop (~100 Hz) for the Serial Plotter ----------
  Serial.print("X:");
  Serial.print(emaX, 3); //3 decimals   
  Serial.print(",Y:");
  Serial.print(emaY, 3);
  Serial.print(",Z:");
  Serial.print(emaZ, 3);
  Serial.print(",BPM_div_100:");
  Serial.print(average / 100, 3);
  if (exercise_mode == 1) {
    Serial.print(",CurlAngle_div_100:");
    Serial.print(curlAngle / 100, 3);
  }
  Serial.print(",Reps:");
  Serial.println(rep_count);
}