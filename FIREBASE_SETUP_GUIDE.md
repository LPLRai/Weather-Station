# 🔥 Firebase Weather Station Setup & History Guide

This guide walks you through setting up **Firebase Realtime Database** for your ESP8266 Weather Station to log, store, and visualize historical weather data across **Days**, **Weeks**, and **Months**.

---

## 🗂️ 1. Firebase Realtime Database Architecture

Your data is organized in an orderly, hierarchical structure:

```text
weather/
├── current/                              <-- Live telemetry (synced every 3-5s)
│   ├── wind/
│   │   ├── speed_kmh: 12.5
│   │   ├── rpm: 45.2
│   │   ├── degrees: 135.0
│   │   ├── compass: "SE"
│   │   ├── raw_angle: 1536
│   │   ├── total_spins: 1240
│   │   └── magnet_valid: true
│   ├── rain/
│   │   ├── rate_mm_h: 2.4
│   │   ├── today_mm: 5.6
│   │   ├── week_mm: 18.2
│   │   ├── month_mm: 42.8
│   │   ├── total_mm: 112.5
│   │   ├── total_tips: 402
│   │   └── is_raining: true
│   └── system/
│       ├── wifi_rssi: -62
│       ├── uptime_sec: 3600
│       ├── free_heap: 28400
│       ├── timestamp: 1728389900000
│       └── timestamp_iso: "2026-10-08T12:45:00Z"
│
└── history/                              <-- History System
    ├── hourly/                           <-- 24-Hour Day History
    │   └── YYYY-MM-DD/                   (e.g., 2026-10-08)
    │       ├── 00/ { rain_mm, wind_avg_kmh, wind_max_kmh, wind_dir_compass, samples }
    │       ├── 01/ { ... }
    │       └── 23/ { ... }
    │
    ├── daily/                            <-- 7-Day Week & Month Day History
    │   └── YYYY-MM-DD/                   (e.g., 2026-10-08)
    │       ├── rain_mm: 5.6
    │       ├── wind_avg_kmh: 7.2
    │       ├── wind_max_kmh: 19.5
    │       ├── samples: 1440
    │       └── last_updated: 1728389900000
    │
    ├── monthly/                          <-- 12-Month Annual History
    │   └── YYYY-MM/                      (e.g., 2026-10)
    │       ├── rain_mm: 42.8
    │       ├── wind_max_kmh: 28.4
    │       └── last_updated: 1728389900000
    │
    └── logs/                             <-- Fine-grained periodic log entries
        └── {timestamp_ms}/
            ├── speed, dir, cardinal, rain_rate, rain_today, rain_total, iso
```

---

## 🚀 2. Setting Up Firebase for Project `weatherproject-864e1`

1. Open your project console: [https://console.firebase.google.com/u/3/project/weatherproject-864e1/database/weatherproject-864e1-default-rtdb/data](https://console.firebase.google.com/u/3/project/weatherproject-864e1/database/weatherproject-864e1-default-rtdb/data)
2. In the **Rules** tab, make sure the rules allow read/write:

```json
{
  "rules": {
    ".read": true,
    ".write": true
  }
}
```
3. Database URL configured:
   `https://weatherproject-864e1-default-rtdb.asia-southeast1.firebasedatabase.app`

---

## ⚙️ 3. Configuring the ESP8266 Code

Open [`firebase_config.h`](file:///C:/Users/verse/Desktop/weather/Weather-Station/firebase_config.h) and fill in your network and Firebase details:

```cpp
// 1. Wi-Fi
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// 2. Firebase Database URL
#define FIREBASE_HOST "https://your-project-id-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH ""  // Leave empty if rules are open or put your Database Secret

// 3. Timezone (seconds from UTC)
// e.g. Nepal UTC+5:45 = 20700 | India UTC+5:30 = 19800 | UTC-5 (EST) = -18000 | UTC+0 = 0
#define TIMEZONE_OFFSET_SEC 20700
```

---

## 📊 4. Viewing the Dashboard

1. Open [`weather-dashboard.html`](file:///C:/Users/verse/Desktop/weather/Weather-Station/weather-dashboard.html) in your browser (Google Chrome, Edge, Safari, Firefox).
2. Click **Config** in the top right and enter your Firebase Database URL.
3. You will have full access to:
   - **Live Telemetry**: Animated compass rose, wind speedometer, rain rate, today/week/month rain totals.
   - **Day History**: 24-hour hourly trend chart, max gusts, prevailing wind direction, hourly rainfall breakdown table.
   - **Week History**: 7-day rainfall bar charts, wettest day stats, weekly wind velocity comparisons.
   - **Month History**: 30-day cumulative rain area chart, daily rainfall bars, number of rainy days.
   - **Export**: One-click export to CSV for Day, Week, or Month records.
   - **Demo Mode**: Click **Demo Data** to preview all charts and animations instantly with simulated weather patterns.
