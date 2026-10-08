// rain_gauge.h
//
// Driver for Hall Effect Tipping Bucket Rain Gauge on ESP-12E (ESP8266).
// Tracks cumulative rain, last hour, today (24h), this week (7d), this month (30d),
// and current rain rate in millimeters. All data is kept in RAM only and starts
// from zero on every reset.
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

    // Initializes pin and interrupt handler, and starts all counters from zero.
    // Also deletes any history file an older version left on the flash.
    bool begin(int pin = RAIN_SENSOR_PIN, float mmPerTip = RAIN_MM_PER_TIP);

    // Periodic housekeeping: turns captured pin edges into counted tips, rolls the
    // minute/hourly/daily history buffers, and prints a reading when a tip is counted.
    // Call regularly in loop().
    void update();

    // Returns the latest computed readings matching the reference English dashboard
    RainReading getReading();

    // Print formatted status and measurements to Serial or any Stream
    void printSummary(Stream &out = Serial);

    // Print one compact, aligned line (rate/today/week/month/total/tips)
    // to Serial or any Stream - meant to be called on a short interval
    // from loop() for an at-a-glance, continuously updating reading,
    // matching the [wind]/[speed] tag style used by the other sensors.
    void printCompact(Stream &out = Serial);

    // Returns historical rain records as JSON for Web UI / API
    String getHistoryJson();

    // Reset counters and clear history
    void clearHistory();

    // Interrupt service routine callback: only records the edge (time + level)
    void IRAM_ATTR onPinEdge();

private:
    enum
    {
        EDGE_QUEUE_SIZE = 32, // must be a power of two
        EDGE_QUEUE_MASK = EDGE_QUEUE_SIZE - 1
    };

    int _pin;
    float _mmPerTip;

    // Pin edges captured by the ISR, consumed by update()
    volatile unsigned long _edgeTime[EDGE_QUEUE_SIZE];
    volatile uint8_t _edgeLevel[EDGE_QUEUE_SIZE];
    volatile uint8_t _edgeHead;
    volatile uint8_t _edgeTail;
    volatile uint16_t _edgesDropped;

    int _lastSeenLevel;
    unsigned long _lastEdgeTime;
    bool _haveEdge;
    unsigned long _lastProcessedTipTime;
    unsigned long _lastTipIntervalMs;

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

    bool handleEdge(unsigned long t, int level, bool polled);
    void removeLegacyStorage();
    void shiftMinute();
    void shiftHour();
    void shiftDay();
};

extern RainGauge rainSensor;

#endif // RAIN_GAUGE_H