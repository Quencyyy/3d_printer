// Simulation flags are defined in config.h
#include "config.h"

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "button.h"
#include <EEPROM.h>
#include "pins.h"
#include "temp_control.h"
#include "gcode.h"
#include "tunes.h"
#include "state.h"
#include "motion.h"
#include "interrupts.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);

int currentFeedrate = 1200;  // 預設速度 mm/min
unsigned long heatStableStart = 0;
const unsigned long stableHoldTime = 3000;

bool useAbsoluteXYZ = true;
bool useRelativeE = false;
float feedrateMultiplier = 1.0f;
float flowrateMultiplier = 1.0f;

float stepsPerMM_X = 25.0;
float stepsPerMM_Y = 25.0;
float stepsPerMM_Z = 25.0;
float stepsPerMM_E = 25.0;

int displayMode = 0;
unsigned long lastPressTime = 0;
unsigned long lastDisplaySwitch = 0;
const unsigned long autoSwitchDelay = 30000;
// Immediately show the idle screen after startup
const unsigned long idleSwitchDelay = 0;
bool isLongPress = false;
bool displayFrozen = false;
unsigned long freezeStartTime = 0;
const unsigned long freezeDuration = 3000;

unsigned long lastLoopTime = 0;
const unsigned long loopInterval = 100;

char lastDisplayContent[33] = {0};

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
    EEPROM.put(32, printer.zOffset);
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
    EEPROM.get(32, printer.zOffset);

    // Validate values in case EEPROM has never been written
    if (!isfinite(printer.Kp) || !isfinite(printer.Ki) || !isfinite(printer.Kd)) {
        printer.Kp = 0.6f;
        printer.Ki = 0.05f;
        printer.Kd = 1.2f;
    }
    if (!isfinite(stepsPerMM_X)) stepsPerMM_X = 25.0f;
    if (!isfinite(stepsPerMM_Y)) stepsPerMM_Y = 25.0f;
    if (!isfinite(stepsPerMM_Z)) stepsPerMM_Z = 25.0f;
    if (!isfinite(stepsPerMM_E)) stepsPerMM_E = 25.0f;
    if (!isfinite(printer.zOffset)) printer.zOffset = 0.0f;
    if (!isfinite(printer.setTemp) || printer.setTemp < 0 || printer.setTemp > 300) {
        printer.setTemp = 0.0f;
    }
}





void showMessage(const char* line1, const char* line2) {
    char newContent[33];
    for (int i = 0; i < 16; i++) {
        newContent[i] = (i < strlen(line1)) ? line1[i] : ' ';
        newContent[i + 16] = (i < strlen(line2)) ? line2[i] : ' ';
    }
    newContent[32] = '\0';

    if (memcmp(newContent, lastDisplayContent, 32) == 0) return;

    for (int i = 0; i < 32; i++) {
        if (newContent[i] != lastDisplayContent[i]) {
            lcd.setCursor(i % 16, i / 16);
            lcd.print(newContent[i]);
            lastDisplayContent[i] = newContent[i];
        }
    }
}

void displayProgressScreen() {
    if (printer.eTotal == 0) {
        showMessage("Print Complete", "Press Button");
        return;
    }

    if (printer.eTotal < 0) {
        showMessage("No Print Job", "");
        return;
    }

    char bar[11];
    int filled = constrain(printer.progress / 10, 0, 10);
    for (int i = 0; i < 10; i++) {
        bar[i] = (i < filled) ? '#' : '-';
    }
    bar[10] = '\0';

    char line1[17];
    line1[0] = '[';
    for (int i = 0; i < 10; i++) line1[i + 1] = bar[i];
    line1[11] = ']';
    int p = printer.progress;
    line1[12] = (p >= 100) ? (p / 100 + '0') : ' ';
    line1[13] = (p >= 10) ? ((p / 10) % 10 + '0') : ' ';
    line1[14] = (p % 10) + '0';
    line1[15] = '%';
    line1[16] = '\0';

    char line2[17];
    line2[0] = 'T';
    line2[1] = ':';
    int t10 = (int)round(printer.currentTemp * 10);
    if (t10 < 0) {
        line2[2] = '-';
        t10 = -t10;
        itoa(t10 / 10, line2 + 3, 10);
        int len = strlen(line2 + 3) + 3;
        line2[len++] = '.';
        line2[len++] = (t10 % 10) + '0';
        line2[len++] = 223;
        line2[len++] = 'C';
        line2[len] = '\0';
    } else {
        itoa(t10 / 10, line2 + 2, 10);
        int len = strlen(line2 + 2) + 2;
        line2[len++] = '.';
        line2[len++] = (t10 % 10) + '0';
        line2[len++] = 223;
        line2[len++] = 'C';
        line2[len] = '\0';
    }
    showMessage(line1, line2);
}

