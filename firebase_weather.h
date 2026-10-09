// firebase_weather.h
//
// Firebase Realtime Database and NTP client for ESP8266 Weather Station.
// Periodically publishes live sensor readings and maintains structured
// Day, Week, and Month history buckets.

#ifndef FIREBASE_WEATHER_H
#define FIREBASE_WEATHER_H

#include <Arduino.h>
#include "firebase_config.h"
#include "wind_direction.h"
#include "wind_speed.h"
#include "rain_gauge.h"

class FirebaseWeather {
public:
    FirebaseWeather();

    // Call once in setup() to start Wi-Fi and NTP sync
    void begin();

    // Call regularly in loop() with latest readings
    void update(const WindReading& wind, const WindSpeedReading& speed, const RainReading& rain);

    // Status helpers
    bool isWifiConnected() const;
    bool isTimeSynced() const;
    int getWifiRssi() const;
    String getIsoTimestamp();
    String getDateString();
    String getMonthString();
    int getHour();
    unsigned long getEpochMs();

private:
    unsigned long _lastLiveUpdate;
    unsigned long _lastLogUpdate;
    unsigned long _lastSummaryUpdate;
    unsigned long _lastWifiCheck;

    // Hourly aggregation tracking
    int _currentHour;
    int _currentDay;
    int _currentMonth;
    int _currentYear;

    float _hourRainAccum;
    float _hourWindSum;
    float _hourWindMax;
    unsigned long _hourSamples;
    float _hourWindDirSum;

    // Day aggregation tracking
    float _dayRainAccum;
    float _dayWindMax;
    float _dayWindSum;
    unsigned long _daySamples;

    // Month aggregation tracking
    float _monthRainAccum;
    float _monthWindMax;

    void connectWifi();
    void syncTime();
    bool sendFirebaseRequest(const String& method, const String& path, const String& jsonPayload);
    void publishCurrent(const WindReading& wind, const WindSpeedReading& speed, const RainReading& rain);
    void publishLog(const WindReading& wind, const WindSpeedReading& speed, const RainReading& rain);
    void updateAggregates(const WindReading& wind, const WindSpeedReading& speed, const RainReading& rain);
    void publishHourlySummary(const String& dateStr, int hour);
    void publishDailySummary(const String& dateStr);
    void publishMonthlySummary(const String& monthStr);

    String cleanHost(const String& rawHost);
};

extern FirebaseWeather firebaseWeather;

#endif // FIREBASE_WEATHER_H
