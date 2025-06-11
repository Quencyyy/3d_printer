#pragma once

struct PrinterState {
    // 溫度控制
    float setTemp;
    float currentTemp;
    bool tempError;
    bool tempErrorNotified;
    bool heatDoneBeeped;

    // 馬達與進度
    long posX, posY, posZ, posE;
    long eStart, eTotal;
    int progress;
    bool eStartSynced;

    // 動態狀態
    bool fanOn;
    bool fanStarted;
    bool fanForced;
    bool heaterOn;

    // 顯示與動作
    char movingAxis;
    int movingDir;
    unsigned long lastMoveTime;

    // PID
    float Kp, Ki, Kd;
    float integral, previousError;
    unsigned long lastTime;

    // 音樂
    int currentTune;
};

extern PrinterState printer;

void resetPrinterState();
void updateProgress();
