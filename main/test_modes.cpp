#include "test_modes.h"
#include "pins.h"
#include "gcode.h"
#include "display.h"
#include <Bounce2.h>
#include <LiquidCrystal_I2C.h>

// References to globals from main program
extern Bounce debouncer;
extern LiquidCrystal_I2C lcd;
extern DisplayMode displayMode;
extern long posX, posY, posZ, posE;
extern void showMessage(const char*, const char*);
extern void checkButton();
extern void updateLCD();
extern void moveAxis(int stepPin, int dirPin, long& pos, int target, int feedrate);

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
    extern float currentTemp; extern int progress;
    currentTemp = dummyTemp;
    progress = dummyProgress;

    checkButton();
    updateLCD();
}
#endif

#ifdef ENABLE_AXIS_CYCLE_TEST
// Axis order: X Y Z E
static int currentAxis = 0;
static bool moving = false;
static bool lastBtnState = HIGH;

void axisTestSetup() {
    showMessage("Axis Test", "Press Button");
}

void axisTestLoop() {
    debouncer.update();
    bool btn = debouncer.read() == LOW;
    if (btn && !lastBtnState) { // button pressed
        if (moving) {
            moving = false;
            currentAxis = (currentAxis + 1) % 4;
            showMessage("Switch Axis", "Press to Start");
        } else {
            moving = true;
            showMessage("Moving Axis", "Press to Stop");
        }
        delay(200); // simple debounce delay
    }
    lastBtnState = btn;

    if (moving) {
        long* posPtr;
        int stepPin, dirPin;
        switch (currentAxis) {
            case 0: posPtr = &posX; stepPin = stepPinX; dirPin = dirPinX; break;
            case 1: posPtr = &posY; stepPin = stepPinY; dirPin = dirPinY; break;
            case 2: posPtr = &posZ; stepPin = stepPinZ; dirPin = dirPinZ; break;
            default: posPtr = &posE; stepPin = stepPinE; dirPin = dirPinE; break;
        }
        moveAxis(stepPin, dirPin, *posPtr, *posPtr + 1, 600);
    }
}
#endif

