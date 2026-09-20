// weather_station.ino
//
// Main sketch for an ESP-12E based modular weather station.
// Includes:
// 1) Wind Direction: AS5600 12-bit magnetic angle sensor (I2C: D1/SCL, D2/SDA)
// 2) Rain Gauge: Hall Effect Tipping Bucket (Pin: D3, 3V3, GND)
//    - Real-time rain metrics: Total, Last Hour, Today (24h), Week (7d), Month (30d), Rate
//    - LittleFS flash storage so rain records persist across reboots
// 3) Web Dashboard & REST API: Live card interface matching reference design
//

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "wind_direction_config.h"
#include "wind_direction.h"

#include "rain_gauge_config.h"
#include "rain_gauge.h"

#if ENABLE_WEB_DASHBOARD
#include "weather_web_dashboard.h"
ESP8266WebServer server(80);
#endif

WindDirection windSensor;

void setupWiFi()
{
#if ENABLE_WEB_DASHBOARD
  Serial.println(F("[wifi] Initializing Wi-Fi connection..."));
  WiFi.mode(WIFI_AP_STA);

  if (String(WIFI_SSID) != "YOUR_WIFI_SSID")
  {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
      delay(500);
      Serial.print(F("."));
      attempts++;
    }
    Serial.println();
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print(F("[wifi] Connected! Dashboard available at: http://"));
    Serial.println(WiFi.localIP());
  }
  else
  {
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.println(F("[wifi] Running in Access Point mode."));
    Serial.print(F("[wifi] Connect to WiFi: "));
    Serial.print(AP_SSID);
    Serial.print(F(" -> Open: http://"));
    Serial.println(WiFi.softAPIP());
  }

  setupWebServer();
#endif
}

void setup()
{
  Serial.begin(SERIAL_BAUD);
  delay(200); // let the USB-serial bridge settle
  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F("    ESP-12E WEATHER STATION BOOTING     "));
  Serial.println(F("========================================"));

  // 1. Initialize Wind Direction Sensor
  if (!windSensor.begin(I2C_SDA_PIN, I2C_SCL_PIN))
  {
    Serial.println(F("[wind] ERROR: AS5600 did not ACK on I2C (0x36). Check wiring."));
  }
  else
  {
    Serial.println(F("[wind] AS5600 found on I2C bus."));
  }

  MagnetStatus mstat = windSensor.getMagnetStatus();
  Serial.println(F("[wind] --- Magnet Diagnostics ---"));
  Serial.print(F("[wind] Magnet detected: "));
  Serial.println(mstat.detected ? F("YES") : F("NO"));
  Serial.print(F("[wind] Signal AGC: "));
  Serial.println(mstat.agc);
  Serial.print(F("[wind] Magnitude: "));
  Serial.println(mstat.magnitude);

#if CALIBRATE_ON_BOOT
  float startRaw = windSensor.recordStartupDirection();
  if (startRaw >= 0)
  {
    Serial.print(F("[wind] Startup baseline calibrated: "));
    Serial.println(startRaw, 1);
  }
#endif

  // 2. Initialize Rain Gauge Sensor & Storage
  Serial.println(F("[rain] Initializing Rain Gauge (Hall Sensor)..."));
  if (rainSensor.begin(RAIN_SENSOR_PIN, RAIN_MM_PER_TIP))
  {
    Serial.print(F("[rain] Sensor attached on GPIO/Pin D3. Tip Resolution: "));
    Serial.print(RAIN_MM_PER_TIP, 4);
    Serial.println(F(" mm/tip"));
  }

  // 3. Setup WiFi & Dashboard
  setupWiFi();

  Serial.println(F("========================================"));
  Serial.println(F("     SETUP COMPLETE - LOGGING DATA      "));
  Serial.println(F("========================================"));
}

void loop()
{
  // Housekeeping for rain ticks & persistence
  rainSensor.update();

#if ENABLE_WEB_DASHBOARD
  server.handleClient();
#endif

  static unsigned long lastSample = 0;
  unsigned long now = millis();

  if (now - lastSample >= SAMPLE_INTERVAL_MS)
  {
    lastSample = now;

    // Read Sensors
    WindReading w = windSensor.read();
    RainReading r = rainSensor.getReading();

    // Output to Serial Monitor
    Serial.print(F("[WIND] Deg: "));
    Serial.print(w.degrees, 1);
    Serial.print(F("° ("));
    Serial.print(w.compass);
    Serial.print(F(") | [RAIN] Total: "));
    Serial.print(r.totalMm, 2);
    Serial.print(F(" mm | 1h: "));
    Serial.print(r.lastHourMm, 2);
    Serial.print(F(" mm | Today: "));
    Serial.print(r.todayMm, 2);
    Serial.print(F(" mm | Week: "));
    Serial.print(r.weekMm, 2);
    Serial.print(F(" mm | Month: "));
    Serial.print(r.monthMm, 2);
    Serial.print(F(" mm | Rate: "));
    Serial.print(r.rainRateMmPerHour, 2);
    Serial.print(F(" mm/h"));
    
    if (r.isRaining)
    {
      Serial.print(F(" [RAINING]"));
    }
    Serial.println();
  }
}
