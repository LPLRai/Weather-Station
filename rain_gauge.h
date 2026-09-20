// rain_gauge.h
//
// Driver for Hall Effect Tipping Bucket Rain Gauge on ESP-12E (ESP8266).
// Tracks cumulative rain, last hour, today (24h), this week (7d), this month (30d),
// and current rain rate in millimeters with flash persistence (LittleFS).
//

#ifndef RAIN_GAUGE_H
#define RAIN_GAUGE_H

#include <Arduino.h>
#include "rain_gauge_config.h"

struct RainReading
{
  float totalMm;           // Total recorded precipitation since reset
  float lastHourMm;        // Rain accumulated in the last 60 minutes
  float todayMm;           // Rain accumulated in the last 24 hours
  float weekMm;            // Rain accumulated in the last 7 days
  float monthMm;           // Rain accumulated in the last 30 days
  float rainRateMmPerHour; // Current instantaneous rain rate (mm/h)
  uint32_t totalTips;      // Total number of bucket tips
  bool isRaining;          // True if a tip occurred in the last 15 minutes
};

class RainGauge
{
public:
  RainGauge();

  // Initializes pin, interrupt handler, and restores historical data from LittleFS
  bool begin(int pin = RAIN_SENSOR_PIN, float mmPerTip = RAIN_MM_PER_TIP);

  // Periodic housekeeping: processes new tips, rolls hourly/daily history buffers,
  // and saves data to flash periodically. Call regularly in loop().
  void update();

  // Returns the latest computed readings matching the reference English dashboard
  RainReading getReading();

  // Print formatted status and measurements to Serial or any Stream
  void printSummary(Stream &out = Serial);

  // Returns historical rain records as JSON for Web UI / API
  String getHistoryJson();

  // Reset counters and clear saved history
  void clearHistory();

  // Save current history to flash storage immediately
  void forceSave();

  // Interrupt service routine callback
  void ICACHE_RAM_ATTR onBucketTip();

private:
  int _pin;
  float _mmPerTip;
  volatile uint32_t _rawTips;
  volatile unsigned long _lastTipTime;
  unsigned long _lastProcessedTipTime;

  uint32_t _lifetimeTips;

  // Rolling 1-minute buckets for the last 60 minutes (60 buckets)
  uint16_t _minuteTips[60];
  uint8_t _currentMinuteIndex;
  unsigned long _lastMinuteTick;

  // Rolling 1-hour buckets for the last 24 hours (24 buckets)
  float _hourlyRainMm[RAIN_HOURLY_HISTORY_HOURS];
  uint8_t _currentHourIndex;
  unsigned long _lastHourTick;

  // Rolling daily buckets for the last 30 days (30 buckets)
  float _dailyRainMm[RAIN_DAILY_HISTORY_DAYS];
  uint8_t _currentDayIndex;
  unsigned long _lastDayTick;

  unsigned long _lastFlashSaveTime;
  bool _historyDirty;

  void shiftMinute();
  void shiftHour();
  void shiftDay();
  void loadFromStorage();
  void saveToStorage();
};

extern RainGauge rainSensor;

#endif // RAIN_GAUGE_H
