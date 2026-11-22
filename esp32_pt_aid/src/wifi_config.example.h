// Copy this file to wifi_config.h (same folder) and fill it in.
// wifi_config.h is ignored by git so your hotspot password stays off GitHub.

#pragma once

#define PT_WIFI_SSID     "MyPhoneHotspot"   // hotspot name (2.4 GHz; on iPhone turn on "Maximize Compatibility")
#define PT_WIFI_PASSWORD "hotspot-password"
// Optional: the ESP32 finds the laptop by itself. This address is only a fallback
// if that fails. Leave it as "" or put the address pt_server.py prints.
#define PT_SERVER_HOST   ""
#define PT_SERVER_PORT   8000
