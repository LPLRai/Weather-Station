#ifndef WIND_SPEED_H
#define WIND_SPEED_H

#include <Arduino.h>

struct WindSpeedReading {
  float speedKMH;
  float rpm;
  float rotations;
  unsigned long pulsesInWindow;
  unsigned long totalSpins;
  unsigned long totalPulses;
};

// ---- Public API ----

// Call once from setup(): configures the pin and attaches the interrupt.
void setupWindSpeed();

// Call repeatedly from loop(): every SAMPLE_INTERVAL_MS it computes
// and prints pulses, rotations, RPM, and speed.
void readWindSpeed();

// Returns the latest calculated wind speed reading
WindSpeedReading getWindSpeedReading();

// ---- Lifetime counters (defined in wind_speed.cpp) ----
extern unsigned long totalPulses;   // lifetime pulse count
extern unsigned long totalSpins;    // lifetime full rotations

#endif // WIND_SPEED_H