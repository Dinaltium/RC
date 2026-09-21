/**
 * oled.cpp — SSD1306 status page
 *
 * Layout (128x64, 6x8 font):
 *   row 0  FORWARD 200        <- current command + speed
 *   row 1  src PHONE_WIFI     <- who is driving
 *   row 2  range  42 cm  OBS  <- ultrasonic + obstacle flag
 *   row 3  bump F- R-  cliff  <- sensor flags
 *   row 4  192.168.4.1        <- AP address
 *   row 5  E-STOP / up 123s
 */
#include "oled.h"
#include "config.h"
#include "robot_state.h"
#include <Arduino.h>

#ifdef ENABLE_OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>

static Adafruit_SSD1306 _display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool     _present = false;
static uint32_t _lastDraw = 0;

static bool probe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

void oledInit() {
    Wire.begin(I2C_SDA, I2C_SCL);   // no-op if tof.cpp already started the bus
    if (!probe(OLED_ADDR)) {
        Serial.print(F("[OLED] Nothing at 0x"));
        Serial.print(OLED_ADDR, HEX);
        Serial.println(F(" — display disabled"));
        return;
    }
    if (!_display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println(F("[OLED] begin() failed — display disabled"));
        return;
    }
    _present = true;
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.println(F("RobotPrototype"));
    _display.print(F("fw ")); _display.println(FIRMWARE_VERSION);
    _display.println(F("booting..."));
    _display.display();
    Serial.println(F("[OLED] SSD1306 ready"));
}

void oledUpdate() {
    if (!_present) return;
    uint32_t now = millis();
    if (now - _lastDraw < OLED_INTERVAL_MS) return;
    _lastDraw = now;

    const RobotState& st = stateGet();
    _display.clearDisplay();
    _display.setCursor(0, 0);

    // Row 0: command, big
    _display.setTextSize(2);
    if (st.emergencyStop) {
        _display.println(F("E-STOP"));
    } else {
        const char* n = actionName(st.currentCommand);
        // fit 10 chars at size 2
        char buf[11]; strncpy(buf, n, 10); buf[10] = 0;
        _display.println(buf);
    }
    _display.setTextSize(1);

    // Row 2: speed + source
    _display.print(F("spd ")); _display.print(st.speed);
    _display.print(F("  ")); _display.println(sourceName(st.activeSource));

    // Row 3: range
    _display.print(F("range "));
    if (st.distanceCm >= 900) _display.print(F("---"));
    else _display.print((int)st.distanceCm);
    _display.print(F(" cm"));
    if (st.obstacleDetected) _display.print(F("  OBST"));
    _display.println();

    // Row 4: bumpers / cliffs
    _display.print(F("bump "));
    _display.print(st.bumpFront ? 'F' : '-'); _display.print(st.bumpRear ? 'R' : '-');
    _display.print(F("  cliff "));
    _display.print(st.cliffFront ? 'F' : '-'); _display.println(st.cliffRear ? 'R' : '-');

    // Row 5: network
    _display.print(WiFi.softAPIP());
    _display.print(F("  "));
    _display.print(WiFi.softAPgetStationNum());
    _display.println(F(" cli"));

    // Row 6: uptime
    _display.print(F("up ")); _display.print(now / 1000); _display.print(F("s"));
    _display.display();
}

bool oledPresent() { return _present; }

#else
void oledInit()   { Serial.println(F("[OLED] Disabled (ENABLE_OLED not defined)")); }
void oledUpdate() {}
bool oledPresent() { return false; }
#endif
