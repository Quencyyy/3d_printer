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
    printer.eStart = 0.0f;
    // -1 indicates progress total not set
    printer.eTotal = -1.0f;
    printer.progress = 0;
    printer.eStartSynced = false;

    printer.heaterOn = false;

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

    printer.nextX = printer.nextY = printer.nextZ = printer.nextE = 0.0f;
    printer.hasNextMove = false;
    printer.remStepX = printer.remStepY = printer.remStepZ = printer.remStepE = 0;
    printer.signX = printer.signY = printer.signZ = printer.signE = 1;

    printer.currentCmd[0] = '\0';
}

void updateProgress() {
    if (printer.eTotal > 0.0f) {
        if (printer.eStart > printer.posE) {
            // Avoid negative delta when retracting
            printer.eStart = printer.posE;
        }
        float delta = printer.posE - printer.eStart;
        if (delta >= printer.eTotal) {
            printer.progress = 100;
            // Mark print as complete until user confirms
            printer.eTotal = 0.0f;
        } else if (delta > 0.0f) {
            printer.progress = (int)(delta * 100.0f / printer.eTotal);
        }
    }
}
