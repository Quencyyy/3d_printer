//#define ENABLE_HOMING
// Optional test modes (disabled by default)
//#define ENABLE_BUTTON_MENU_TEST
//#define ENABLE_AXIS_CYCLE_TEST
// Uncomment to feed predefined G-code without host software
//#define DEBUG_INPUT

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
#include "button.h"
#include <EEPROM.h>
#include "pins.h"
#include "temp_control.h"
#include "gcode.h"
#include "tunes.h"
#include "test_modes.h"
#include "state.h"
#include "motion.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);

int currentFeedrate = 1000;  // 預設速度 mm/min
unsigned long heatStableStart = 0;
const unsigned long stableHoldTime = 3000;

bool useAbsolute = true;

float stepsPerMM_X = 80.0;
float stepsPerMM_Y = 80.0;
float stepsPerMM_Z = 80.0;
float stepsPerMM_E = 80.0;

int displayMode = 0;
unsigned long lastPressTime = 0;
unsigned long lastDisplaySwitch = 0;
const unsigned long autoSwitchDelay = 10000;
bool isLongPress = false;
bool confirmStop = false;
unsigned long confirmStartTime = 0;

unsigned long lastLoopTime = 0;
const unsigned long loopInterval = 100;

String lastDisplayContent = "";

// 進度估算變數由 state 模組管理

void saveSettingsToEEPROM() {
    EEPROM.put(0, printer.Kp);
    EEPROM.put(4, printer.Ki);
    EEPROM.put(8, printer.Kd);
    EEPROM.put(12, printer.setTemp);
    EEPROM.put(16, stepsPerMM_X);
    EEPROM.put(20, stepsPerMM_Y);
    EEPROM.put(24, stepsPerMM_Z);
    EEPROM.put(28, stepsPerMM_E);
}

void loadSettingsFromEEPROM() {
    EEPROM.get(0, printer.Kp);
    EEPROM.get(4, printer.Ki);
    EEPROM.get(8, printer.Kd);
    EEPROM.get(12, printer.setTemp);
    EEPROM.get(16, stepsPerMM_X);
    EEPROM.get(20, stepsPerMM_Y);
    EEPROM.get(24, stepsPerMM_Z);
    EEPROM.get(28, stepsPerMM_E);
}




void clearTempError() {
    if (printer.currentTemp > -10 && printer.currentTemp < 300) {
        printer.tempError = false;
        printer.tempErrorNotified = false;
        showMessage("Sensor OK", "System Normal");
        delay(500);
        lastDisplayContent = "";
    }
}


void showMessage(const char* line1, const char* line2) {
    String content = String(line1) + "\n" + String(line2);
    if (content != lastDisplayContent) {
        lastDisplayContent = content;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(line1);
        lcd.setCursor(0, 1);
        lcd.print(line2);
    }
}

void displayTempScreen() {
    char buf[17];
    snprintf(buf, sizeof(buf), "T:%.1f%cC Set:%.0f", printer.currentTemp, 223, printer.setTemp);
    showMessage(buf, "");
}

void displayCoordScreen() {
    char buf1[17], buf2[17];
    snprintf(buf1, sizeof(buf1), "X%ld Y%ld", printer.posX, printer.posY);
    snprintf(buf2, sizeof(buf2), "Z%ld E%ld", printer.posZ, printer.posE);
    showMessage(buf1, buf2);
}

void displayStatusScreen() {
    if (printer.tempError) {
        showMessage("Sensor ERROR!", "Check & Press Btn");
    } else {
        char bar[11];
        int filled = constrain(printer.progress / 10, 0, 10);
        for (int i = 0; i < 10; i++) {
            bar[i] = (i < filled) ? '#' : '-';
        }
        bar[10] = '\0';

        char line1[17];
        if (printer.progress >= 100)
            snprintf(line1, sizeof(line1), "[%s]%3d%%", bar, printer.progress);
        else
            snprintf(line1, sizeof(line1), "[%s] %3d%%", bar, printer.progress);
        showMessage(line1, "");
    }
}

