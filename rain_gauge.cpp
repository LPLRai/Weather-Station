// rain_gauge.cpp
//
// Implementation of RainGauge class with edge capture, debouncing, rolling
// time-window accumulators (RAM only), and diagnostic outputs.
//

#include "rain_gauge.h"
#include "rain_gauge_config.h"
#include <LittleFS.h>

RainGauge rainSensor;

// File an older version used to keep history on flash.
static const char *RAIN_LEGACY_FILE = "/rain_data.bin";

// Global ISR router for ESP8266
static void IRAM_ATTR globalRainISR()
{
    rainSensor.onPinEdge();
}

RainGauge::RainGauge()
    : _pin(RAIN_SENSOR_PIN),
      _mmPerTip(RAIN_MM_PER_TIP),
      _edgeHead(0),
      _edgeTail(0),
      _edgesDropped(0),
      _lastSeenLevel(HIGH),
      _lastEdgeTime(0),
      _haveEdge(false),
      _lastProcessedTipTime(0),
      _lastTipIntervalMs(0),
      _lifetimeTips(0),
      _currentMinuteIndex(0),
      _lastMinuteTick(0),
      _currentHourIndex(0),
      _lastHourTick(0),
      _currentDayIndex(0),
      _lastDayTick(0)
{
    memset(_minuteTips, 0, sizeof(_minuteTips));
    memset(_hourlyRainMm, 0, sizeof(_hourlyRainMm));
    memset(_dailyRainMm, 0, sizeof(_dailyRainMm));
}

// ISR: only record when the pin changed and what level it has now. All the
// decisions (debounce, counting) happen in update(), using these timestamps,
// so a slow loop() iteration can never change the result.
void IRAM_ATTR RainGauge::onPinEdge()
{
    uint8_t head = _edgeHead;
    uint8_t next = (head + 1) & EDGE_QUEUE_MASK;
    if (next == _edgeTail)
    {
        _edgesDropped++;
        return;
    }
    _edgeTime[head] = millis();
    _edgeLevel[head] = digitalRead(_pin) ? 1 : 0;
    _edgeHead = next;
}

bool RainGauge::begin(int pin, float mmPerTip)
{
    _pin = pin;
    _mmPerTip = mmPerTip;

    // Everything is kept in RAM; make sure no old saved totals can come back.
    removeLegacyStorage();

    pinMode(_pin, INPUT_PULLUP);
    _lastSeenLevel = digitalRead(_pin);

    unsigned long now = millis();
    _lastMinuteTick = now;
    _lastHourTick = now;
    _lastDayTick = now;

    noInterrupts();
    _edgeHead = 0;
    _edgeTail = 0;
    _edgesDropped = 0;
    interrupts();
    _haveEdge = false;
    attachInterrupt(digitalPinToInterrupt(_pin), globalRainISR, CHANGE);

    // Report the starting state (all zero) right away.
    printCompact(Serial);

    return true;
}

void RainGauge::removeLegacyStorage()
{
    // Never format or create a filesystem just to look for the old file.
    LittleFSConfig cfg;
    cfg.setAutoFormat(false);
    LittleFS.setConfig(cfg);

    if (!LittleFS.begin())
    {
        return;
    }

    if (LittleFS.exists(RAIN_LEGACY_FILE))
    {
        if (LittleFS.remove(RAIN_LEGACY_FILE))
        {
            Serial.println(F("[rain] Removed old saved rain history from flash."));
        }
        else
        {
            Serial.println(F("[rain] Could not remove old saved rain history from flash."));
        }
    }
    LittleFS.end();
}

// Decides whether one pin edge is a new bucket tip. Edges within
// RAIN_DEBOUNCE_MS of the previous edge belong to the same tip.
bool RainGauge::handleEdge(unsigned long t, int level, bool polled)
{
    bool newTip = true;
    bool firstEdge = !_haveEdge;
    long gap = 0;

    if (_haveEdge)
    {
        gap = (long)(t - _lastEdgeTime);
        if (gap < 0)
            gap = 0; // edge stamped slightly before one already processed
        newTip = (unsigned long)gap >= RAIN_DEBOUNCE_MS;
    }

    if (!_haveEdge || (long)(t - _lastEdgeTime) > 0)
    {
        _lastEdgeTime = t;
    }
    _haveEdge = true;
    _lastSeenLevel = level;

#if RAIN_DEBUG_EDGES
    Serial.print(F("[rain] edge: pin "));
    Serial.print(level ? F("HIGH") : F("LOW "));
    if (firstEdge)
    {
        Serial.print(F(" (first edge since boot)"));
    }
    else
    {
        Serial.print(F(" (+"));
        Serial.print((unsigned long)gap);
        Serial.print(F(" ms)"));
    }
    if (polled)
    {
        Serial.print(F(" [found by polling]"));
    }
    Serial.println(newTip ? F(" -> TIP counted") : F(" -> ignored, same tip"));
#endif

    return newTip;
}

