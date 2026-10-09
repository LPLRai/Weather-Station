// firebase_config.h
//
// Configuration for ESP8266 Wi-Fi, NTP time sync, and Firebase Realtime Database.
// Edit your Wi-Fi credentials and Firebase database URL below.

#ifndef FIREBASE_CONFIG_H
#define FIREBASE_CONFIG_H

#include <Arduino.h>

// =====================================================================
// 1. Wi-Fi Configuration
// =====================================================================
#define WIFI_SSID "SunwayForAI"
#define WIFI_PASSWORD "Sunway@123"

// Enable or disable Wi-Fi / Firebase entirely (set false for offline/serial only)
#define FIREBASE_ENABLED true

// =====================================================================
// 2. Firebase Realtime Database Configuration
// =====================================================================
// Your Firebase Realtime Database URL (Asia-Southeast1 / Singapore Region)
// Firebase Project: weatherproject-864e1
#define FIREBASE_HOST "https://weatherproject-864e1-default-rtdb.asia-southeast1.firebasedatabase.app"

// Firebase Database Secret or Auth Token.
// If your Firebase Realtime Database rules allow read/write without auth (e.g. test mode),
// you can leave this empty: ""
#define FIREBASE_AUTH ""

// Base path in the database for weather data (no leading or trailing slashes)
#define FIREBASE_BASE_PATH "weather"

// =====================================================================
// 3. Update & Logging Intervals (Milliseconds)
// =====================================================================
// How often to update the live current readings in Firebase (default: every 3 seconds)
#define FIREBASE_LIVE_INTERVAL_MS 3000UL

// How often to write a time-series history log snapshot (default: every 60 seconds)
#define FIREBASE_LOG_INTERVAL_MS 60000UL

// How often to sync/update the hourly & daily aggregate summaries (default: every 30 seconds)
#define FIREBASE_SUMMARY_INTERVAL_MS 30000UL

// =====================================================================
// 4. NTP Time & Timezone Configuration
// =====================================================================
// Time servers
#define NTP_SERVER_PRIMARY "pool.ntp.org"
#define NTP_SERVER_SECONDARY "time.nist.gov"

// Timezone offset in seconds (e.g. UTC+0 = 0, UTC+5:45 (Nepal) = 20700, UTC+5:30 (India) = 19800, UTC-5 (EST) = -18000, UTC+1 (CET) = 3600)
#define TIMEZONE_OFFSET_SEC 20700

// Daylight Savings Time offset in seconds (0 if not used)
#define DST_OFFSET_SEC 0

#endif // FIREBASE_CONFIG_H
