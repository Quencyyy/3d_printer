#include "state.h"
#include <Arduino.h>
#include "tunes.h"

PrinterState printer;

void resetPrinterState() {
    printer.setTemp = 0.0f;
    printer.currentTemp = 0.0f;
    printer.rawTemp = 0;
    printer.heatDoneBeeped = false;
    printer.waitingForHeat = false;

    printer.posX = printer.posY = printer.posZ = printer.posE = 0.0f;
    printer.extrusionStartMM = 0.0f;
    // -1 indicates progress total not set
    printer.extrusionTotalMM = -1.0f;
    printer.progress = 0;
    printer.isExtrusionStartSynced = false;

    printer.heaterOn = false;
    printer.motorsEnabled = false;

    printer.movingAxis = ' ';
    printer.movingDir = 0;
    printer.lastMoveTime = 0;

    printer.Kp = 0.6f;
    printer.Ki = 0.05f;
    printer.Kd = 1.2f;
    printer.pwmValue = 0.0f;
    printer.lastOutput = 0.0f;
    printer.integral = 0.0f;
    printer.previousError = 0.0f;
    printer.lastTime = millis();


    printer.paused = false;
    printer.dwellActive = false;
    printer.dwellUntil = 0;

    printer.nextX = printer.nextY = printer.nextZ = printer.nextE = 0.0f;
    printer.hasNextMove = false;
    printer.remainingStepsX = printer.remainingStepsY = printer.remainingStepsZ = printer.remainingStepsE = 0;
    printer.signX = printer.signY = printer.signZ = printer.signE = 1;

    printer.currentCmd[0] = '\0';
}

void updateProgress() {
    if (printer.extrusionTotalMM > 0.0f) {
        if (printer.extrusionStartMM > printer.posE) {
            // Avoid negative delta when retracting
            printer.extrusionStartMM = printer.posE;
        }
        float delta = printer.posE - printer.extrusionStartMM;
        if (delta >= printer.extrusionTotalMM) {
            printer.progress = 100;
            // Mark print as complete until user confirms
            printer.extrusionTotalMM = 0.0f;
        } else if (delta > 0.0f) {
            printer.progress = (int)(delta * 100.0f / printer.extrusionTotalMM);
        }
    }
}
