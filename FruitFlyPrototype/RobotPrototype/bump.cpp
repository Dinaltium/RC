/**
 * bump.cpp — Debounced endstop reading
 *
 * Switches are wired normally-closed to GND with an external 10 kΩ pull-up,
 * so the resting level is LOW and a press (or a cut wire) reads HIGH.
 */

#include "bump.h"
#include "config.h"
#include <Arduino.h>

struct BumpInput {
    uint8_t  pin;
    bool     pressed;
    bool     rawLast;
    uint32_t changedAt;
};

#ifdef ENABLE_BUMP
static BumpInput _front = { BUMP_FRONT_PIN, false, false, 0 };
static BumpInput _rear  = { BUMP_REAR_PIN,  false, false, 0 };

static void readDebounced(BumpInput& b, const char* name) {
    bool raw = digitalRead(b.pin) == HIGH;
    uint32_t now = millis();
    if (raw != b.rawLast) {
        b.rawLast = raw;
        b.changedAt = now;
    }
    if (raw != b.pressed && now - b.changedAt >= BUMP_DEBOUNCE_MS) {
        b.pressed = raw;
        Serial.print(F("[BUMP] "));
        Serial.print(name);
        Serial.println(b.pressed ? F(" pressed") : F(" released"));
    }
}
#endif

void bumpInit() {
#ifdef ENABLE_BUMP
    pinMode(BUMP_FRONT_PIN, INPUT);   // GPIO 34/35: input only, external pull-up required
    pinMode(BUMP_REAR_PIN,  INPUT);
    _front.rawLast = _front.pressed = digitalRead(BUMP_FRONT_PIN) == HIGH;
    _rear.rawLast  = _rear.pressed  = digitalRead(BUMP_REAR_PIN)  == HIGH;
    Serial.print(F("[BUMP] Initialized (NC endstops) front="));
    Serial.print(_front.pressed ? "HIT" : "clear");
    Serial.print(F(" rear="));
    Serial.println(_rear.pressed ? "HIT" : "clear");
    if (_front.pressed || _rear.pressed) {
        Serial.println(F("[BUMP] WARNING: a bumper reads pressed at boot; check wiring and pull-ups"));
    }
#else
    Serial.println(F("[BUMP] Disabled (ENABLE_BUMP not defined)"));
#endif
}

void bumpUpdate() {
#ifdef ENABLE_BUMP
    readDebounced(_front, "front");
    readDebounced(_rear,  "rear");
#endif
}

bool bumpFront() {
#ifdef ENABLE_BUMP
    return _front.pressed;
#else
    return false;
#endif
}

bool bumpRear() {
#ifdef ENABLE_BUMP
    return _rear.pressed;
#else
    return false;
#endif
}
