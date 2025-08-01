#include "button.h"
#include "pins.h"
#include "interrupts.h"
#include <Arduino.h>

// Use interrupt-driven flag instead of polling Bounce2
static bool lastState = HIGH;  // pull-up button
static bool justFlag = false;
static unsigned long pressStart = 0;
static unsigned long lastDebounce = 0;
static unsigned long debounceMsVal = 25;

void initButton(int pin, unsigned long debounceMs) {
    pinMode(pin, INPUT_PULLUP);
    lastState = digitalRead(pin);
    debounceMsVal = debounceMs;
}

void updateButton() {
    if (!buttonTriggered) return;
    buttonTriggered = false;
    bool state = digitalRead(buttonPin);
    unsigned long now = millis();
    if (state != lastState && (now - lastDebounce >= debounceMsVal)) {
        if (state == LOW) {
            pressStart = now;
            justFlag = true;
        }
        lastState = state;
        lastDebounce = now;
    }
}

bool isPressed() {
    return lastState == LOW;
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

