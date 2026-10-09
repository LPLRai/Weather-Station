// firebase_weather.cpp
//
// Implementation of Firebase Realtime Database integration for ESP8266.

#include "firebase_weather.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

FirebaseWeather firebaseWeather;

FirebaseWeather::FirebaseWeather()
    : _lastLiveUpdate(0),
      _lastLogUpdate(0),
      _lastSummaryUpdate(0),
      _lastWifiCheck(0),
      _currentHour(-1),
      _currentDay(-1),
      _currentMonth(-1),
      _currentYear(-1),
      _hourRainAccum(0.0f),
      _hourWindSum(0.0f),
      _hourWindMax(0.0f),
      _hourSamples(0),
      _hourWindDirSum(0.0f),
      _dayRainAccum(0.0f),
      _dayWindMax(0.0f),
      _dayWindSum(0.0f),
      _daySamples(0),
      _monthRainAccum(0.0f),
      _monthWindMax(0.0f)
{
}

String FirebaseWeather::cleanHost(const String& rawHost) {
    String host = rawHost;
    host.trim();
    if (host.startsWith("https://")) {
        host = host.substring(8);
    } else if (host.startsWith("http://")) {
        host = host.substring(7);
    }
    if (host.endsWith("/")) {
        host = host.substring(0, host.length() - 1);
    }
    return host;
}

void FirebaseWeather::begin() {
#if !FIREBASE_ENABLED
    Serial.println(F("[firebase] Firebase disabled in firebase_config.h"));
    return;
#endif

    Serial.println(F("[firebase] Initializing Wi-Fi & NTP for Firebase..."));
    connectWifi();
    syncTime();
}

void FirebaseWeather::connectWifi() {
    if (WiFi.status() == WL_CONNECTED) return;

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print(F("[wifi] Connecting to SSID: "));
    Serial.println(WIFI_SSID);

    unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < 8000) {
        delay(250);
        Serial.print(F("."));
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print(F("[wifi] Connected! IP: "));
        Serial.print(WiFi.localIP());
        Serial.print(F(" | RSSI: "));
        Serial.print(WiFi.RSSI());
        Serial.println(F(" dBm"));
    } else {
        Serial.println(F("[wifi] Connection in progress; will retry in background."));
    }
}

void FirebaseWeather::syncTime() {
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.println(F("[ntp] Synchronizing time with NTP servers..."));
    configTime(TIMEZONE_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);

    time_t now = time(nullptr);
    int retries = 0;
    while (now < 8 * 3600 * 2 && retries < 20) {
        delay(100);
        now = time(nullptr);
        retries++;
    }

    if (now > 8 * 3600 * 2) {
        Serial.print(F("[ntp] Time synchronized! Current ISO time: "));
        Serial.println(getIsoTimestamp());
    } else {
        Serial.println(F("[ntp] Time sync pending... will update on next cycle."));
    }
}

bool FirebaseWeather::isWifiConnected() const {
    return (WiFi.status() == WL_CONNECTED);
}

bool FirebaseWeather::isTimeSynced() const {
    time_t now = time(nullptr);
    return (now > 1600000000); // Year 2020+
}

int FirebaseWeather::getWifiRssi() const {
    return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
}

unsigned long FirebaseWeather::getEpochMs() {
    time_t now = time(nullptr);
    return (unsigned long)now * 1000UL + (millis() % 1000UL);
}

String FirebaseWeather::getIsoTimestamp() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    char buf[30];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return String(buf);
}

String FirebaseWeather::getDateString() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char buf[16];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &timeinfo);
    return String(buf);
}

String FirebaseWeather::getMonthString() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char buf[16];
    strftime(buf, sizeof(buf), "%Y-%m", &timeinfo);
    return String(buf);
}

int FirebaseWeather::getHour() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_hour;
}

