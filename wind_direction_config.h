#ifndef CONFIG_H
#define CONFIG_H

// =====================================================================
// Hardware pins — ESP-12E on a NodeMCU-style board, wired as:
//   AS5600 VCC -> 3V   AS5600 GND -> G
//   AS5600 SCL -> D1   AS5600 SDA -> D2
// =====================================================================
// D1/D2 are normally provided as macros by the ESP8266 core once you
// pick "NodeMCU 1.0 (ESP-12E Module)" under Tools > Board in the Arduino
// IDE (D1 = GPIO5, D2 = GPIO4). The #ifndef guards below fill them in
// with the same values if you ever compile against a board variant
// (e.g. "Generic ESP8266 Module") that doesn't define them, so this
// keeps working either way. GPIO4/5 are also boot-safe pins, unlike
// GPIO0/2/15 which set the boot mode and must be left alone.
#ifndef D1
#define D1 5
#endif
#ifndef D2
#define D2 4
#endif

#define I2C_SCL_PIN D1 // GPIO5
#define I2C_SDA_PIN D2 // GPIO4

// =====================================================================
// Serial
// =====================================================================
#define SERIAL_BAUD 115200

// =====================================================================
// Wind vane calibration - how to make North read as 0 degrees
// =====================================================================
// 0) First make sure the magnet itself is healthy: power up, open the
//    Serial monitor, and check the "[wind] magnet diagnostics" block
//    printed at boot. It must say "magnet detected (MD): yes" with no
//    ML/MH warnings - readings are meaningless until that's true. If
//    it's not, see the troubleshooting notes printed alongside it
//    (magnet type/centering/air gap) before doing anything below.
//
// 1) With WIND_DIR_OFFSET_DEG still at 0.0 below, upload and watch the
//    Serial monitor. Physically point the vane at true north and let
//    the "deg=" value settle.
// 2) Set WIND_DIR_OFFSET_DEG to the NEGATIVE of whatever it printed.
//    e.g. if it read "deg=63.4" while pointing north, set this to -63.4.
// 3) Re-upload, point the vane north again, and confirm it now reads
//    ~0.0 deg / "N".
// 4) If N reads correctly but rotating clockwise makes the reading go
//    the wrong way (e.g. N -> NW -> W instead of N -> NE -> E), set
//    WIND_DIR_INVERT to true below and repeat steps 1-3.
#define WIND_DIR_OFFSET_DEG -333.1f

// Some mounts turn the magnet the "wrong" way relative to compass
// rotation (e.g. gear/belt coupling). See step 4 above.
#define WIND_DIR_INVERT false

// When true, setup() re-zeroes the offset to whatever direction the vane
// HAPPENS to be pointing at every power-on/reset - not necessarily true
// north. That's fragile for a device left running outdoors (a random
// reset while the vane is mid-swing would silently redefine "north").
// Leave this false and use the fixed WIND_DIR_OFFSET_DEG calibration
// above instead; only set it true if you specifically want "wherever
// it's pointing when it boots" to always mean 0 deg.
#define CALIBRATE_ON_BOOT false

// =====================================================================
// Sampling
// =====================================================================
#define SAMPLE_INTERVAL_MS 1000

#endif // CONFIG_H