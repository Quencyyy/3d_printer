#include "interrupts.h"
#include <EnableInterrupt.h>
#include "pins.h"

volatile bool buttonTriggered = false;

void onButtonInterrupt() { buttonTriggered = true; }

void setupInterrupts() {
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(endstopX, INPUT_PULLUP);
    pinMode(endstopY, INPUT_PULLUP);
    pinMode(endstopZ, INPUT_PULLUP);

    enableInterrupt(buttonPin, onButtonInterrupt, CHANGE);
}