void RainGauge::shiftMinute()
{
    _currentMinuteIndex = (_currentMinuteIndex + 1) % 60;
    _minuteTips[_currentMinuteIndex] = 0;
}

void RainGauge::shiftHour()
{
    _currentHourIndex = (_currentHourIndex + 1) % RAIN_HOURLY_HISTORY_HOURS;
    _hourlyRainMm[_currentHourIndex] = 0.0f;
}

void RainGauge::shiftDay()
{
    _currentDayIndex = (_currentDayIndex + 1) % RAIN_DAILY_HISTORY_DAYS;
    _dailyRainMm[_currentDayIndex] = 0.0f;
}

void RainGauge::update()
{
    unsigned long now = millis();
    uint32_t newTips = 0;

    // Turn the edges captured by the ISR into tips
    while (_edgeTail != _edgeHead)
    {
        uint8_t i = _edgeTail;
        unsigned long t = _edgeTime[i];
        int level = _edgeLevel[i];
        _edgeTail = (i + 1) & EDGE_QUEUE_MASK;
        if (handleEdge(t, level, false))
        {
            newTips++;
        }
    }

    if (_edgesDropped > 0)
    {
        noInterrupts();
        uint16_t dropped = _edgesDropped;
        _edgesDropped = 0;
        interrupts();
        Serial.print(F("[rain] WARNING: "));
        Serial.print(dropped);
        Serial.println(F(" pin edges lost (queue full), the signal is extremely noisy."));
    }

    // Secondary fallback: if the pin level differs from the last edge seen and no
    // interrupt edge is waiting, an edge was missed. Feed it through the same logic,
    // so it can never be counted twice for the same tip.
    int currentPinState = digitalRead(_pin);
    if (currentPinState != _lastSeenLevel && _edgeTail == _edgeHead)
    {
        if (handleEdge(millis(), currentPinState, true))
        {
            newTips++;
        }
    }

    if (newTips > 0)
    {
        _lifetimeTips += newTips;
        _minuteTips[_currentMinuteIndex] += newTips;

        float rainMm = newTips * _mmPerTip;
        _hourlyRainMm[_currentHourIndex] += rainMm;
        _dailyRainMm[_currentDayIndex] += rainMm;

        if (_lastProcessedTipTime > 0)
        {
            _lastTipIntervalMs = now - _lastProcessedTipTime;
        }
        _lastProcessedTipTime = now;
    }

    // Handle minute tick (60,000 ms) safely without WDT starvation
    if (now - _lastMinuteTick >= 60000UL)
    {
        unsigned long elapsedMinutes = (now - _lastMinuteTick) / 60000UL;
        if (elapsedMinutes > 60)
            elapsedMinutes = 60;
        for (unsigned long i = 0; i < elapsedMinutes; i++)
        {
            shiftMinute();
        }
        _lastMinuteTick = now;
    }

    // Handle hour tick (3,600,000 ms) safely
    if (now - _lastHourTick >= 3600000UL)
    {
        unsigned long elapsedHours = (now - _lastHourTick) / 3600000UL;
        if (elapsedHours > RAIN_HOURLY_HISTORY_HOURS)
            elapsedHours = RAIN_HOURLY_HISTORY_HOURS;
        for (unsigned long i = 0; i < elapsedHours; i++)
        {
            shiftHour();
        }
        _lastHourTick = now;
    }

    // Handle day tick (86,400,000 ms) safely
    if (now - _lastDayTick >= 86400000UL)
    {
        unsigned long elapsedDays = (now - _lastDayTick) / 86400000UL;
        if (elapsedDays > RAIN_DAILY_HISTORY_DAYS)
            elapsedDays = RAIN_DAILY_HISTORY_DAYS;
        for (unsigned long i = 0; i < elapsedDays; i++)
        {
            shiftDay();
        }
        _lastDayTick = now;
    }

#if RAIN_PRINT_ON_TIP
    // Live update: report the new totals as soon as a tip is counted.
    if (newTips > 0)
    {
        printCompact(Serial);
    }
#endif
}

