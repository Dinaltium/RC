/**
 * tof.cpp — VL53L0X bring-up and polling (Pololu VL53L0X library)
 */

#include "tof.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>

struct TofSlot {
    bool        enabled;
    uint8_t     xshut;
    uint8_t     addr;
    const char* name;
    VL53L0X     dev;
    bool        ok;
    uint16_t    lastMm;
};

#ifdef FRONT_RANGE_VL53L0X
constexpr bool TOF_FRONT_ENABLED = true;
#else
constexpr bool TOF_FRONT_ENABLED = false;
#endif
#ifdef ENABLE_TOF_CLIFF
constexpr bool TOF_CLIFF_ENABLED = true;
#else
constexpr bool TOF_CLIFF_ENABLED = false;
#endif

static TofSlot _slots[TOF_COUNT] = {
    { TOF_FRONT_ENABLED, TOF_FRONT_XSHUT,       TOF_FRONT_ADDR,       "front-range", VL53L0X(), false, 0xFFFF },
    { TOF_CLIFF_ENABLED, TOF_CLIFF_FRONT_XSHUT, TOF_CLIFF_FRONT_ADDR, "cliff-front", VL53L0X(), false, 0xFFFF },
    { TOF_CLIFF_ENABLED, TOF_CLIFF_REAR_XSHUT,  TOF_CLIFF_REAR_ADDR,  "cliff-rear",  VL53L0X(), false, 0xFFFF },
};

static bool     _busStarted = false;
static uint32_t _lastPoll   = 0;

void tofInit() {
    bool any = false;
    for (auto& s : _slots) any |= s.enabled;
    if (!any) {
        Serial.println(F("[TOF] No VL53L0X sensors enabled"));
        return;
    }

    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    _busStarted = true;

    // Hold every sensor in reset first so only one answers at 0x29 at a time
    for (auto& s : _slots) {
        if (!s.enabled) continue;
        pinMode(s.xshut, OUTPUT);
        digitalWrite(s.xshut, LOW);
    }
    delay(10);

    for (auto& s : _slots) {
        if (!s.enabled) continue;
        digitalWrite(s.xshut, HIGH);
        delay(10);                       // boot time after XSHUT release
        s.dev.setTimeout(50);
        if (!s.dev.init()) {
            Serial.print(F("[TOF] "));
            Serial.print(s.name);
            Serial.println(F(": init FAILED (not connected?)"));
            s.ok = false;
            digitalWrite(s.xshut, LOW);  // keep it off the bus
            continue;
        }
        s.dev.setAddress(s.addr);
        s.dev.setMeasurementTimingBudget(33000);  // ~30 Hz
        s.dev.startContinuous(TOF_INTERVAL_MS);
        s.ok = true;
        Serial.print(F("[TOF] "));
        Serial.print(s.name);
        Serial.print(F(" ready at 0x"));
        Serial.println(s.addr, HEX);
    }
}

bool tofAvailable(TofId id) {
    return id < TOF_COUNT && _slots[id].ok;
}

uint16_t tofReadMm(TofId id) {
    return id < TOF_COUNT ? _slots[id].lastMm : 0xFFFF;
}

void tofUpdate() {
    if (!_busStarted) return;
    uint32_t now = millis();
    if (now - _lastPoll < TOF_INTERVAL_MS) return;
    _lastPoll = now;

    for (auto& s : _slots) {
        if (!s.ok) continue;
        // readRangeContinuousMillimeters() waits for data-ready; with the
        // sensor running at TOF_INTERVAL_MS and us polling at the same rate
        // the wait is ~0, and the 50 ms timeout bounds the worst case.
        uint16_t mm = s.dev.readRangeContinuousMillimeters();
        if (s.dev.timeoutOccurred() || mm >= 8190) {
            s.lastMm = 0xFFFF;
        } else {
            s.lastMm = mm;
        }
    }
}
