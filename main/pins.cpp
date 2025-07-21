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
const int dirPinE  = 13;

const int heaterPin = 10;//Y- 3,5,6,9,10,11可做 PWM 輸出
// Thermistor connected to analog pin A3
const int tempPin   = A3;//Cooler
#ifndef BUZZER_PIN
#define BUZZER_PIN 9
#endif
// Buzzer pin defaults to D9 but can be overridden with BUZZER_PIN
const int buzzerPin = BUZZER_PIN;
// Motor enable uses D8
const int motorEnablePin = 8;
const int buttonPin = 11;//Z-

const int endstopX = A0;//D9 -> Abort
const int endstopY = A1;//D10 -> Hold
const int endstopZ = A2;//D11 -> Resume

// 軟體參數
int eMaxSteps = 1000;
