/*
  Wind Speed Sensor (Hall Effect) - ESP12E (ESP8266)
  Wiring: Hall sensor OUT -> D6 (GPIO12), VCC -> 3V3, GND -> GND
  Settings live in wind_speed_config.h
*/

#include <Arduino.h>
#include "wind_speed.h"
#include "wind_speed_config.h"

// ---- Globals ----
volatile unsigned long pulseCount = 0;
unsigned long lastSampleTimeSpeed = 0;

unsigned long totalPulses = 0;   // lifetime pulse count
unsigned long totalSpins = 0;    // lifetime full rotations

// ISR: just increment counter, keep it fast
void IRAM_ATTR handlePulse() {
  pulseCount++;
}

void setupWindSpeed() {
  pinMode(WIND_SPEED_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WIND_SPEED_PIN), handlePulse, FALLING);

  lastSampleTimeSpeed = millis();
}

void readWindSpeed() {
  unsigned long now = millis();

  if (now - lastSampleTimeSpeed >= WIND_SPEED_SAMPLE_INTERVAL_MS) {
    // Safely grab and reset pulse count
    noInterrupts();
    unsigned long pulses = pulseCount;
    pulseCount = 0;
    interrupts();

    float elapsedSeconds = (now - lastSampleTimeSpeed) / 1000.0;
    lastSampleTimeSpeed = now;

    // Rotations in this window
    float rotations = pulses / (float)WIND_SPEED_PULSES_PER_ROTATION;
    float rps = rotations / elapsedSeconds;
    float rpm = rps * 60.0;
    float circumference = 2.0 * PI * WIND_SPEED_RADIUS_M;
    float speedMS = rps * circumference * WIND_SPEED_CALIBRATION_FACTOR;
    float speedKMH = speedMS * 3.6;

    // Update lifetime counters
    totalPulses += pulses;
    totalSpins += (unsigned long)rotations;

#if WIND_SPEED_PRINT_ENABLED
    Serial.print(F("[speed] Pulses: "));
    Serial.print(pulses);
    Serial.print(F(" | Spins: "));
    Serial.print(rotations, 2);
    Serial.print(F(" | RPM: "));
    Serial.print(rpm, 2);
    Serial.print(F(" | Speed: "));
    Serial.print(speedKMH, 2);
    Serial.print(F(" km/h | Total Spins: "));
    Serial.println(totalSpins);
#endif
  }
}