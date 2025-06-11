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

    printer.posX = printer.posY = printer.posZ = printer.posE = 0;
    printer.eStart = 0;
    printer.eTotal = 1000;
    printer.progress = 0;
    printer.eStartSynced = false;

    printer.fanOn = false;
    printer.fanStarted = false;
    printer.fanForced = false;
    printer.heaterOn = false;

    printer.movingAxis = ' ';
    printer.movingDir = 0;
    printer.lastMoveTime = 0;

    printer.Kp = 20.0f;
    printer.Ki = 1.0f;
    printer.Kd = 50.0f;
    printer.integral = 0.0f;
    printer.previousError = 0.0f;
    printer.lastTime = millis();

    printer.currentTune = TUNE_MARIO; // default tune defined in tunes.h
}

void updateProgress() {
    if (printer.eTotal > 0) {
        if (printer.eStart > printer.posE) {
            // 若 E 軸回抽導致起點大於當前位置，歸零避免負值
            printer.eStart = printer.posE;
        }
        long delta = printer.posE - printer.eStart;
        if (delta > 0 && delta <= printer.eTotal) {
            printer.progress = (int)(delta * 100L / printer.eTotal);
        } else if (delta > printer.eTotal) {
            printer.progress = 100;
        }
    }
}
