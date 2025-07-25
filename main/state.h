#pragma once

struct PrinterState {
    // 溫度控制
    float setTemp;
    float currentTemp;
    int rawTemp; // last raw ADC value for debugging
    bool heatDoneBeeped;
    bool waitingForHeat;
    float pwmValue;
    float lastOutput;

    // 馬達與進度
    float posX, posY, posZ, posE;
    float eStart, eTotal;
    int progress;
    bool eStartSynced;

    // 動態狀態
    bool heaterOn;

    // 顯示與動作
    char movingAxis;
    int movingDir;
    unsigned long lastMoveTime;

    // PID
    float Kp, Ki, Kd;
    float integral, previousError;
    unsigned long lastTime;

    // 暫停狀態 (M0)
    bool paused;

    // Upcoming and remaining move tracking
    float nextX, nextY, nextZ, nextE; // next target or relative move
    bool hasNextMove;
    long remStepX, remStepY, remStepZ, remStepE; // remaining steps during move
    int signX, signY, signZ, signE; // direction of current move
};

extern PrinterState printer;

void resetPrinterState();
void updateProgress();