bool FirebaseWeather::sendFirebaseRequest(const String& method, const String& path, const String& jsonPayload) {
#if !FIREBASE_ENABLED
    return false;
#endif

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Bypass SSL cert validation on ESP8266 for maximum speed/RAM efficiency
    client.setTimeout(3000);

    HTTPClient http;
    String host = cleanHost(FIREBASE_HOST);
    String url = "https://" + host + "/" + FIREBASE_BASE_PATH + "/" + path + ".json";

    if (strlen(FIREBASE_AUTH) > 0) {
        url += "?auth=" + String(FIREBASE_AUTH);
    }

    if (!http.begin(client, url)) {
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    int httpCode = http.sendRequest(method.c_str(), (uint8_t*)jsonPayload.c_str(), jsonPayload.length());

    bool success = (httpCode >= 200 && httpCode < 300);
    if (!success && httpCode > 0) {
        Serial.print(F("[firebase] HTTP "));
        Serial.print(httpCode);
        if (httpCode == 401 || httpCode == 403) {
            Serial.print(F(" (Permission Denied - check Firebase Rules!)"));
        }
        Serial.print(F(" on "));
        Serial.println(path);
    }

    http.end();
    return success;
}

void FirebaseWeather::publishCurrent(const WindReading& wind, const WindSpeedReading& speed, const RainReading& rain) {
    String payload = "{";
    
    // Wind
    payload += "\"wind\":{";
    payload += "\"speed_kmh\":" + String(speed.speedKMH, 2) + ",";
    payload += "\"rpm\":" + String(speed.rpm, 1) + ",";
    payload += "\"rotations\":" + String(speed.rotations, 2) + ",";
    payload += "\"total_spins\":" + String(speed.totalSpins) + ",";
    payload += "\"total_pulses\":" + String(speed.totalPulses) + ",";
    payload += "\"degrees\":" + String(wind.degrees, 1) + ",";
    payload += "\"compass\":\"" + String(wind.compass) + "\",";
    payload += "\"raw_angle\":" + String(wind.rawAngle) + ",";
    payload += "\"magnet_valid\":" + String(wind.valid ? "true" : "false");
    payload += "},";

    // Rain
    payload += "\"rain\":{";
    payload += "\"rate_mm_h\":" + String(rain.rainRateMmPerHour, 2) + ",";
    payload += "\"last_hour_mm\":" + String(rain.lastHourMm, 2) + ",";
    payload += "\"today_mm\":" + String(rain.todayMm, 2) + ",";
    payload += "\"week_mm\":" + String(rain.weekMm, 2) + ",";
    payload += "\"month_mm\":" + String(rain.monthMm, 2) + ",";
    payload += "\"total_mm\":" + String(rain.totalMm, 2) + ",";
    payload += "\"total_tips\":" + String(rain.totalTips) + ",";
    payload += "\"is_raining\":" + String(rain.isRaining ? "true" : "false");
    payload += "},";

    // System
    payload += "\"system\":{";
    payload += "\"wifi_rssi\":" + String(getWifiRssi()) + ",";
    payload += "\"uptime_sec\":" + String(millis() / 1000UL) + ",";
    payload += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    payload += "\"timestamp\":" + String(getEpochMs()) + ",";
    payload += "\"timestamp_iso\":\"" + getIsoTimestamp() + "\"";
    payload += "}";

    payload += "}";

    sendFirebaseRequest("PATCH", "current", payload);
}

void FirebaseWeather::publishLog(const WindReading& wind, const WindSpeedReading& speed, const RainReading& rain) {
    if (!isTimeSynced()) return;

    unsigned long epochMs = getEpochMs();
    String path = "history/logs/" + String(epochMs);

    String payload = "{";
    payload += "\"t\":" + String(epochMs) + ",";
    payload += "\"iso\":\"" + getIsoTimestamp() + "\",";
    payload += "\"speed\":" + String(speed.speedKMH, 2) + ",";
    payload += "\"rpm\":" + String(speed.rpm, 1) + ",";
    payload += "\"dir\":" + String(wind.degrees, 1) + ",";
    payload += "\"cardinal\":\"" + String(wind.compass) + "\",";
    payload += "\"rain_rate\":" + String(rain.rainRateMmPerHour, 2) + ",";
    payload += "\"rain_today\":" + String(rain.todayMm, 2) + ",";
    payload += "\"rain_total\":" + String(rain.totalMm, 2);
    payload += "}";

    sendFirebaseRequest("PUT", path, payload);
}

void FirebaseWeather::updateAggregates(const WindReading& wind, const WindSpeedReading& speed, const RainReading& rain) {
    time_t now = time(nullptr);
    if (now < 1600000000) return;

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    int hour = timeinfo.tm_hour;
    int day = timeinfo.tm_mday;
    int month = timeinfo.tm_mon + 1;
    int year = timeinfo.tm_year + 1900;

    // Detect rollover of hour
    if (_currentHour != -1 && _currentHour != hour) {
        char prevDateBuf[16];
        strftime(prevDateBuf, sizeof(prevDateBuf), "%Y-%m-%d", &timeinfo);
        publishHourlySummary(String(prevDateBuf), _currentHour);

        // Reset hour accumulators
        _hourRainAccum = 0.0f;
        _hourWindSum = 0.0f;
        _hourWindMax = 0.0f;
        _hourSamples = 0;
        _hourWindDirSum = 0.0f;
    }

    // Detect rollover of day
    if (_currentDay != -1 && _currentDay != day) {
        char prevDateBuf[16];
        strftime(prevDateBuf, sizeof(prevDateBuf), "%Y-%m-%d", &timeinfo);
        publishDailySummary(String(prevDateBuf));

        _dayRainAccum = 0.0f;
        _dayWindMax = 0.0f;
        _dayWindSum = 0.0f;
        _daySamples = 0;
    }

    // Detect rollover of month
    if (_currentMonth != -1 && _currentMonth != month) {
        char prevMonthBuf[16];
        strftime(prevMonthBuf, sizeof(prevMonthBuf), "%Y-%m", &timeinfo);
        publishMonthlySummary(String(prevMonthBuf));

        _monthRainAccum = 0.0f;
        _monthWindMax = 0.0f;
    }

    _currentHour = hour;
    _currentDay = day;
    _currentMonth = month;
    _currentYear = year;

    // Accumulate samples
    _hourSamples++;
    _hourWindSum += speed.speedKMH;
    if (speed.speedKMH > _hourWindMax) _hourWindMax = speed.speedKMH;
    _hourWindDirSum += wind.degrees;

    _daySamples++;
    _dayWindSum += speed.speedKMH;
    if (speed.speedKMH > _dayWindMax) _dayWindMax = speed.speedKMH;

    if (speed.speedKMH > _monthWindMax) _monthWindMax = speed.speedKMH;
}

void FirebaseWeather::publishHourlySummary(const String& dateStr, int hour) {
    String hourStr = (hour < 10 ? "0" : "") + String(hour);
    String path = "history/hourly/" + dateStr + "/" + hourStr;

    float avgWind = _hourSamples > 0 ? (_hourWindSum / _hourSamples) : 0.0f;
    float avgDir = _hourSamples > 0 ? (_hourWindDirSum / _hourSamples) : 0.0f;

    String payload = "{";
    payload += "\"hour\":" + String(hour) + ",";
    payload += "\"rain_mm\":" + String(_hourRainAccum, 2) + ",";
    payload += "\"wind_avg_kmh\":" + String(avgWind, 2) + ",";
    payload += "\"wind_max_kmh\":" + String(_hourWindMax, 2) + ",";
    payload += "\"wind_dir_deg\":" + String(avgDir, 1) + ",";
    payload += "\"wind_dir_compass\":\"" + String(WindDirection::degreesToCompass(avgDir)) + "\",";
    payload += "\"samples\":" + String(_hourSamples) + ",";
    payload += "\"timestamp\":" + String(getEpochMs());
    payload += "}";

    sendFirebaseRequest("PUT", path, payload);
}

void FirebaseWeather::publishDailySummary(const String& dateStr) {
    String path = "history/daily/" + dateStr;

    float avgWind = _daySamples > 0 ? (_dayWindSum / _daySamples) : 0.0f;

    String payload = "{";
    payload += "\"date\":\"" + dateStr + "\",";
    payload += "\"rain_mm\":" + String(_dayRainAccum, 2) + ",";
    payload += "\"wind_avg_kmh\":" + String(avgWind, 2) + ",";
    payload += "\"wind_max_kmh\":" + String(_dayWindMax, 2) + ",";
    payload += "\"samples\":" + String(_daySamples) + ",";
    payload += "\"last_updated\":" + String(getEpochMs());
    payload += "}";

    sendFirebaseRequest("PATCH", path, payload);
}

void FirebaseWeather::publishMonthlySummary(const String& monthStr) {
    String path = "history/monthly/" + monthStr;

    String payload = "{";
    payload += "\"month\":\"" + monthStr + "\",";
    payload += "\"rain_mm\":" + String(_monthRainAccum, 2) + ",";
    payload += "\"wind_max_kmh\":" + String(_monthWindMax, 2) + ",";
    payload += "\"last_updated\":" + String(getEpochMs());
    payload += "}";

    sendFirebaseRequest("PATCH", path, payload);
}

void FirebaseWeather::update(const WindReading& wind, const WindSpeedReading& speed, const RainReading& rain) {
#if !FIREBASE_ENABLED
    return;
#endif

    unsigned long now = millis();

    // WiFi Auto-reconnect check (every 10s if disconnected)
    if (WiFi.status() != WL_CONNECTED) {
        if (now - _lastWifiCheck >= 10000UL) {
            _lastWifiCheck = now;
            WiFi.reconnect();
        }
        return;
    }

    // Accumulate reading metrics
    updateAggregates(wind, speed, rain);

    // Live update
    if (now - _lastLiveUpdate >= FIREBASE_LIVE_INTERVAL_MS) {
        _lastLiveUpdate = now;
        publishCurrent(wind, speed, rain);
    }

    // History Log entry
    if (now - _lastLogUpdate >= FIREBASE_LOG_INTERVAL_MS) {
        _lastLogUpdate = now;
        publishLog(wind, speed, rain);
    }

    // Summary Periodic Update (updates current active day and month rolling summary)
    if (now - _lastSummaryUpdate >= FIREBASE_SUMMARY_INTERVAL_MS) {
        _lastSummaryUpdate = now;
        if (isTimeSynced()) {
            _dayRainAccum = rain.todayMm;
            _monthRainAccum = rain.monthMm;

            String dateStr = getDateString();
            String monthStr = getMonthString();
            int curHour = getHour();

            publishDailySummary(dateStr);
            publishMonthlySummary(monthStr);
            publishHourlySummary(dateStr, curHour);
        }
    }
}
