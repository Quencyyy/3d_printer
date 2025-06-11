#include "test_modes.h"
#include "pins.h"
#include "gcode.h"
#include "button.h"
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "state.h"
#include "motion.h"

// References to globals from main program
extern LiquidCrystal_I2C lcd;
extern int displayMode;
extern void showMessage(const char*, const char*);
extern void checkButton();
extern void updateLCD();

#ifdef ENABLE_BUTTON_MENU_TEST
// Simple values used to mimic real printing status
static int dummyProgress = 0;
static float dummyTemp = 25.0;

void testMenuSetup() {
    showMessage("Menu Test", "Press Button");
}

void testMenuLoop() {
    // simulate printing data
    dummyProgress = (dummyProgress + 1) % 101;
    dummyTemp += 0.1;
    if (dummyTemp > 60) dummyTemp = 25.0;

    // override globals used by display
    printer.currentTemp = dummyTemp;
    printer.progress = dummyProgress;

    checkButton();
    updateLCD();
}
#endif

#ifdef ENABLE_AXIS_CYCLE_TEST
// Axis order: X Y Z E
static int currentAxis = 0;
static bool moving = false;
static const char axisChars[] = {'X', 'Y', 'Z', 'E'};

void axisTestSetup() {
    showMessage("Axis Test", "Press Button");
    Serial.println(F("Axis cycle test ready"));
}

void axisTestLoop() {
    updateButton();
    if (justPressed()) {
        if (moving) {
            moving = false;
            currentAxis = (currentAxis + 1) % 4;
            char buf[17];
            snprintf(buf, sizeof(buf), "Next: %c axis", axisChars[currentAxis]);
            showMessage(buf, "Press to Start");
            Serial.println(buf);
        } else {
            moving = true;
            char buf[17];
            snprintf(buf, sizeof(buf), "Moving %c axis", axisChars[currentAxis]);
            showMessage(buf, "Press to Stop");
            Serial.println(buf);
        }
    }

    if (moving) {
        long* posPtr;
        int stepPin, dirPin;
        char axis;
        switch (currentAxis) {
            case 0: axis = 'X'; posPtr = &printer.posX; stepPin = stepPinX; dirPin = dirPinX; break;
            case 1: axis = 'Y'; posPtr = &printer.posY; stepPin = stepPinY; dirPin = dirPinY; break;
            case 2: axis = 'Z'; posPtr = &printer.posZ; stepPin = stepPinZ; dirPin = dirPinZ; break;
            default: axis = 'E'; posPtr = &printer.posE; stepPin = stepPinE; dirPin = dirPinE; break;
        }
        moveAxis(stepPin, dirPin, *posPtr, *posPtr + 1, 600, axis);
    }
}
#endif

