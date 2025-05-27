#pragma once

// 馬達控制腳位
extern const int stepPinX, dirPinX;
extern const int stepPinY, dirPinY;
extern const int stepPinZ, dirPinZ;
extern const int stepPinE, dirPinE;

// 硬體控制腳位
extern const int fanPin;
extern const int heaterPin;
extern const int buzzerPin;
extern const int buttonPin;

// 軟體旗標與限制
extern int eMaxSteps;
extern bool eStartSynced;
