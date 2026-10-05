// rain_gauge.cpp
//
// Implementation of RainGauge class with debouncing, rolling time-window
// accumulators, LittleFS persistence, and diagnostic outputs.
//

#include "rain_gauge.h"

#if RAIN_ENABLE_STORAGE
#include <LittleFS.h>
#endif

RainGauge rainSensor;

// Global ISR router for ESP8266
static void IRAM_ATTR globalRainISR()
{
    rainSensor.onBucketTip();
}

RainGauge::RainGauge()
    : _pin(RAIN_SENSOR_PIN),
      _mmPerTip(RAIN_MM_PER_TIP),
      _rawTips(0),
      _lastTipTime(0),
      _lastPinState(HIGH),
      _lastProcessedTipTime(0),
      _lastTipIntervalMs(0),
      _lifetimeTips(0),
      _currentMinuteIndex(0),
      _lastMinuteTick(0),
      _currentHourIndex(0),
      _lastHourTick(0),
      _currentDayIndex(0),
      _lastDayTick(0),
      _lastFlashSaveTime(0),
      _historyDirty(false)
{
    memset(_minuteTips, 0, sizeof(_minuteTips));
    memset(_hourlyRainMm, 0, sizeof(_hourlyRainMm));
    memset(_dailyRainMm, 0, sizeof(_dailyRainMm));
}

void IRAM_ATTR RainGauge::onBucketTip()
{
    unsigned long now = millis();
    // Hardware/magnet debounce check in ISR
    if (now - _lastTipTime >= RAIN_DEBOUNCE_MS)
    {
        _rawTips++;
        _lastTipTime = now;
    }
}

bool RainGauge::begin(int pin, float mmPerTip)
{
    _pin = pin;
    _mmPerTip = mmPerTip;

    pinMode(_pin, INPUT_PULLUP);
    _lastPinState = digitalRead(_pin);
    attachInterrupt(digitalPinToInterrupt(_pin), globalRainISR, CHANGE);

    unsigned long now = millis();
    _lastMinuteTick = now;
    _lastHourTick = now;
    _lastDayTick = now;
    _lastFlashSaveTime = now;
    // _lastTipTime defaults to 0 from the constructor, which is ambiguous
    // with a genuine early millis() reading right after boot - seeding it here
    // guarantees the first real tip is never spuriously rejected.
    _lastTipTime = now - RAIN_DEBOUNCE_MS;

#if RAIN_ENABLE_STORAGE
    if (LittleFS.begin())
    {
        loadFromStorage();
    }
    else
    {
        Serial.println(F("[rain] LittleFS mount failed. Formatting storage..."));
        LittleFS.format();
        LittleFS.begin();
    }
#endif

    return true;
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

    // Secondary fallback: check pin state change to catch tips even if
    // an interrupt edge was missed or suppressed by hardware resting state.
    int currentPinState = digitalRead(_pin);
    if (currentPinState != _lastPinState)
    {
        _lastPinState = currentPinState;
        if (now - _lastTipTime >= RAIN_DEBOUNCE_MS)
        {
            _rawTips++;
            _lastTipTime = now;
        }
    }

    // Atomically grab tips count from ISR / polling
    noInterrupts();
    uint32_t currentRaw = _rawTips;
    _rawTips = 0;
    interrupts();

    if (currentRaw > 0)
    {
        _lifetimeTips += currentRaw;
        _minuteTips[_currentMinuteIndex] += currentRaw;

        float rainMm = currentRaw * _mmPerTip;
        _hourlyRainMm[_currentHourIndex] += rainMm;
        _dailyRainMm[_currentDayIndex] += rainMm;

        if (_lastProcessedTipTime > 0)
        {
            _lastTipIntervalMs = now - _lastProcessedTipTime;
        }
        _lastProcessedTipTime = now;
        _historyDirty = true;
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
        _historyDirty = true;
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
        _historyDirty = true;
    }

#if RAIN_ENABLE_STORAGE
    // Commit to flash periodically if modified
    if (_historyDirty && (now - _lastFlashSaveTime >= RAIN_SAVE_INTERVAL_MS))
    {
        saveToStorage();
        _lastFlashSaveTime = now;
        _historyDirty = false;
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

#if RAIN_ENABLE_STORAGE

struct PersistentRainData
{
    uint32_t magic;
    uint32_t lifetimeTips;
    float hourlyRainMm[RAIN_HOURLY_HISTORY_HOURS];
    float dailyRainMm[RAIN_DAILY_HISTORY_DAYS];
    uint8_t currentHourIndex;
    uint8_t currentDayIndex;
};

static const uint32_t RAIN_MAGIC = 0x5241494E; // "RAIN"

void RainGauge::loadFromStorage()
{
    if (!LittleFS.exists("/rain_data.bin"))
    {
        Serial.println(F("[rain] No existing history file on flash. Starting fresh."));
        return;
    }

    File f = LittleFS.open("/rain_data.bin", "r");
    if (!f)
    {
        Serial.println(F("[rain] Failed to open history file for reading."));
        return;
    }

    PersistentRainData data;
    if (f.read((uint8_t *)&data, sizeof(data)) == sizeof(data))
    {
        if (data.magic == RAIN_MAGIC)
        {
            _lifetimeTips = data.lifetimeTips;
            _currentHourIndex = data.currentHourIndex % RAIN_HOURLY_HISTORY_HOURS;
            _currentDayIndex = data.currentDayIndex % RAIN_DAILY_HISTORY_DAYS;
            memcpy(_hourlyRainMm, data.hourlyRainMm, sizeof(_hourlyRainMm));
            memcpy(_dailyRainMm, data.dailyRainMm, sizeof(_dailyRainMm));
            Serial.println(F("[rain] Loaded saved history from flash storage successfully."));
        }
    }
    f.close();
}

void RainGauge::saveToStorage()
{
    File f = LittleFS.open("/rain_data.bin", "w");
    if (!f)
    {
        Serial.println(F("[rain] Error writing history to flash."));
        return;
    }

    PersistentRainData data;
    data.magic = RAIN_MAGIC;
    data.lifetimeTips = _lifetimeTips;
    data.currentHourIndex = _currentHourIndex;
    data.currentDayIndex = _currentDayIndex;
    memcpy(data.hourlyRainMm, _hourlyRainMm, sizeof(_hourlyRainMm));
    memcpy(data.dailyRainMm, _dailyRainMm, sizeof(_dailyRainMm));

    f.write((const uint8_t *)&data, sizeof(data));
    f.close();
    Serial.println(F("[rain] History successfully saved to flash."));
}

#else

void RainGauge::loadFromStorage() {}
void RainGauge::saveToStorage() {}

#endif

void RainGauge::forceSave()
{
    saveToStorage();
}

void RainGauge::clearHistory()
{
    _lifetimeTips = 0;
    memset(_minuteTips, 0, sizeof(_minuteTips));
    memset(_hourlyRainMm, 0, sizeof(_hourlyRainMm));
    memset(_dailyRainMm, 0, sizeof(_dailyRainMm));
    saveToStorage();
    Serial.println(F("[rain] Rain history cleared."));
}