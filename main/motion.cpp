#include "motion.h"
#include "pins.h"
#include "state.h"
#include "gcode.h"
#include <Arduino.h>

// Determine whether the axis should be physically moved
static bool isPhysicalAxis(char axis) {
    // When eTotal is negative we are in "virtual extrusion" mode
    return !(axis == 'E' && printer.eTotal < 0);
}

// Calculate step count and apply extrusion limits
static long calculateSteps(char axis, long currentPos, int &distance, float spm) {
    if (axis == 'E' && distance > 0) {
        extern int eMaxSteps;
        if (currentPos + distance > eMaxSteps) {
            distance = eMaxSteps - currentPos;
            if (distance <= 0) return 0;
        }
    }
    return lroundf(fabs(distance * spm));
}

// Set motor direction based on travel distance
static void setMotorDirection(int dirPin, int distance) {
    int dir = (distance >= 0) ? HIGH : LOW;
    digitalWrite(dirPin, dir);
}

// Simple acceleration control
static void moveWithAccel(int stepPin, long steps, long minDelay) {
    const int ACCEL_STEPS = 50;
    long startDelay = minDelay * 2;  // start slower then ramp up
    int rampSteps = min(steps / 2, (long)ACCEL_STEPS);
    long delayDelta = rampSteps > 0 ? (startDelay - minDelay) / rampSteps : 0;
    long currentDelay = startDelay;

    for (long i = 0; i < steps; i++) {
        digitalWrite(stepPin, HIGH);
        delayMicroseconds(5);
        digitalWrite(stepPin, LOW);
        delayMicroseconds(currentDelay);

        if (rampSteps > 0) {
            if (i < rampSteps) {
                // acceleration zone
                currentDelay = max(minDelay, currentDelay - delayDelta);
            } else if (i >= steps - rampSteps) {
                // deceleration zone
                currentDelay = min(startDelay, currentDelay + delayDelta);
            }
        }
    }
}

void moveAxis(int stepPin, int dirPin, long& pos, int target, int feedrate, char axis) {
    int distance = useAbsolute ? target - pos : target;

    float spm = stepsPerMM_X;
    if (axis == 'Y') spm = stepsPerMM_Y;
    else if (axis == 'Z') spm = stepsPerMM_Z;
    else if (axis == 'E') spm = stepsPerMM_E;

    long steps = calculateSteps(axis, pos, distance, spm);
    if (steps == 0) {
        pos += distance; // update by actual movement (likely zero)
        return;
    }

    if (isPhysicalAxis(axis)) {
        digitalWrite(motorEnablePin, HIGH);
        setMotorDirection(dirPin, distance);

        long minDelay = (long)(60000000.0 / (feedrate * spm));
        minDelay = max(50L, minDelay);  // minimum safety

        moveWithAccel(stepPin, steps, minDelay);

        digitalWrite(motorEnablePin, LOW);
    }

    // Update position once using final travel distance
    pos += distance;
}

#ifdef ENABLE_HOMING
void homeAxis(int stepPin, int dirPin, int endstopPin, const char* label) {
    digitalWrite(motorEnablePin, HIGH);
    digitalWrite(dirPin, LOW);
    while (digitalRead(endstopPin) == HIGH) {
        digitalWrite(stepPin, HIGH);
        delayMicroseconds(800);
        digitalWrite(stepPin, LOW);
        delayMicroseconds(800);
    }
    digitalWrite(motorEnablePin, LOW);
    extern void sendOk(const String &msg); // from gcode.cpp
    sendOk(String(label) + " Homed");
}
#endif
