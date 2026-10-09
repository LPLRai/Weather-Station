// rain_gauge_config.h
//
// Configuration for the tipping-bucket Hall effect rain gauge on ESP-12E.
//

#ifndef RAIN_GAUGE_CONFIG_H
#define RAIN_GAUGE_CONFIG_H

#include <Arduino.h>

// =====================================================================
// Pin Mapping - ESP-12E / NodeMCU
// Hall Sensor / Reed Switch Wiring: VCC -> 3V3, GND -> GND, OUT / Signal -> D7 (GPIO13)
// =====================================================================
#ifndef D7
#define D7 13 // GPIO13 on ESP8266 / NodeMCU (Boot-safe & Interrupt capable)
#endif

// Rain gauge tipper pin: D7 (GPIO13)
#ifndef RAIN_SENSOR_PIN
#define RAIN_SENSOR_PIN D7
#endif

// =====================================================================
// Calibration
// =====================================================================
// Amount of rain in millimeters represented by one tipping bucket pulse.
// Standard tipping buckets:
// - Misol / SparkFun / Davis standard: ~0.2794 mm (0.011 inches)
// - Generic standard 0.2 mm / tip: 0.2000 mm
// Adjust this to match your physical bucket's calibration.
#define RAIN_MM_PER_TIP 0.2794f

// Tip window in milliseconds. Pin edges that arrive within this time of the
// previous edge belong to the same bucket tip: the first edge counts the tip
// and the following ones are ignored (contact bounce, or the trailing edge of
// the same Hall pulse). The window restarts on every edge.
// It must be longer than the pulse one tip produces on the pin, and shorter
// than the time between two real tips (seconds, even in a downpour).
// If you hand-test by flicking the bucket back and forth faster than this,
// the second flick is treated as part of the first one.
#define RAIN_DEBOUNCE_MS 200

// How often (in milliseconds) to print a compact rain reading to Serial.
#define RAIN_PRINT_INTERVAL_MS 5000

// 1 = also print a compact rain reading immediately every time a tip is
// counted, so a dashboard updates right away instead of waiting for the next
// RAIN_PRINT_INTERVAL_MS. 0 = only the periodic print.
#define RAIN_PRINT_ON_TIP 1

// 1 = print one "[rain] edge: ..." line for every raw pin change, with the
// level, the time since the previous edge and whether it was counted as a
// tip. Use it to see exactly what the sensor does on each side of the bucket.
// 0 = silent.
#define RAIN_DEBUG_EDGES 1

// =====================================================================
// History Settings
// =====================================================================
// Rain totals and history live in RAM only. Nothing is written to the
// ESP's flash, so every reset or power cycle starts again from zero.

// Number of hourly records to keep in memory (24 hours)
#define RAIN_HOURLY_HISTORY_HOURS 24

// Number of daily records to keep in memory (30 days)
#define RAIN_DAILY_HISTORY_DAYS 30

#endif // RAIN_GAUGE_CONFIG_H