static int formatFloat1(char* out, float val) {
    int n = (int)round(val * 10);
    bool neg = n < 0;
    if (neg) n = -n;
    char tmp[8];
    itoa(n / 10, tmp, 10);
    int idx = 0;
    if (neg) out[idx++] = '-';
    int l = strlen(tmp);
    memcpy(out + idx, tmp, l); idx += l;
    out[idx++] = '.';
    out[idx++] = (n % 10) + '0';
    out[idx] = '\0';
    return idx;
}

void displayCoordScreen() {
    char line1[17];
    char line2[17];
    int idx;
    if (useAbsoluteXYZ) {
        idx = 0;
        line1[idx++] = 'X';
        idx += formatFloat1(line1 + idx, printer.posX);
        line1[idx++] = ' ';
        line1[idx++] = 'Y';
        idx += formatFloat1(line1 + idx, printer.posY);
        line1[idx] = '\0';

        idx = 0;
        line2[idx++] = 'Z';
        idx += formatFloat1(line2 + idx, printer.posZ);
        line2[idx++] = ' ';
        line2[idx++] = 'E';
        idx += formatFloat1(line2 + idx, printer.posE);
        line2[idx] = '\0';
    } else {
        idx = 0;
        line1[idx++] = '\xCE';
        line1[idx++] = '\x94';
        line1[idx++] = 'X';
        idx += formatFloat1(line1 + idx, printer.nextX);
        line1[idx++] = ' ';
        line1[idx++] = '\xCE';
        line1[idx++] = '\x94';
        line1[idx++] = 'Y';
        idx += formatFloat1(line1 + idx, printer.nextY);
        line1[idx] = '\0';

        idx = 0;
        line2[idx++] = '\xCE';
        line2[idx++] = '\x94';
        line2[idx++] = 'Z';
        idx += formatFloat1(line2 + idx, printer.nextZ);
        line2[idx++] = ' ';
        line2[idx++] = '\xCE';
        line2[idx++] = '\x94';
        line2[idx++] = 'E';
        idx += formatFloat1(line2 + idx, printer.nextE);
        line2[idx] = '\0';
    }
    showMessage(line1, line2);

}

void displaySerialScreen() {
    char buf1[17], buf2[17];
    for (int i = 0; i < 16; i++) {
        buf1[i] = printer.currentCmd[i] ? printer.currentCmd[i] : ' ';
        buf2[i] = printer.currentCmd[i + 16] ? printer.currentCmd[i + 16] : ' ';
    }
    buf1[16] = '\0';
    buf2[16] = '\0';
    showMessage(buf1, buf2);
}


void displayIdleScreen(int animPos) {
    static const char msg[] = "Waiting job";
    static unsigned long lastScroll = 0;
    static int offset = 0;
    const unsigned long scrollInterval = 400;

    int len = strlen(msg);
    char line1[17];
    if (len <= 15) {
        memset(line1, ' ', 15);
        memcpy(line1, msg, len);
    } else {
        if (millis() - lastScroll > scrollInterval) {
            offset = (offset + 1) % (len + 1);
            lastScroll = millis();
        }
        for (int i = 0; i < 15; i++) {
            int idx = (offset + i) % (len + 1);
            line1[i] = (idx < len) ? msg[idx] : ' ';
        }
    }
    line1[15] = '\0';

    char line2[17];
    int t = (int)round(printer.currentTemp);
    int idx2 = 0;
    if (t < -9 || t > 999) {
        line2[idx2++] = '_';
        line2[idx2++] = 223;
        line2[idx2++] = 'C';
    } else {
        if (t < 0) {
            line2[idx2++] = '-';
            t = -t;
        }
        char buf[5];
        itoa(t, buf, 10);
        int l = strlen(buf);
        memcpy(line2 + idx2, buf, l); idx2 += l;
        line2[idx2++] = 223;
        line2[idx2++] = 'C';
    }
    while (idx2 < 14) line2[idx2++] = ' ';
    line2[idx2++] = '>'; line2[idx2++] = '>';
    line2[idx2] = '\0';
    static const char anim[] = "|/-\\";
    line1[15] = anim[animPos];
    showMessage(line1, line2);
}

