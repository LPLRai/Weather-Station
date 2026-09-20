/*
  Wind Speed Sensor (Hall Effect) - ESP12E (ESP8266)
  Wiring: Hall sensor OUT -> D6 (GPIO12), VCC -> 3V3, GND -> GND
*/

#include <Arduino.h>

// ---- Pin Definition ----
#define WIND_SENSOR_PIN D6   // GPIO12 on ESP12E

// ---- Calibration ----
// Radius of your anemometer (center to cup), in meters.
// Measure this from your physical build - it directly affects accuracy.
const float ANEMOMETER_RADIUS_M = 0.09;   // e.g. 9 cm - CHANGE to match your build

// Number of magnet pulses per full rotation (usually 1, sometimes 2 if you placed 2 magnets)
const int PULSES_PER_ROTATION = 1;

// How often we calculate and report speed (ms)
const unsigned long SAMPLE_INTERVAL_MS = 2000;

// ---- Globals ----
volatile unsigned long pulseCount = 0;
unsigned long lastSampleTime = 0;

unsigned long totalPulses = 0;   // lifetime pulse count
unsigned long totalSpins = 0;    // lifetime full rotations

// ISR: just increment counter, keep it fast
void IRAM_ATTR handlePulse() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);
  pinMode(WIND_SENSOR_PIN, INPUT_PULLUP); // Hall sensor usually open-drain, pullup needed
  attachInterrupt(digitalPinToInterrupt(WIND_SENSOR_PIN), handlePulse, FALLING);

  lastSampleTime = millis();
  Serial.println("Wind speed sensor ready...");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    // Safely grab and reset pulse count
    noInterrupts();
    unsigned long pulses = pulseCount;
    pulseCount = 0;
    interrupts();

    float elapsedSeconds = (now - lastSampleTime) / 1000.0;
    lastSampleTime = now;

    // Rotations in this window
    float rotations = pulses / (float)PULSES_PER_ROTATION;

    // Rotations per second
    float rps = rotations / elapsedSeconds;

    // RPM (human-readable rotational speed)
    float rpm = rps * 60.0;

    // Circumference swept by the cup = 2 * pi * radius
    float circumference = 2.0 * PI * ANEMOMETER_RADIUS_M;

    // Linear speed in m/s = rotations per second * circumference
    float speedMS = rps * circumference;

    // Convert to km/h
    float speedKMH = speedMS * 3.6;

    // Update lifetime counters
    totalPulses += pulses;
    totalSpins += (unsigned long)rotations;

    Serial.print("Pulses: ");
    Serial.print(pulses);
    Serial.print(" | Spins: ");
    Serial.print(rotations, 2);
    Serial.print(" | RPM: ");
    Serial.print(rpm, 2);
    Serial.print(" | Speed: ");
    Serial.print(speedKMH, 2);
    Serial.print(" km/h | Total Spins: ");
    Serial.println(totalSpins);
  }
}