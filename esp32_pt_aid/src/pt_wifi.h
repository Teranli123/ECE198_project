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
//             int a; if (ptWifiTakeAge(a)) { age = a; ... }
//
// Finding the laptop: the server announces itself on the WiFi with mDNS
// (service "_ptaid._tcp", the same thing printers use). The ESP32 looks it up
// with the built-in ESPmDNS library, so the laptop's address can change
// without reflashing. If the lookup fails, it falls back to PT_SERVER_HOST.
//
// WiFi name and password live in wifi_config.h
// (copy wifi_config.example.h to wifi_config.h and fill it in).

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>

#if __has_include("wifi_config.h")
#include "wifi_config.h"
#else
#error "Copy src/wifi_config.example.h to src/wifi_config.h and fill in your hotspot name and password."
#endif

#ifndef PT_SERVER_HOST
#define PT_SERVER_HOST ""       // optional fallback; discovery normally finds the laptop
#endif
#ifndef PT_SERVER_PORT
#define PT_SERVER_PORT 8000
#endif
#define PT_REDISCOVER_AFTER 6   // failed sends in a row (~3 s) before looking for the server again
#ifndef PT_WIFI_TX_POWER
#define PT_WIFI_TX_POWER WIFI_POWER_8_5dBm   // default is 19.5 dBm
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
  int pendingAge = -1;        // set by the WiFi task when the dashboard sends a new patient age
  int lastAge = -1;           // last age received (WiFi task only), so each change is passed on once
  char deviceId[7] = "000000";

  // Look for the laptop server on the network. Fills host/port; false if not found.
  bool discover(String &host, uint16_t &port) {
    int n = MDNS.queryService("ptaid", "tcp");   // blocks up to ~3 s; fine, this is the WiFi task
    if (n > 0) {
      host = MDNS.IP(0).toString();
      port = MDNS.port(0);
      Serial.printf("Found PT Aid server at %s:%u\n", host.c_str(), port);
      return true;
    }
    if (strlen(PT_SERVER_HOST) > 0) {
      host = PT_SERVER_HOST;
      port = PT_SERVER_PORT;
      Serial.printf("Server not found on the network; trying %s:%u from wifi_config.h\n", host.c_str(), port);
      return true;
    }
    Serial.println("Server not found on the network yet. Is pt_server.py running?");
    return false;
  }

  // Explain why WiFi isn't connected yet, in plain words.
  void printWifiProblem(wl_status_t st) {
    switch (st) {
      case WL_NO_SSID_AVAIL:
        Serial.printf("WiFi: can't see a network called \"%s\". Check the exact name (case and spaces) "
                      "and that it is 2.4 GHz (iPhone: turn on Maximize Compatibility).\n", PT_WIFI_SSID);
        break;
      case WL_CONNECT_FAILED:
        Serial.printf("WiFi: \"%s\" refused the connection. The password is probably wrong.\n", PT_WIFI_SSID);
        break;
      case WL_CONNECTION_LOST:
        Serial.println("WiFi: connection lost, reconnecting...");
        break;
      default:
        Serial.printf("WiFi: still connecting to \"%s\"...\n", PT_WIFI_SSID);
        break;
    }
  }

  void task(void *) {
    // Don't save WiFi settings in flash: always use exactly what's in wifi_config.h,
    // so reflashing with a new hotspot never mixes with an old saved one.
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, true);  // forget any network saved by older firmware
    delay(100);
    WiFi.setAutoReconnect(true);
    Serial.printf("WiFi: joining \"%s\"\n", PT_WIFI_SSID);
    WiFi.begin(PT_WIFI_SSID, PT_WIFI_PASSWORD);
    // Lower transmit power cuts the current spikes that trigger brownout resets on
    // weak USB power. Plenty for a hotspot in the same room; raise it if range is poor.
    WiFi.setTxPower(PT_WIFI_TX_POWER);
    uint32_t lastReport = millis();

    String host;
    uint16_t port = PT_SERVER_PORT;
    bool announced = false, haveServer = false;
    int failures = 0;
    char url[64];

    for (;;) {
      wl_status_t st = WiFi.status();
      if (st != WL_CONNECTED) {
        announced = false;
        if (millis() - lastReport > 5000) { printWifiProblem(st); lastReport = millis(); }
        vTaskDelay(pdMS_TO_TICKS(500));
        continue;
      }
      if (!announced) {
        Serial.printf("WiFi connected as device %s, IP %s\n", deviceId, WiFi.localIP().toString().c_str());
        MDNS.end();
        MDNS.begin((String("ptaid-") + deviceId).c_str());
        announced = true;
        haveServer = false;
      }
      if (!haveServer || failures >= PT_REDISCOVER_AFTER) {
        failures = 0;
        haveServer = discover(host, port);
        if (!haveServer) { vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
        snprintf(url, sizeof(url), "http://%s:%u/api/data", host.c_str(), port);
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
        failures = (code == 200) ? 0 : failures + 1;
        if (code == 200) {
          String reply = http.getString();          // {"mode": 1, "age": 45}
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
          int a = reply.indexOf("\"age\"");
          if (a >= 0) {
            int age = reply.substring(reply.indexOf(':', a) + 1).toInt();
            if (age > 0 && age != lastAge) {
              lastAge = age;
              Serial.printf("Dashboard set patient age to %d\n", age);
              portENTER_CRITICAL(&mux);
              pendingAge = age;
              portEXIT_CRITICAL(&mux);
            }
          }
        }
        http.end();
      } else {
        failures++;
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

// True (once) when the instructor set this patient's age on the dashboard.
inline bool ptWifiTakeAge(int &newAge) {
  bool got = false;
  portENTER_CRITICAL(&ptwifi::mux);
  if (ptwifi::pendingAge > 0) { newAge = ptwifi::pendingAge; ptwifi::pendingAge = -1; got = true; }
  portEXIT_CRITICAL(&ptwifi::mux);
  return got;
}
