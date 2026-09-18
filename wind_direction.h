// wind_direction.h
//
// Driver for an AS5600 12-bit contactless magnetic rotary sensor, used
// here as a wind vane angle sensor. A diametrically-magnetized magnet
// is mounted on the vane shaft directly above the AS5600 IC; the chip
// reports the magnet's absolute angle over I2C.
//
#ifndef WIND_DIRECTION_H
#define WIND_DIRECTION_H

#include <Arduino.h>

struct WindReading
{
  bool valid;          // false if the magnet wasn't detected this read (MD)
  bool tooWeak;        // magnet detected but too weak/far (ML)
  bool tooStrong;      // magnet detected but too strong/close (MH)
  uint16_t rawAngle;   // 0-4095 straight from the sensor (12-bit)
  float degrees;       // 0.0-359.99, calibrated compass heading
  const char *compass; // "N", "NNE", "NE", ... (16-point compass)
};

// Full magnet-positioning health check, for boot-time diagnostics.
struct MagnetStatus
{
  bool detected;      // MD - a magnet is in range at all
  bool tooWeak;       // ML - magnet too weak / too far away
  bool tooStrong;     // MH - magnet too strong / too close
  uint8_t agc;        // Automatic Gain Control, 0-255 (0-128 @ 3.3V).
                      // Near either end of its range means the sensor
                      // is compensating hard - a sign the air gap is off.
  uint16_t magnitude; // CORDIC signal magnitude. ~2000-2200 is typical
                      // for a well-positioned magnet; near 0 means the
                      // sensor barely sees a field at all.
};

class WindDirection
{
public:
  explicit WindDirection(uint8_t i2cAddress = 0x36);

  // Starts I2C on the given pins and pings the sensor's address.
  // Returns false if nothing ACKs (wiring / address problem).
  bool begin(int sdaPin, int sclPin);

  // Reads the STATUS register and returns true only when the magnet
  // is within the sensor's usable range (bit MD set).
  bool isMagnetDetected();

  // Full diagnostic snapshot (status bits + AGC + magnitude). Costs a
  // couple of extra I2C transactions, so main.ino calls this once at
  // boot to help you position the magnet, not on every loop iteration.
  MagnetStatus getMagnetStatus();

  // Takes one full reading: raw count + calibrated degrees + compass label.
  WindReading read();

  // Treats whatever direction the vane is CURRENTLY pointing at as the
  // new zero-degree ("N") reference, by averaging a few samples and
  // storing the offset needed to cancel it out. main.ino calls this
  // once in setup() so the direction recorded at power-on becomes the
  // baseline ("record the direction when started").
  // Returns the raw (uncalibrated) heading that was captured, in degrees.
  float recordStartupDirection();

  // ---- Pure helpers (no hardware access) exposed for unit testing ----

  // Converts a raw 0-4095 AS5600 count into 0-359.99 degrees.
  static float rawToDegrees(uint16_t raw);

  // Applies offset + optional inversion and wraps the result into [0, 360).
  static float applyCalibration(float rawDegrees, float offsetDeg, bool invert);

  // Maps 0-359.99 degrees onto a 16-point compass label.
  static const char *degreesToCompass(float degrees);

private:
  uint8_t _addr;
  float _offsetDeg;

  uint8_t readReg8(uint8_t reg);
  uint16_t readReg16(uint8_t highReg);
  uint8_t readStatusRegister();
  uint16_t readRawAngleRegister();
};

#endif // WIND_DIRECTION_H