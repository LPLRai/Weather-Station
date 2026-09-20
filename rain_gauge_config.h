// rain_gauge_config.h
//
// Configuration for the tipping-bucket Hall effect rain gauge on ESP-12E.
//

#ifndef RAIN_GAUGE_CONFIG_H
#define RAIN_GAUGE_CONFIG_H

#include <Arduino.h>

// =====================================================================
// Pin Mapping - ESP-12E / NodeMCU
// Hall Sensor Wiring: VCC -> 3V3, GND -> GND, OUT / Signal -> D3
// =====================================================================
#ifndef D3
#define D3 0 // GPIO0 on ESP8266 / NodeMCU
#endif

#define RAIN_SENSOR_PIN D3

// =====================================================================
// Calibration
// =====================================================================
// Amount of rain in millimeters represented by one tipping bucket pulse.
// Standard tipping buckets:
// - Misol / SparkFun / Davis standard: ~0.2794 mm (0.011 inches)
// - Generic standard 0.2 mm / tip: 0.2000 mm
// Adjust this to match your physical bucket's calibration.
#define RAIN_MM_PER_TIP 0.2794f

// Hardware switch / Hall contact debounce window in milliseconds.
// Prevents duplicate counts from mechanical bounce when the bucket tips.
#define RAIN_DEBOUNCE_MS 60

// =====================================================================
// Persistent Storage & History Settings (LittleFS)
// =====================================================================
// Save historical aggregates to LittleFS flash so data persists across reboots.
#define RAIN_ENABLE_STORAGE true

// How often (in milliseconds) rain totals are committed to flash if new rain fell.
// (Default: every 5 minutes = 300,000 ms to preserve flash wear)
#define RAIN_SAVE_INTERVAL_MS 300000UL

// Number of hourly records to keep in memory/flash (24 hours)
#define RAIN_HOURLY_HISTORY_HOURS 24

// Number of daily records to keep in memory/flash (30 days)
#define RAIN_DAILY_HISTORY_DAYS 30

// =====================================================================
// WiFi / Web Dashboard
// =====================================================================
#define ENABLE_WEB_DASHBOARD true
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// If WiFi cannot connect, ESP starts an Access Point with this name:
#define AP_SSID "WeatherStation-AP"
#define AP_PASSWORD "" // Leave empty for open AP

#endif // RAIN_GAUGE_CONFIG_H
