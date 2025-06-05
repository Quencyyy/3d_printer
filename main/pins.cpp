#include "pins.h"
#include <Arduino.h>

// CNC Shield 馬達腳位對應
const int stepPinX = 2;
const int dirPinX  = 5;
const int stepPinY = 3;
const int dirPinY  = 6;
const int stepPinZ = 4;
const int dirPinZ  = 7;
const int stepPinE = 8;
const int dirPinE  = 9;

// 控制腳位
const int fanPin    = 10;
const int heaterPin = 11;   // 例如 D11 可做 PWM 輸出
const int buzzerPin = 12;   // 例如 D12 連接蜂鳴器
const int buttonPin = 13;

const int endstopX = A1;
const int endstopY = A2;
const int endstopZ = A3;

// 軟體參數
int eMaxSteps = 1000;
bool eStartSynced = false;
