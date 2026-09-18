// weather_station.ino
//
// Main sketch for an ESP-12E based weather recording device.
// Currently wires up the AS5600 wind-direction sensor (see
// wind_direction.h/.cpp) and prints readings to the Serial monitor.
// Designed to be extended with more sensors later (each as its own
// module, following the same pattern as wind_direction).
//
#include <Arduino.h>
#include "wind_direction_config.h"
#include "wind_direction.h"

WindDirection windSensor;

void setup()
{
  Serial.begin(SERIAL_BAUD);
  delay(200); // let the USB-serial bridge settle
  Serial.println();
  Serial.println(F("=== Weather station booting ==="));

  if (!windSensor.begin(I2C_SDA_PIN, I2C_SCL_PIN))
  {
    Serial.println(F("[wind] ERROR: AS5600 did not ACK on I2C. Check wiring/address (0x36)."));
  }
  else
  {
    Serial.println(F("[wind] AS5600 found on I2C bus."));
  }

  MagnetStatus mstat = windSensor.getMagnetStatus();
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

  if (!mstat.detected)
  {
    Serial.println(F("[wind] -> No magnet seen at all. Check: it must be DIAMETRICALLY magnetized (poles side-to-side across the disc, not stacked top/bottom), centered directly over the IC, and within roughly 0.5-3mm air gap."));
  }
  else if (mstat.tooWeak)
  {
    Serial.println(F("[wind] -> Magnet detected but weak/far - move it closer to the sensor or use a stronger magnet."));
  }
  else if (mstat.tooStrong)
  {
    Serial.println(F("[wind] -> Magnet detected but too strong/close - increase the air gap slightly."));
  }
  else
  {
    Serial.println(F("[wind] -> Magnet looks well positioned."));
  }

#if CALIBRATE_ON_BOOT
  float startRaw = windSensor.recordStartupDirection();
  if (startRaw < 0)
  {
    Serial.println(F("[wind] Startup calibration skipped: no magnet seen."));
  }
  else
  {
    Serial.print(F("[wind] Startup direction recorded: "));
    Serial.print(startRaw, 1);
    Serial.println(F(" deg raw -> this position is now the 0 deg / N reference."));
  }
#else
  Serial.print(F("[wind] Using fixed calibration offset from wind_direction_config.h: "));
  Serial.print((float)WIND_DIR_OFFSET_DEG, 1);
  Serial.println(F(" deg. Point the vane north and confirm deg reads ~0 below."));
#endif

  Serial.println(F("=== Setup complete, starting readings ==="));
}

void loop()
{
  static unsigned long lastSample = 0;
  unsigned long now = millis();

  if (now - lastSample >= SAMPLE_INTERVAL_MS)
  {
    lastSample = now;

    WindReading w = windSensor.read();

    Serial.print(F("[wind] raw="));
    Serial.print(w.rawAngle);
    Serial.print(F(" deg="));
    Serial.print(w.degrees, 1);
    Serial.print(F(" dir="));
    Serial.print(w.compass);
    if (!w.valid)
    {
      Serial.print(F("  (!) magnet not detected"));
    }
    else if (w.tooWeak)
    {
      Serial.print(F("  (!) magnet too weak/far"));
    }
    else if (w.tooStrong)
    {
      Serial.print(F("  (!) magnet too strong/close"));
    }
    Serial.println();
  }
}
