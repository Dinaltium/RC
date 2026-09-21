/**
 * cliff.cpp — Latched cliff detection from the two downward ToF sensors
 */

#include "cliff.h"
#include "config.h"
#include "tof.h"
#include <Arduino.h>

struct CliffSide {
    TofId       id;
    const char* name;
    bool        latched;
    uint8_t     consecutive;
    bool        unavailable;
};

static CliffSide _front = { TOF_CLIFF_FRONT, "front", false, 0, true };
static CliffSide _rear  = { TOF_CLIFF_REAR,  "rear",  false, 0, true };

static bool isDrop(uint16_t mm) {
    // No return / out of range = nothing under the sensor = drop.
    if (mm == 0xFFFF || mm > CLIFF_MAX_VALID_MM) return true;
    return mm > (uint16_t)(CLIFF_FLOOR_MM + CLIFF_MARGIN_MM);
}

static void evaluate(CliffSide& c) {
    if (c.unavailable) return;
    uint16_t mm = tofReadMm(c.id);
    if (isDrop(mm)) {
        if (c.consecutive < 255) c.consecutive++;
        if (!c.latched && c.consecutive >= CLIFF_CONFIRM_READS) {
            c.latched = true;
            Serial.print(F("[CLIFF] *** "));
            Serial.print(c.name);
            Serial.print(F(" drop detected ("));
            if (mm == 0xFFFF) Serial.print(F("no return")); else Serial.print(mm);
            Serial.println(F(" mm), latched ***"));
        }
    } else {
        c.consecutive = 0;
    }
}

static void reportMissing(CliffSide& c) {
    if (!c.unavailable) return;
    Serial.print(F("[CLIFF] WARNING: "));
    Serial.print(c.name);
    if (CLIFF_FAIL_SAFE) {
        c.latched = true;
        Serial.println(F(" sensor missing; movement that way is BLOCKED (CLIFF_FAIL_SAFE)"));
    } else {
        Serial.println(F(" sensor missing; running WITHOUT cliff protection on that side"));
    }
}

void cliffInit() {
#ifdef ENABLE_TOF_CLIFF
    _front.unavailable = !tofAvailable(TOF_CLIFF_FRONT);
    _rear.unavailable  = !tofAvailable(TOF_CLIFF_REAR);
    reportMissing(_front);
    reportMissing(_rear);
    Serial.print(F("[CLIFF] Initialized: floor "));
    Serial.print(CLIFF_FLOOR_MM);
    Serial.print(F(" mm, trip above "));
    Serial.print(CLIFF_FLOOR_MM + CLIFF_MARGIN_MM);
    Serial.println(F(" mm"));
#else
    Serial.println(F("[CLIFF] Disabled (ENABLE_TOF_CLIFF not defined)"));
#endif
}

void cliffUpdate() {
#ifdef ENABLE_TOF_CLIFF
    evaluate(_front);
    evaluate(_rear);
#endif
}

bool cliffFront()      { return _front.latched; }
bool cliffRear()       { return _rear.latched; }
bool cliffSensorsOk()  { return !_front.unavailable && !_rear.unavailable; }

void cliffClear() {
    if (_front.latched || _rear.latched) Serial.println(F("[CLIFF] Latches cleared by user"));
    // A missing sensor under CLIFF_FAIL_SAFE stays latched.
    _front.latched = _front.unavailable && CLIFF_FAIL_SAFE;
    _rear.latched  = _rear.unavailable  && CLIFF_FAIL_SAFE;
    _front.consecutive = _rear.consecutive = 0;
}
