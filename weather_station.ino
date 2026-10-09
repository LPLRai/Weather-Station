// weather_station.ino
//
// Main sketch for an ESP-12E based weather recording device.
// Wires up the AS5600 wind-direction sensor (wind_direction.h/.cpp),
// the rain gauge (rain_gauge.h/.cpp) and the hall-effect wind speed
// sensor (wind_speed.h/.cpp), and prints readings to the Serial monitor.
// Designed to be extended with more sensors later (each as its own
// module, following the same pattern as wind_direction).
//
#include <Arduino.h>
#include "wind_direction_config.h"
#include "wind_direction.h"
#include "rain_gauge.h"
#include "wind_speed.h"
#include "wind_speed_config.h"
#include "rain_gauge_config.h"
#include "firebase_config.h"
#include "firebase_weather.h"

WindDirection windSensor;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200); // let the USB-serial bridge settle
  Serial.println();
  Serial.println(F("=== Weather station booting ==="));

  // ---------------- Wind direction (AS5600) ----------------
  if (!windSensor.begin(I2C_SDA_PIN, I2C_SCL_PIN)) {
    Serial.println(F("[wind] ERROR: AS5600 did not ACK on I2C. Check wiring/address (0x36)."));
  } else {
    Serial.println(F("[wind] AS5600 found on I2C bus."));
  }

  MagnetStatus mstat = windSensor.getMagnetStatus();

  if (!mstat.i2cOk) {
    Serial.println(F("[wind] ERROR: I2C read failed (no ACK / short read). This is a WIRING problem, not a magnet problem - double check SDA/SCL/VCC/GND and pull-ups before touching the magnet."));
  } else {
    Serial.println(F("[wind] --- magnet diagnostics ---"));
    Serial.print(F("[wind] magnet detected (MD): "));
    Serial.println(mstat.detected ? F("yes") : F("NO"));
    Serial.print(F("[wind] too weak / too far (ML): "));
    Serial.println(mstat.tooWeak ? F("YES") : F("no"));
    Serial.print(F("[wind] too strong / too close (MH): "));
    Serial.println(mstat.tooStrong ? F("YES") : F("no"));
    Serial.print(F("[wind] AGC (mid-range is ideal): "));
    Serial.println(mstat.agc);
    Serial.print(F("[wind] Magnitude (~2000-2200 typical when well-positioned): "));
    Serial.println(mstat.magnitude);

    if (!mstat.detected) {
      Serial.println(F("[wind] -> No magnet seen at all. Check: it must be DIAMETRICALLY magnetized (poles side-to-side across the disc, not stacked top/bottom), centered directly over the IC, and within roughly 0.5-3mm air gap."));
    } else if (mstat.tooWeak) {
      Serial.println(F("[wind] -> Magnet detected but weak/far - move it closer to the sensor or use a stronger magnet."));
    } else if (mstat.tooStrong) {
      Serial.println(F("[wind] -> Magnet detected but too strong/close - increase the air gap slightly."));
    } else {
      Serial.println(F("[wind] -> Magnet looks well positioned."));
    }
  }

#if CALIBRATE_ON_BOOT
  float startRaw = windSensor.recordStartupDirection();
  if (startRaw < 0) {
    Serial.println(F("[wind] Startup calibration skipped: no magnet seen."));
  } else {
    Serial.print(F("[wind] Startup direction recorded: "));
    Serial.print(startRaw, 1);
    Serial.println(F(" deg raw -> this position is now the 0 deg / N reference."));
  }
#else
  Serial.print(F("[wind] Using fixed calibration offset from wind_direction_config.h: "));
  Serial.print((float)WIND_DIR_OFFSET_DEG, 1);
  Serial.println(F(" deg. Point the vane north and confirm deg reads ~0 below."));
#endif

  // ---------------- Rain gauge ----------------
  rainSensor.begin();
  Serial.print(F("[rain] Sensor ready on pin D7 (GPIO"));
  Serial.print(RAIN_SENSOR_PIN);
  Serial.print(F("). mm/tip="));
  Serial.print(RAIN_MM_PER_TIP, 4);
  Serial.print(F("  print interval="));
  Serial.print(RAIN_PRINT_INTERVAL_MS);
  Serial.println(F(" ms."));

  // ---------------- Wind speed (hall effect) ----------------
  setupWindSpeed();
  Serial.println(F("[speed] Hall sensor ready on pin D6 (GPIO12)."));

  // ---------------- Firebase & Wi-Fi ----------------
  firebaseWeather.begin();

  Serial.println(F("=== Setup complete, starting readings ==="));
}

void loop() {
  unsigned long now = millis();

  // Wind speed: has its own internal timer, so call it every loop.
  readWindSpeed();

  // Wind direction
  static WindReading latestWind;
  static unsigned long lastWindSample = 0;
  if (now - lastWindSample >= SAMPLE_INTERVAL_MS) {
    lastWindSample = now;

    latestWind = windSensor.read();

    Serial.print(F("[wind] raw="));
    Serial.print(latestWind.rawAngle);
    Serial.print(F(" deg="));
    Serial.print(latestWind.degrees, 1);
    Serial.print(F(" dir="));
    Serial.print(latestWind.compass);
    if (!latestWind.valid) {
      Serial.print(F("  (!) magnet not detected"));
    } else if (latestWind.tooWeak) {
      Serial.print(F("  (!) magnet too weak/far"));
    } else if (latestWind.tooStrong) {
      Serial.print(F("  (!) magnet too strong/close"));
    }
    Serial.println();
  }

  // Runs every loop iteration (not interval-gated) so tip counts move
  // from the ISR into the rolling buckets promptly and the minute/hour/
  // day rollovers happen on schedule.
  rainSensor.update();

  static unsigned long lastRainPrint = 0;
  if (now - lastRainPrint >= RAIN_PRINT_INTERVAL_MS) {
    lastRainPrint = now;
    rainSensor.printCompact();
  }

  // Firebase Realtime Sync & History Aggregation
  WindSpeedReading latestSpeed = getWindSpeedReading();
  RainReading latestRain = rainSensor.getReading();
  firebaseWeather.update(latestWind, latestSpeed, latestRain);
}
