#include "interrupts.h"
#include <EnableInterrupt.h>
#include "pins.h"

volatile bool buttonTriggered = false;
volatile bool endstopXTriggered = false;
volatile bool endstopYTriggered = false;
volatile bool endstopZTriggered = false;

void onButtonInterrupt() { buttonTriggered = true; }
void onEndstopXInterrupt() { endstopXTriggered = true; }
void onEndstopYInterrupt() { endstopYTriggered = true; }
void onEndstopZInterrupt() { endstopZTriggered = true; }

void setupInterrupts() {
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(endstopX, INPUT_PULLUP);
    pinMode(endstopY, INPUT_PULLUP);
    pinMode(endstopZ, INPUT_PULLUP);

    enableInterrupt(buttonPin, onButtonInterrupt, CHANGE);
    enableInterrupt(endstopX, onEndstopXInterrupt, CHANGE);
    enableInterrupt(endstopY, onEndstopYInterrupt, CHANGE);
    enableInterrupt(endstopZ, onEndstopZInterrupt, CHANGE);
}