void updateLCD() {
    static int animPos = 0;
    static const char anim[] = "|/-\\";

    if (displayMode == 0) {
        displayTempScreen();
    } else if (displayMode == 1) {
        displayCoordScreen();
    } else {
        displayStatusScreen();
    }

    bool moving = (millis() - printer.lastMoveTime) < 1000 && printer.movingAxis != ' ';
    lcd.setCursor(12, 1);
    lcd.print(printer.fanOn ? 'F' : ' ');
    lcd.print(printer.heaterOn ? 'H' : ' ');
    if (moving) {
        lcd.print(printer.movingAxis);
        lcd.print(printer.movingDir > 0 ? '>' : '<');
    } else {
        lcd.print(' ');
        lcd.print(anim[animPos]);
        animPos = (animPos + 1) % 4;
    }
}


void checkButton() {
    updateButton();
    bool state = isPressed();
    static bool prevState = false;
    static unsigned long pressStartTime = 0;
    unsigned long now = millis();

    if (printer.tempError) {
        clearTempError();
        prevState = state;
        return;
    }

    if (justPressed()) {
        pressStartTime = now;
        if (confirmStop) {
            confirmStartTime = now;
        }
    }

    if (state && !isLongPress && now - pressStartTime > 50) {
        if (longPressed(3000)) {
            if (confirmStop) {
                if (now - confirmStartTime >= 3000) {
                    forceStop();
                    confirmStop = false;
                }
            } else {
                confirmStop = true;
                confirmStartTime = now;
                showMessage("Confirm Stop?", "Hold 3s again");
            }
            isLongPress = true;
        }
    }

    if (!state && prevState) {
        if (confirmStop && now - confirmStartTime < 5000) {
            confirmStop = false;
            showMessage("Cancelled", "");
            delay(300);
        } else if (!isLongPress) {
            displayMode = (displayMode + 1) % 3;
            lastDisplaySwitch = now;
        }
        isLongPress = false;
        lastPressTime = now;
    }

    prevState = state;
}

void autoSwitchDisplay() {
    if (displayMode != 2 && printer.progress < 100 && printer.eStartSynced) {
        unsigned long now = millis();
        if (now - lastDisplaySwitch >= autoSwitchDelay) {
            displayMode = 2;
            lastDisplaySwitch = now;
        }
    }
}

void forceStop() {
    printer.setTemp = 0;
    analogWrite(heaterPin, 0);
    printer.heaterOn = false;
    digitalWrite(fanPin, LOW);
    printer.fanOn = false;
    showMessage("** Forced STOP **", "");
}




void setup() {
    initButton(buttonPin);

    pinMode(heaterPin, OUTPUT);
    pinMode(fanPin, OUTPUT);
#ifdef ENABLE_BUZZER
    pinMode(buzzerPin, OUTPUT);
#endif
    pinMode(motorEnablePin, OUTPUT);
    digitalWrite(motorEnablePin, LOW);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("System Ready");
    delay(1000);
    lcd.clear();
    lastDisplaySwitch = millis();

    Serial.begin(9600);
    loadSettingsFromEEPROM();
#ifdef ENABLE_BUTTON_MENU_TEST
    testMenuSetup();
#endif
#ifdef ENABLE_AXIS_CYCLE_TEST
    axisTestSetup();
#endif
}

void loop() {
#ifdef ENABLE_BUTTON_MENU_TEST
    testMenuLoop();
    return;
#endif
#ifdef ENABLE_AXIS_CYCLE_TEST
    axisTestLoop();
    return;
#endif
    unsigned long now = millis();
    if (now - lastLoopTime >= loopInterval) {
        lastLoopTime = now;
        readTemperature();
        controlHeater();
        checkButton();
        autoSwitchDisplay();
        updateLCD();
        processGcode();
    }
}

