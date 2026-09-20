// weather_web_dashboard.h
//
// Lightweight, responsive HTML/JS web dashboard embedded directly in flash.
// Mimics modern weather station cards and displays live English metrics:
// Total Rain, Last Hour, Today (24h), Week (7d), Month (30d), and Rain Rate.
//

#ifndef WEATHER_WEB_DASHBOARD_H
#define WEATHER_WEB_DASHBOARD_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "rain_gauge.h"
#include "wind_direction.h"

extern ESP8266WebServer server;
extern WindDirection windSensor;

inline void setupWebServer()
{
  // Endpoint: JSON metrics API
  server.on("/api/data", HTTP_GET, []() {
    RainReading r = rainSensor.getReading();
    WindReading w = windSensor.read();

    String json = "{";
    json += "\"rain\":" + rainSensor.getHistoryJson() + ",";
    json += "\"wind\":{";
    json += "\"deg\":" + String(w.degrees, 1) + ",";
    json += "\"compass\":\"" + String(w.compass) + "\",";
    json += "\"valid\":" + String(w.valid ? "true" : "false");
    json += "}}";

    server.send(200, "application/json", json);
  });

  // Endpoint: Reset History
  server.on("/api/reset_rain", HTTP_POST, []() {
    rainSensor.clearHistory();
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Rain history cleared\"}");
  });

  // Endpoint: Main Web Dashboard UI
  server.on("/", HTTP_GET, []() {
    String html = F(R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP12 Weather Station</title>
  <style>
    :root {
      --bg: #f4f6f9;
      --card-bg: #ffffff;
      --text: #2c3e50;
      --subtext: #7f8c8d;
      --primary: #2980b9;
      --accent: #3498db;
      --border: #e2e8f0;
      --water: #38bdf8;
    }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background-color: var(--bg);
      color: var(--text);
      margin: 0;
      padding: 16px;
    }
    .container {
      max-width: 540px;
      margin: 0 auto;
    }
    .header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 16px;
    }
    .header h1 {
      font-size: 1.25rem;
      font-weight: 600;
      margin: 0;
      color: var(--text);
    }
    .badge {
      display: inline-block;
      padding: 4px 8px;
      border-radius: 12px;
      font-size: 0.75rem;
      font-weight: 600;
      background: #e0f2fe;
      color: #0369a1;
    }
    .main-card {
      background: var(--card-bg);
      border-radius: 20px;
      padding: 24px;
      box-shadow: 0 4px 15px rgba(0,0,0,0.05);
      margin-bottom: 16px;
      border: 1px solid var(--border);
      position: relative;
      overflow: hidden;
    }
    .main-card::after {
      content: "";
      position: absolute;
      right: -20px;
      top: -20px;
      width: 100px;
      height: 100px;
      background: radial-gradient(circle, rgba(56,189,248,0.2) 0%, rgba(255,255,255,0) 70%);
      border-radius: 50%;
    }
    .card-label {
      font-size: 1rem;
      font-weight: 500;
      color: var(--subtext);
      display: flex;
      align-items: center;
      gap: 6px;
    }
    .big-metric {
      font-size: 3.5rem;
      font-weight: 700;
      line-height: 1.1;
      margin: 12px 0 6px 0;
      color: #1e293b;
    }
    .unit {
      font-size: 1.5rem;
      font-weight: 400;
      color: var(--subtext);
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 12px;
      margin-bottom: 16px;
    }
    .sub-card {
      background: var(--card-bg);
      border-radius: 16px;
      padding: 16px 12px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.04);
      border: 1px solid var(--border);
      text-align: left;
    }
    .sub-card .label {
      font-size: 0.82rem;
      font-weight: 600;
      color: var(--subtext);
      margin-bottom: 8px;
    }
    .sub-card .value {
      font-size: 1.35rem;
      font-weight: 700;
      color: #1e293b;
    }
    .sub-card .unit-small {
      font-size: 0.8rem;
      font-weight: 400;
      color: var(--subtext);
    }
    .month-card {
      background: var(--card-bg);
      border-radius: 16px;
      padding: 18px 20px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.04);
      border: 1px solid var(--border);
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 16px;
    }
    .wind-card {
      background: var(--card-bg);
      border-radius: 16px;
      padding: 16px 20px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.04);
      border: 1px solid var(--border);
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 16px;
    }
    .btn-reset {
      background: #f1f5f9;
      border: 1px solid #cbd5e1;
      color: #475569;
      padding: 8px 14px;
      border-radius: 8px;
      font-size: 0.8rem;
      cursor: pointer;
      font-weight: 600;
    }
    .btn-reset:hover {
      background: #e2e8f0;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>Weather Station</h1>
      <span class="badge" id="statusBadge">Live</span>
    </div>

    <!-- Total / Event Rain Card -->
    <div class="main-card">
      <div class="card-label">
        <span>Rain (Total)</span>
        <svg width="18" height="18" viewBox="0 0 24 24" fill="#38bdf8"><path d="M12 2.69l5.66 5.66a8 8 0 1 1-11.31 0z"/></svg>
      </div>
      <div class="big-metric">
        <span id="totalRain">0.00</span> <span class="unit">mm</span>
      </div>
      <div style="font-size: 0.85rem; color: var(--subtext);">
        Rate: <b id="rainRate">0.00</b> mm/h &bull; Tips: <span id="totalTips">0</span>
      </div>
    </div>

    <!-- 3-Column Cards: Last Hour, Today, This Week -->
    <div class="grid">
      <div class="sub-card">
        <div class="label">Last Hour</div>
        <div class="value"><span id="lastHour">0.00</span> <span class="unit-small">mm</span></div>
      </div>
      <div class="sub-card">
        <div class="label">Today (24h)</div>
        <div class="value"><span id="todayRain">0.00</span> <span class="unit-small">mm</span></div>
      </div>
      <div class="sub-card">
        <div class="label">This Week</div>
        <div class="value"><span id="weekRain">0.00</span> <span class="unit-small">mm</span></div>
      </div>
    </div>

    <!-- This Month Card -->
    <div class="month-card">
      <div>
        <div class="label" style="font-size:0.85rem; color:var(--subtext); font-weight:600;">This Month (30 Days)</div>
        <div style="font-size:1.6rem; font-weight:700; color:#1e293b; margin-top:4px;">
          <span id="monthRain">0.00</span> <span class="unit-small">mm</span>
        </div>
      </div>
      <svg width="28" height="28" viewBox="0 0 24 24" fill="#38bdf8"><path d="M12 2.69l5.66 5.66a8 8 0 1 1-11.31 0z"/></svg>
    </div>

    <!-- Wind Direction Card -->
    <div class="wind-card">
      <div>
        <div style="font-size:0.85rem; color:var(--subtext); font-weight:600;">Wind Direction</div>
        <div style="font-size:1.3rem; font-weight:700; color:#1e293b; margin-top:4px;">
          <span id="windCompass">--</span> (<span id="windDeg">--</span>°)
        </div>
      </div>
      <span style="font-size: 1.5rem;">🧭</span>
    </div>

    <div style="text-align: right; margin-top: 10px;">
      <button class="btn-reset" onclick="resetRainHistory()">Clear Rain History</button>
    </div>
  </div>

  <script>
    async function fetchData() {
      try {
        const res = await fetch('/api/data');
        const d = await res.json();
        
        document.getElementById('totalRain').innerText = d.rain.totalMm.toFixed(2);
        document.getElementById('rainRate').innerText = d.rain.rainRateMmPerHour.toFixed(2);
        document.getElementById('totalTips').innerText = d.rain.totalTips;
        document.getElementById('lastHour').innerText = d.rain.lastHourMm.toFixed(2);
        document.getElementById('todayRain').innerText = d.rain.todayMm.toFixed(2);
        document.getElementById('weekRain').innerText = d.rain.weekMm.toFixed(2);
        document.getElementById('monthRain').innerText = d.rain.monthMm.toFixed(2);

        if (d.rain.isRaining) {
          document.getElementById('statusBadge').innerText = 'Raining Now';
          document.getElementById('statusBadge').style.background = '#dbeafe';
          document.getElementById('statusBadge').style.color = '#1d4ed8';
        } else {
          document.getElementById('statusBadge').innerText = 'Online';
          document.getElementById('statusBadge').style.background = '#e0f2fe';
          document.getElementById('statusBadge').style.color = '#0369a1';
        }

        if (d.wind) {
          document.getElementById('windCompass').innerText = d.wind.compass || '--';
          document.getElementById('windDeg').innerText = (d.wind.deg !== undefined) ? d.wind.deg.toFixed(1) : '--';
        }
      } catch (err) {
        console.error("Fetch error:", err);
      }
    }

    async function resetRainHistory() {
      if (confirm("Are you sure you want to clear saved rain records?")) {
        await fetch('/api/reset_rain', { method: 'POST' });
        fetchData();
      }
    }

    setInterval(fetchData, 2000);
    fetchData();
  </script>
</body>
</html>
)rawliteral");

    server.send(200, "text/html", html);
  });

  server.begin();
}

#endif // WEATHER_WEB_DASHBOARD_H
