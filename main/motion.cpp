#include "motion.h"
#include "pins.h"
#include "state.h"
#include "gcode.h"
#include <Arduino.h>

// Access button handling from main program
extern void checkButton();
extern bool useRelativeE;
extern int displayMode;
extern void updateLCD();


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

    unsigned long lastPoll = millis();
    for (long i = 0; i < steps; i++) {
        digitalWrite(stepPin, HIGH);
        // Maintain high pulse for reliable 1/4 step operation
        delayMicroseconds(1000);
        digitalWrite(stepPin, LOW);

        unsigned long now = millis();
        if (now - lastPoll >= 50) {
            lastPoll = now;
            checkButton();
            if (displayMode == 1) updateLCD();
        }

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
    int distance;
    if (axis == 'E' && useRelativeE) {
        distance = target;
    } else {
        distance = useAbsoluteXYZ ? target - pos : target;
    }

    float spm = stepsPerMM_X;
    if (axis == 'Y') spm = stepsPerMM_Y;
    else if (axis == 'Z') spm = stepsPerMM_Z;
    else if (axis == 'E') spm = stepsPerMM_E;

    long steps = calculateSteps(axis, pos, distance, spm);
#ifdef SIMULATE_EXTRUDER
    if (axis == 'E') {
        pos += distance; // update without physical movement
        return;
    }
#endif
    if (steps == 0) {
        pos += distance; // update by actual movement (likely zero)
        return;
    }

    digitalWrite(motorEnablePin, LOW);
    setMotorDirection(dirPin, distance);

    long stepPeriod = (long)(60000000.0 / (feedrate * spm));
    // account for 1 ms high pulse so resulting period matches commanded F
    long minDelay = max(50L, stepPeriod - 1000L);

    moveWithAccel(stepPin, steps, minDelay);

    digitalWrite(motorEnablePin, HIGH);

    // Update position once using final travel distance
    pos += distance;
}

void homeAxis(int stepPin, int dirPin, int endstopPin, const char* label) {
    digitalWrite(motorEnablePin, LOW);
    digitalWrite(dirPin, LOW);
    while (digitalRead(endstopPin) == HIGH) {
        digitalWrite(stepPin, HIGH);
        delayMicroseconds(1000);
        digitalWrite(stepPin, LOW);
        delayMicroseconds(1000);
    }
    digitalWrite(motorEnablePin, HIGH);
    extern void sendOk(const String &msg); // from gcode.cpp
    sendOk(String(label) + " Homed");
}

// Accelerated multi-axis movement using Bresenham/DDA
static void moveWithAccelSync(long stepsX, long stepsY, long stepsZ, long stepsE,
                              long maxSteps, long minDelay) {
    const int ACCEL_STEPS = 50;
    long startDelay = minDelay * 2;
    int rampSteps = min(maxSteps / 2, (long)ACCEL_STEPS);
    long delayDelta = rampSteps > 0 ? (startDelay - minDelay) / rampSteps : 0;
    long currentDelay = startDelay;

    long errX = maxSteps / 2;
    long errY = maxSteps / 2;
    long errZ = maxSteps / 2;
    long errE = maxSteps / 2;

    unsigned long lastPoll = millis();
    for (long i = 0; i < maxSteps; i++) {
        bool doX = false, doY = false, doZ = false, doE = false;
        if (stepsX) { errX -= stepsX; if (errX < 0) { errX += maxSteps; doX = true; } }
        if (stepsY) { errY -= stepsY; if (errY < 0) { errY += maxSteps; doY = true; } }
        if (stepsZ) { errZ -= stepsZ; if (errZ < 0) { errZ += maxSteps; doZ = true; } }
        if (stepsE) { errE -= stepsE; if (errE < 0) { errE += maxSteps; doE = true; } }

        if (doX) digitalWrite(stepPinX, HIGH);
        if (doY) digitalWrite(stepPinY, HIGH);
        if (doZ) digitalWrite(stepPinZ, HIGH);
#ifndef SIMULATE_EXTRUDER
        if (doE) digitalWrite(stepPinE, HIGH);
#endif
        if (doX || doY || doZ || doE) delayMicroseconds(1000);
        if (doX) { digitalWrite(stepPinX, LOW); if (printer.remStepX > 0) printer.remStepX--; }
        if (doY) { digitalWrite(stepPinY, LOW); if (printer.remStepY > 0) printer.remStepY--; }
        if (doZ) { digitalWrite(stepPinZ, LOW); if (printer.remStepZ > 0) printer.remStepZ--; }
#ifndef SIMULATE_EXTRUDER
        if (doE) { digitalWrite(stepPinE, LOW); if (printer.remStepE > 0) printer.remStepE--; }
#else
        if (doE && printer.remStepE > 0) printer.remStepE--;
#endif

        unsigned long now = millis();
        if (now - lastPoll >= 50) {
            lastPoll = now;
            checkButton();
            if (displayMode == 1) updateLCD();
        }

        delayMicroseconds(currentDelay);

        if (rampSteps > 0) {
            if (i < rampSteps) {
                currentDelay = max(minDelay, currentDelay - delayDelta);
            } else if (i >= maxSteps - rampSteps) {
                currentDelay = min(startDelay, currentDelay + delayDelta);
            }
        }
    }
}

