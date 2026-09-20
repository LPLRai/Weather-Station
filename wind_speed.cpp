/*
  Wind Speed Sensor (Hall Effect) - ESP12E (ESP8266)
  Wiring: Hall sensor OUT -> D6 (GPIO12), VCC -> 3V3, GND -> GND
*/

#include <Arduino.h>
#include "wind_speed.h"

// ---- Pin Definition ----
#define WIND_SENSOR_PIN D6   // GPIO12 on ESP12E

// ---- Calibration ----
const float ANEMOMETER_RADIUS_M = 0.09;   // e.g. 9 cm - CHANGE to match your build
const int PULSES_PER_ROTATION = 1;
const unsigned long SAMPLE_INTERVAL_MS = 2000;

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
  pinMode(WIND_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WIND_SENSOR_PIN), handlePulse, FALLING);

  lastSampleTimeSpeed = millis();
  //Serial.println(F("[speed] Wind speed sensor ready..."));
}

void readWindSpeed() {
  unsigned long now = millis();

  if (now - lastSampleTimeSpeed >= SAMPLE_INTERVAL_MS) {
    // Safely grab and reset pulse count
    noInterrupts();
    unsigned long pulses = pulseCount;
    pulseCount = 0;
    interrupts();

    float elapsedSeconds = (now - lastSampleTimeSpeed) / 1000.0;
    lastSampleTimeSpeed = now;

    // Rotations in this window
    float rotations = pulses / (float)PULSES_PER_ROTATION;
    float rps = rotations / elapsedSeconds;
    float rpm = rps * 60.0;
    float circumference = 2.0 * PI * ANEMOMETER_RADIUS_M;
    float speedMS = rps * circumference;
    float speedKMH = speedMS * 3.6;

    // Update lifetime counters
    totalPulses += pulses;
    totalSpins += (unsigned long)rotations;

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
  }
}