void updateLCD() {
    if (displayFrozen) {
        if (millis() - freezeStartTime < freezeDuration) {
            return;
        }
        displayFrozen = false;
        memset(lastDisplayContent, 0, sizeof(lastDisplayContent));
    }
    if (printer.paused) {
        showMessage("** Paused **", "Press Button");
        return;
    }
    static int animPos = 0;
    static const char anim[] = "|/-\\";

    bool idle = (displayMode == 0 &&
                 printer.eTotal == -1 &&
                 millis() - lastPressTime >= idleSwitchDelay);
    if (idle) {
        displayIdleScreen(animPos);
    } else if (displayMode == 0) {
        displayProgressScreen();
    } else if (displayMode == 1) {
        displayCoordScreen();
    } else {
        displaySerialScreen();
    }

    bool moving = (millis() - printer.lastMoveTime) < 1000 && printer.movingAxis != ' ';
    lcd.setCursor(11, 1);
    lcd.print(printer.heaterOn ? 'H' : ' ');
    lcd.setCursor(12, 1);
    lcd.print(useAbsoluteXYZ ? "ABS" : "REL");
    lcd.setCursor(15, 1);
    if (moving) {
        lcd.print(printer.movingDir > 0 ? '>' : '<');
    } else {
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

    if (printer.paused) {
        if (justPressed()) {
            printer.paused = false;
            showMessage("Resuming", "");
            delay(300);
            memset(lastDisplayContent, 0, sizeof(lastDisplayContent));
        }
        prevState = state;
        return;
    }

    if (printer.eTotal == 0) {
        if (justPressed()) {
            printer.eTotal = -1;
            printer.progress = 0;
            printer.eStartSynced = false;
            memset(lastDisplayContent, 0, sizeof(lastDisplayContent)); // force LCD refresh
        }
        prevState = state;
        return;
    }


    if (justPressed()) {
        pressStartTime = now;
    }

    if (state && !isLongPress && now - pressStartTime > 50) {
        if (longPressed(3000)) {
            enterPauseMode();
            sendOk(F("Paused"));
            isLongPress = true;
        }
    }

    if (!state && prevState) {
        if (!isLongPress) {
            displayMode = (displayMode + 1) % 3;
            lastDisplaySwitch = now;
        }
        isLongPress = false;
        lastPressTime = now;
    }

    prevState = state;
}

void autoSwitchDisplay() {
    if (displayMode != 0 && printer.progress < 100 && printer.eStartSynced && printer.eTotal > 0) {
        unsigned long now = millis();
        if (now - lastPressTime >= autoSwitchDelay && now - lastDisplaySwitch >= autoSwitchDelay) {
            displayMode = 0;
            lastDisplaySwitch = now;
        }
    }
}

void enterPauseMode() {
    printer.paused = true;
    showMessage("** Paused **", "Press Button");
    memset(lastDisplayContent, 0, sizeof(lastDisplayContent));
}


void runTemperatureTask() {
    readTemperature();
    controlHeater();
}

void runInputTask() {
    checkButton();
}

void runDisplayTask() {
    autoSwitchDisplay();
    updateLCD();
}

void runGcodeTask() {
    if (!printer.paused) {
        processGcode();
    }
}




void setup() {
    setupInterrupts();
    initButton(buttonPin);

    pinMode(heaterPin, OUTPUT);
    pinMode(buzzerPin, OUTPUT);
    pinMode(motorEnablePin, OUTPUT);
    
    // Configure stepper driver pins
    pinMode(stepPinX, OUTPUT);
    pinMode(dirPinX, OUTPUT);
    pinMode(stepPinY, OUTPUT);
    pinMode(dirPinY, OUTPUT);
    pinMode(stepPinZ, OUTPUT);
    pinMode(dirPinZ, OUTPUT);
    pinMode(stepPinE, OUTPUT);
    pinMode(dirPinE, OUTPUT);

    digitalWrite(motorEnablePin, HIGH);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("System Ready");
    delay(1000);
    lcd.clear();
    lastDisplaySwitch = millis();

    Serial.begin(115200);
    resetPrinterState();
    loadSettingsFromEEPROM();
}

void loop() {
    unsigned long now = millis();
    if (now - lastLoopTime >= loopInterval) {
        lastLoopTime = now;
        runTemperatureTask();
        runInputTask();
        runDisplayTask();
        runGcodeTask();
    }
}