void moveAxes(long targetX, long targetY, long targetZ, long targetE, int feedrate) {
    int distX = useAbsoluteXYZ ? targetX - printer.posX : targetX;
    int distY = useAbsoluteXYZ ? targetY - printer.posY : targetY;
    int distZ = useAbsoluteXYZ ? targetZ - printer.posZ : targetZ;
    int distE;
    if (useRelativeE) {
        distE = targetE;
    } else {
        distE = useAbsoluteXYZ ? targetE - printer.posE : targetE;
    }

    float spmX = stepsPerMM_X;
    float spmY = stepsPerMM_Y;
    float spmZ = stepsPerMM_Z;
    float spmE = stepsPerMM_E;

    long stepsX = calculateSteps('X', printer.posX, distX, spmX);
    long stepsY = calculateSteps('Y', printer.posY, distY, spmY);
    long stepsZ = calculateSteps('Z', printer.posZ, distZ, spmZ);
    long stepsE = calculateSteps('E', printer.posE, distE, spmE);

    long maxSteps = max(max(stepsX, stepsY), max(stepsZ, stepsE));
    if (maxSteps == 0) {
        printer.posX += distX;
        printer.posY += distY;
        printer.posZ += distZ;
        printer.posE += distE;
        return;
    }

    if (distE != 0) {
        if (printer.eTotal == -1) {
            Serial.println(F("WARN: eTotal unset"));
        }
        if (!printer.eStartSynced) {
            printer.eStart = printer.posE;
            printer.eStartSynced = true;
        }
    }

    digitalWrite(motorEnablePin, LOW);
    setMotorDirection(dirPinX, distX);
    setMotorDirection(dirPinY, distY);
    setMotorDirection(dirPinZ, distZ);
#ifndef SIMULATE_EXTRUDER
    setMotorDirection(dirPinE, distE);
#endif

    float spmLongest = spmX;
    if (stepsY >= stepsX && stepsY >= stepsZ && stepsY >= stepsE) spmLongest = spmY;
    else if (stepsZ >= stepsX && stepsZ >= stepsY && stepsZ >= stepsE) spmLongest = spmZ;
    else if (stepsE >= stepsX && stepsE >= stepsY && stepsE >= stepsZ) spmLongest = spmE;

    long stepPeriod = (long)(60000000.0 / (feedrate * spmLongest));
    long minDelay = max(50L, stepPeriod - 1000L);

    moveWithAccelSync(stepsX, stepsY, stepsZ, stepsE, maxSteps, minDelay);

    digitalWrite(motorEnablePin, HIGH);

    printer.posX += distX;
    printer.posY += distY;
    printer.posZ += distZ;
    printer.posE += distE;

    updateProgress();

    char axis = 'X';
    int disp = distX;
    long absDist = labs(distX);
    if (labs(distY) > absDist) { axis='Y'; absDist=labs(distY); disp = distY; }
    if (labs(distZ) > absDist) { axis='Z'; absDist=labs(distZ); disp = distZ; }
    if (labs(distE) > absDist) { axis='E'; absDist=labs(distE); disp = distE; }
    printer.movingAxis = axis;
    printer.movingDir = (disp >= 0) ? 1 : -1;
    printer.lastMoveTime = millis();
}

