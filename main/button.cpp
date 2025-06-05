#include "button.h"
#include <Bounce2.h>
#include <Arduino.h>

static Bounce debouncer = Bounce();
static bool lastState = HIGH;  // pull-up button
static bool justFlag = false;
static unsigned long pressStart = 0;

void initButton(int pin, unsigned long debounceMs) {
    pinMode(pin, INPUT_PULLUP);
    debouncer.attach(pin);
    debouncer.interval(debounceMs);
    lastState = HIGH;
}

void updateButton() {
    debouncer.update();
    bool state = debouncer.read();
    if (state != lastState) {
        if (state == LOW) {
            pressStart = millis();
            justFlag = true;
        }
        lastState = state;
    }
}

bool isPressed() {
    return debouncer.read() == LOW;
}

static bool consumeJust() {
    if (justFlag) {
        justFlag = false;
        return true;
    }
    return false;
}

bool justPressed() {
    return consumeJust();
}

bool longPressed(unsigned long ms) {
    return isPressed() && (millis() - pressStart >= ms);
}

bool doublePressed(unsigned long ms) {
    static unsigned long lastSingle = 0;
    if (consumeJust()) {
        unsigned long now = millis();
        if (now - lastSingle <= ms) {
            lastSingle = 0;
            return true;
        }
        lastSingle = now;
    }
    return false;
}
