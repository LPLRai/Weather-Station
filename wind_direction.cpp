// wind_direction.cpp
//
// See wind_direction.h for the interface and a short explanation. This
// file implements register-level I2C access to the AS5600, plus the
// pure math (angle scaling, calibration, compass mapping).
//
// AS5600 register map (from the AMS datasheet, address is always 0x36):
//   0x0B        STATUS      bit5=MD magnet detected, bit4=ML too weak,
//                            bit3=MH too strong
//   0x0C..0x0D  RAW_ANGLE   12-bit, unscaled angle, 0-4095 for 360 deg
//   0x0E..0x0F  ANGLE       12-bit, scaled/filtered (unused here)
//   0x1A        AGC         automatic gain control (diagnostic)
//   0x1B..0x1C  MAGNITUDE   CORDIC magnitude (diagnostic)
//
#include "wind_direction.h"
#include "wind_direction_config.h"
#include <Wire.h>
#include <math.h>

static const uint8_t REG_STATUS = 0x0B;
static const uint8_t REG_RAW_ANGLE = 0x0C; // + 0x0D low byte
static const uint8_t REG_AGC = 0x1A;
static const uint8_t REG_MAGNITUDE = 0x1B; // + 0x1C low byte

static const uint8_t STATUS_MH_BIT = 3; // magnet too strong
static const uint8_t STATUS_ML_BIT = 4; // magnet too weak
static const uint8_t STATUS_MD_BIT = 5; // magnet detected

static const char *COMPASS_POINTS[16] = {
    "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
    "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};

WindDirection::WindDirection(uint8_t i2cAddress)
    : _addr(i2cAddress), _offsetDeg(WIND_DIR_OFFSET_DEG) {}

bool WindDirection::begin(int sdaPin, int sclPin)
{
  Wire.begin(sdaPin, sclPin);
  Wire.beginTransmission(_addr);
  uint8_t err = Wire.endTransmission();
  return (err == 0);
}

uint8_t WindDirection::readReg8(uint8_t reg)
{
  Wire.beginTransmission(_addr);
  Wire.write(reg);
  Wire.endTransmission(false); // repeated start, keep the bus
  Wire.requestFrom((int)_addr, 1);
  if (Wire.available() < 1)
    return 0;
  return Wire.read();
}

uint16_t WindDirection::readReg16(uint8_t highReg)
{
  Wire.beginTransmission(_addr);
  Wire.write(highReg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)_addr, 2);
  if (Wire.available() < 2)
    return 0;
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  return ((uint16_t)(hi & 0x0F) << 8) | lo; // top nibble of hi is reserved
}

uint8_t WindDirection::readStatusRegister()
{
  return readReg8(REG_STATUS);
}

uint16_t WindDirection::readRawAngleRegister()
{
  return readReg16(REG_RAW_ANGLE);
}

bool WindDirection::isMagnetDetected()
{
  return bitRead(readStatusRegister(), STATUS_MD_BIT);
}

MagnetStatus WindDirection::getMagnetStatus()
{
  MagnetStatus s;
  uint8_t status = readStatusRegister();
  s.detected = bitRead(status, STATUS_MD_BIT);
  s.tooWeak = bitRead(status, STATUS_ML_BIT);
  s.tooStrong = bitRead(status, STATUS_MH_BIT);
  s.agc = readReg8(REG_AGC);
  s.magnitude = readReg16(REG_MAGNITUDE);
  return s;
}

float WindDirection::rawToDegrees(uint16_t raw)
{
  // 12-bit count (0-4095) maps onto one full 360 deg turn.
  return (raw * 360.0f) / 4096.0f;
}

float WindDirection::applyCalibration(float rawDegrees, float offsetDeg, bool invert)
{
  float deg = invert ? (360.0f - rawDegrees) : rawDegrees;
  deg += offsetDeg;
  deg = fmodf(deg, 360.0f);
  if (deg < 0)
    deg += 360.0f;
  return deg;
}

const char *WindDirection::degreesToCompass(float degrees)
{
  // 16 points, 22.5 deg wide each, centered on the point itself
  // (so e.g. 348.75-360 and 0-11.24 both map to "N").
  int index = ((int)((degrees + 11.25f) / 22.5f)) % 16;
  if (index < 0)
    index += 16;
  return COMPASS_POINTS[index];
}

WindReading WindDirection::read()
{
  WindReading r;
  r.rawAngle = readRawAngleRegister();
  uint8_t status = readStatusRegister();
  r.valid = bitRead(status, STATUS_MD_BIT);
  r.tooWeak = bitRead(status, STATUS_ML_BIT);
  r.tooStrong = bitRead(status, STATUS_MH_BIT);

  float raw = rawToDegrees(r.rawAngle);
  r.degrees = applyCalibration(raw, _offsetDeg, WIND_DIR_INVERT);
  r.compass = degreesToCompass(r.degrees);
  return r;
}

float WindDirection::recordStartupDirection()
{
  // Average a handful of samples to smooth out sensor noise, then bake
  // the result into the offset so the vane's position right now reads
  // as WIND_DIR_OFFSET_DEG (0 deg / "N" by default) going forward.
  const int SAMPLES = 8;
  float sum = 0.0f;
  int good = 0;

  for (int i = 0; i < SAMPLES; i++)
  {
    uint16_t raw = readRawAngleRegister();
    bool detected = bitRead(readStatusRegister(), STATUS_MD_BIT);
    if (detected)
    {
      sum += rawToDegrees(raw);
      good++;
    }
    delay(10);
  }

  if (good == 0)
  {
    // Magnet not seen at all during calibration — leave the offset as
    // configured in config.h rather than guessing.
    return -1.0f;
  }

  float avgRaw = sum / good;
  _offsetDeg = WIND_DIR_OFFSET_DEG - avgRaw;
  return avgRaw;
}