// PT Aid WiFi link: sends heart rate and reps to the session server on the
// instructor's laptop (server/pt_server.py), and receives the "exercise for
// everyone" setting back.
//
// Networking runs in its own FreeRTOS task on core 0, so a slow or dropped
// WiFi connection never stalls the 100 Hz sensor loop on core 1.
//
// Use from main.cpp:
//   #include "pt_wifi.h"
//   setup():  ptWifiBegin();
//   loop():   ptWifiPublish(average, rep_count, exercise_mode);
//             int m; if (ptWifiTakeMode(m)) { exercise_mode = m; rep_count = 0; ... }
//
// WiFi name, password and the laptop's address live in wifi_config.h
// (copy wifi_config.example.h to wifi_config.h and fill it in).

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#if __has_include("wifi_config.h")
#include "wifi_config.h"
#else
#error "Copy src/wifi_config.example.h to src/wifi_config.h and fill in your hotspot and laptop address."
#endif

#ifndef PT_SERVER_PORT
#define PT_SERVER_PORT 8000
#endif
#ifndef PT_SEND_MS
#define PT_SEND_MS 500        // send twice a second
#endif

namespace ptwifi {
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  float bpm = 0;
  int reps = 0;
  int mode = 0;
  int pendingMode = -1;       // set by the WiFi task when the server wants a different exercise
  char deviceId[7] = "000000";

  void task(void *) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(PT_WIFI_SSID, PT_WIFI_PASSWORD);

    char url[64];
    snprintf(url, sizeof(url), "http://%s:%d/api/data", PT_SERVER_HOST, PT_SERVER_PORT);
    bool announced = false;

    for (;;) {
      if (WiFi.status() != WL_CONNECTED) {
        announced = false;
        vTaskDelay(pdMS_TO_TICKS(500));
        continue;
      }
      if (!announced) {
        Serial.printf("WiFi connected as device %s, IP %s, sending to %s\n",
                      deviceId, WiFi.localIP().toString().c_str(), url);
        announced = true;
      }

      float b; int r, m;
      portENTER_CRITICAL(&mux);
      b = bpm; r = reps; m = mode;
      portEXIT_CRITICAL(&mux);

      char body[96];
      int len = snprintf(body, sizeof(body), "{\"id\":\"%s\",\"bpm\":%.1f,\"reps\":%d,\"mode\":%d}",
                         deviceId, b, r, m);

      HTTPClient http;
      http.setConnectTimeout(1000);
      http.setTimeout(1000);
      if (http.begin(url)) {
        http.addHeader("Content-Type", "application/json");
        int code = http.POST((uint8_t *)body, len);
        if (code == 200) {
          String reply = http.getString();          // {"mode": 1}
          int i = reply.indexOf("\"mode\"");
          if (i >= 0) {
            int colon = reply.indexOf(':', i);
            int want = reply.substring(colon + 1).toInt();
            if (want != m) {
              portENTER_CRITICAL(&mux);
              pendingMode = want;
              portEXIT_CRITICAL(&mux);
            }
          }
        }
        http.end();
      }
      vTaskDelay(pdMS_TO_TICKS(PT_SEND_MS));
    }
  }
}

// Start WiFi and the sending task. Call once in setup().
inline void ptWifiBegin() {
  uint64_t mac = ESP.getEfuseMac();   // unique per board; last 3 bytes become the device ID
  snprintf(ptwifi::deviceId, sizeof(ptwifi::deviceId), "%02X%02X%02X",
           (uint8_t)(mac >> 24), (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
  xTaskCreatePinnedToCore(ptwifi::task, "ptwifi", 6144, nullptr, 1, nullptr, 0);
}

// Hand the latest values to the WiFi task. Cheap; call every loop.
inline void ptWifiPublish(float bpm, int reps, int mode) {
  portENTER_CRITICAL(&ptwifi::mux);
  ptwifi::bpm = bpm;
  ptwifi::reps = reps;
  ptwifi::mode = mode;
  portEXIT_CRITICAL(&ptwifi::mux);
}

// True (once) when the instructor switched the exercise on the dashboard.
inline bool ptWifiTakeMode(int &newMode) {
  bool got = false;
  portENTER_CRITICAL(&ptwifi::mux);
  if (ptwifi::pendingMode >= 0) { newMode = ptwifi::pendingMode; ptwifi::pendingMode = -1; got = true; }
  portEXIT_CRITICAL(&ptwifi::mux);
  return got;
}
