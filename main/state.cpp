#include "state.h"
#include <Arduino.h>
#include "tunes.h"

PrinterState printer;

void resetPrinterState() {
    printer.setTemp = 0.0f;
    printer.currentTemp = 0.0f;
    printer.rawTemp = 0;
    printer.tempError = false;
    printer.tempErrorNotified = false;
    printer.heatDoneBeeped = false;
    printer.waitingForHeat = false;

    printer.posX = printer.posY = printer.posZ = printer.posE = 0;
    printer.eStart = 0;
    // -1 indicates progress total not set
    printer.eTotal = -1;
    printer.progress = 0;
    printer.eStartSynced = false;

    printer.heaterOn = false;

    printer.movingAxis = ' ';
    printer.movingDir = 0;
    printer.lastMoveTime = 0;

    printer.Kp = 20.0f;
    printer.Ki = 1.0f;
    printer.Kd = 50.0f;
    printer.pwmValue = 0.0f;
    printer.lastOutput = 0.0f;
    printer.integral = 0.0f;
    printer.previousError = 0.0f;
    printer.lastTime = millis();

    printer.currentTune = TUNE_MARIO; // default tune defined in tunes.h

    printer.paused = false;
}

void updateProgress() {
    if (printer.eTotal > 0) {
        if (printer.eStart > printer.posE) {
            // Avoid negative delta when retracting
            printer.eStart = printer.posE;
        }
        long delta = printer.posE - printer.eStart;
        if (delta >= printer.eTotal) {
            printer.progress = 100;
            // Mark print as complete until user confirms
            printer.eTotal = 0;
        } else if (delta > 0) {
            printer.progress = (int)(delta * 100L / printer.eTotal);
        }
    }
}
