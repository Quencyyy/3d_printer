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

#ifdef ENABLE_MOTOR_ENABLE
extern const int motorEnablePin;
#endif
extern const int buttonPin;
extern const int endstopX;
extern const int endstopY;
extern const int endstopZ;

// 軟體旗標與限制
extern int eMaxSteps;
extern bool eStartSynced;
