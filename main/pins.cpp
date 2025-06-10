#include "pins.h"
#include <Arduino.h>

// CNC Shield 馬達腳位對應
const int stepPinX = 2;
const int dirPinX  = 5;
const int stepPinY = 3;
const int dirPinY  = 6;
const int stepPinZ = 4;
const int dirPinZ  = 7;
// Extruder uses D12 so motor enable can stay on D8
const int stepPinE = 12;
const int dirPinE  = 9;

// 控制腳位
const int fanPin    = 10;
const int heaterPin = 11;   // 例如 D11 可做 PWM 輸出
#ifndef BUZZER_PIN
#define BUZZER_PIN 8
#endif
// Buzzer pin defaults to D8 but can be overridden with BUZZER_PIN
const int buzzerPin = BUZZER_PIN;
// Motor enable uses D8; conflicts with buzzer if BUZZER_PIN is also 8
const int motorEnablePin = 8;
const int buttonPin = 13;

const int endstopX = A1;
const int endstopY = A2;
const int endstopZ = A3;

// 軟體參數
int eMaxSteps = 1000;
bool eStartSynced = false;
