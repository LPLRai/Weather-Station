// wind_speed_config.h
//
// Tunable settings for the hall-effect anemometer (wind_speed.cpp).
// Edit values here instead of digging through the code.
//
#ifndef WIND_SPEED_CONFIG_H
#define WIND_SPEED_CONFIG_H

// ---- Pin ----
// Hall sensor OUT -> D6 (GPIO12) on ESP-12E / NodeMCU.
#define WIND_SPEED_PIN D6

// ---- Calibration ----
// Distance from the anemometer's axis to the centre of a cup, in metres.
// CHANGE this to match your build (e.g. 0.09 = 9 cm).
#define WIND_SPEED_RADIUS_M 0.09f

// Magnet passes (pulses) the hall sensor sees per full rotation.
// 1 magnet = 1, 2 magnets = 2, and so on.
#define WIND_SPEED_PULSES_PER_ROTATION 1

// Optional correction factor applied to the computed speed.
// Cup anemometers usually need one to match a reference meter. Leave at 1.0
// until you have something to calibrate against.
#define WIND_SPEED_CALIBRATION_FACTOR 1.0f

// ---- Timing ----
// How often a speed reading is calculated and printed (ms).
#define WIND_SPEED_SAMPLE_INTERVAL_MS 2000UL

// ---- Serial output ----
// Set to 0 to silence the [speed] prints (values are still calculated).
#define WIND_SPEED_PRINT_ENABLED 1

#endif // WIND_SPEED_CONFIG_H