RainReading RainGauge::getReading()
{
    RainReading r;
    r.totalTips = _lifetimeTips;
    r.totalMm = _lifetimeTips * _mmPerTip;

    // Sum last 60 minutes for last hour
    uint32_t pastHourTips = 0;
    for (int i = 0; i < 60; i++)
    {
        pastHourTips += _minuteTips[i];
    }
    r.lastHourMm = pastHourTips * _mmPerTip;

    // Sum past 24 hours for Today
    float sum24h = 0.0f;
    for (int i = 0; i < RAIN_HOURLY_HISTORY_HOURS; i++)
    {
        sum24h += _hourlyRainMm[i];
    }
    r.todayMm = sum24h;

    // Sum past 7 days for This Week
    float sum7d = 0.0f;
    for (int i = 0; i < 7 && i < RAIN_DAILY_HISTORY_DAYS; i++)
    {
        int idx = (_currentDayIndex - i + RAIN_DAILY_HISTORY_DAYS) % RAIN_DAILY_HISTORY_DAYS;
        sum7d += _dailyRainMm[idx];
    }
    r.weekMm = sum7d;

    // Sum past 30 days for This Month
    float sum30d = 0.0f;
    for (int i = 0; i < RAIN_DAILY_HISTORY_DAYS; i++)
    {
        sum30d += _dailyRainMm[i];
    }
    r.monthMm = sum30d;

    // Rain rate: instantaneous mm/hour based on last 10 minutes
    uint32_t past10MinTips = 0;
    for (int i = 0; i < 10; i++)
    {
        int idx = (_currentMinuteIndex - i + 60) % 60;
        past10MinTips += _minuteTips[idx];
    }
    r.rainRateMmPerHour = (past10MinTips * _mmPerTip) * 6.0f; // 10 min * 6 = 1 hour

    // Active rain flag: true if tip recorded in the last 15 minutes
    unsigned long now = millis();
    r.isRaining = (now - _lastProcessedTipTime < 900000UL) && (_lastProcessedTipTime > 0);

    return r;
}

void RainGauge::printSummary(Stream &out)
{
    RainReading r = getReading();

    out.println(F("========== RAIN GAUGE SUMMARY =========="));
    out.print(F("Current / Total Rain : "));
    out.print(r.totalMm, 2);
    out.println(F(" mm"));

    out.print(F("Last Hour (Ultima ora): "));
    out.print(r.lastHourMm, 2);
    out.println(F(" mm"));

    out.print(F("Today 24h (Oggi)     : "));
    out.print(r.todayMm, 2);
    out.println(F(" mm"));

    out.print(F("This Week (Settimana): "));
    out.print(r.weekMm, 2);
    out.println(F(" mm"));

    out.print(F("This Month (Mese)    : "));
    out.print(r.monthMm, 2);
    out.println(F(" mm"));

    out.print(F("Rain Rate            : "));
    out.print(r.rainRateMmPerHour, 2);
    out.println(F(" mm/h"));

    out.print(F("Status               : "));
    out.println(r.isRaining ? F("RAINING NOW") : F("No Rain"));
    out.println(F("========================================="));
}

void RainGauge::printCompact(Stream &out)
{
    RainReading r = getReading();
    char line[160];
    snprintf(line, sizeof(line),
             "[rain] rate:%6.2f mm/h  today:%7.2f mm  week:%8.2f mm  month:%8.2f mm  total:%8.2f mm  tips:%6lu  %s",
             r.rainRateMmPerHour, r.todayMm, r.weekMm, r.monthMm, r.totalMm,
             (unsigned long)r.totalTips, r.isRaining ? "RAINING" : "dry");
    out.println(line);
}

String RainGauge::getHistoryJson()
{
    RainReading r = getReading();
    String json = "{";
    json += "\"totalMm\":" + String(r.totalMm, 2) + ",";
    json += "\"lastHourMm\":" + String(r.lastHourMm, 2) + ",";
    json += "\"todayMm\":" + String(r.todayMm, 2) + ",";
    json += "\"weekMm\":" + String(r.weekMm, 2) + ",";
    json += "\"monthMm\":" + String(r.monthMm, 2) + ",";
    json += "\"rainRateMmPerHour\":" + String(r.rainRateMmPerHour, 2) + ",";
    json += "\"totalTips\":" + String(r.totalTips) + ",";
    json += "\"isRaining\":" + String(r.isRaining ? "true" : "false") + ",";

    // 24 Hour History Array
    json += "\"hourly\":[";
    for (int i = 0; i < RAIN_HOURLY_HISTORY_HOURS; i++)
    {
        int idx = (_currentHourIndex - i + RAIN_HOURLY_HISTORY_HOURS) % RAIN_HOURLY_HISTORY_HOURS;
        json += String(_hourlyRainMm[idx], 2);
        if (i < RAIN_HOURLY_HISTORY_HOURS - 1)
            json += ",";
    }
    json += "],";

    // 30 Day History Array
    json += "\"daily\":[";
    for (int i = 0; i < RAIN_DAILY_HISTORY_DAYS; i++)
    {
        int idx = (_currentDayIndex - i + RAIN_DAILY_HISTORY_DAYS) % RAIN_DAILY_HISTORY_DAYS;
        json += String(_dailyRainMm[idx], 2);
        if (i < RAIN_DAILY_HISTORY_DAYS - 1)
            json += ",";
    }
    json += "]";
    json += "}";
    return json;
}

void RainGauge::clearHistory()
{
    _lifetimeTips = 0;
    memset(_minuteTips, 0, sizeof(_minuteTips));
    memset(_hourlyRainMm, 0, sizeof(_hourlyRainMm));
    memset(_dailyRainMm, 0, sizeof(_dailyRainMm));
    Serial.println(F("[rain] Rain history cleared."));
}