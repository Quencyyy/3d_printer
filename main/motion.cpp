#include "motion.h"
#include "pins.h"
#include "state.h"
#include <Arduino.h>

// Prevent over-extrusion for the E axis (distance is in mm)
static long applyELimit(char axis, long pos, int &distance, float spm) {
    if (axis == 'E' && distance > 0) {
        extern int eMaxSteps;
        if (pos + distance > eMaxSteps) {
            distance = eMaxSteps - pos;
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

    digitalWrite(motorEnablePin, HIGH);
    setMotorDirection(dirPin, distance);

    long steps = applyELimit(axis, pos, distance, spm);  // limit E axis travel
    if (steps == 0) {
        digitalWrite(motorEnablePin, LOW);
        return;
    }

    long minDelay = (long)(60000000.0 / (feedrate * spm));
    minDelay = max(50L, minDelay);  // minimum safety

    moveWithAccel(stepPin, steps, minDelay);  // perform movement with accel

    digitalWrite(motorEnablePin, LOW);
    pos = useAbsolute ? target : pos + target;
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
    Serial.print(label);
    Serial.println(" Homed");
}
#endif
