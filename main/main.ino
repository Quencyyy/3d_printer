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
#include "gcode.h"
#include "tunes.h"
#include "test_modes.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int tempPin = A0;


float setTemp = 0.0;
float Kp = 20, Ki = 1, Kd = 50;
float integral = 0, previousError = 0;
int currentFeedrate = 1000;  // 預設速度 mm/min
unsigned long lastTime = 0;

bool fanStarted = false;
bool fanForced = false;
bool heatDoneBeeped = false;
bool tempError = false; // 熱敏電阻錯誤旗標
bool tempErrorNotified = false; // 避免重複響鈴
float currentTemp = 0.0;
unsigned long heatStableStart = 0;
const unsigned long stableHoldTime = 3000;

long posX = 0, posY = 0, posZ = 0, posE = 0;
bool useAbsolute = true;

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

// 進度估算變數
long eStart = 0;
long eTotal = 1000;
int progress = 0;

// 狀態與動作指示
bool fanOn = false;
bool heaterOn = false;
char movingAxis = ' ';
int movingDir = 0; // 1:正向 -1:反向
unsigned long lastMoveTime = 0;

void saveSettingsToEEPROM() {
    EEPROM.put(0, Kp);
    EEPROM.put(4, Ki);
    EEPROM.put(8, Kd);
    EEPROM.put(12, setTemp);
}

void loadSettingsFromEEPROM() {
    EEPROM.get(0, Kp);
    EEPROM.get(4, Ki);
    EEPROM.get(8, Kd);
    EEPROM.get(12, setTemp);
}

void readTemperature() {
    int raw = analogRead(tempPin);
    float voltage = raw * 5.0 / 1023.0;
    float resistance = (5.0 - voltage) * 10000.0 / voltage;
    currentTemp = 1.0 / (log(resistance / 10000.0) / 3950.0 + 1.0 / 298.15) - 273.15;

    // 錯誤檢查與恢復條件
    if (currentTemp < -10 || currentTemp > 300) {
        tempError = true;
        tempErrorNotified = false;
        setTemp = 0;
        analogWrite(heaterPin, 0);
        heaterOn = false;
        digitalWrite(fanPin, LOW);
        fanOn = false;
    }

    if (tempError && !tempErrorNotified) {
#ifdef ENABLE_BUZZER
        beepErrorAlert();
#endif
        tempErrorNotified = true;
    }
}

void beepErrorAlert() {
#ifdef ENABLE_BUZZER
    for (int i = 0; i < 5; i++) {
        tone(buzzerPin, 1000, 150);
        delay(200);
    }
    noTone(buzzerPin);
#endif
}

void clearTempError() {
    if (currentTemp > -10 && currentTemp < 300) {
        tempError = false;
        tempErrorNotified = false;
        showMessage("Sensor OK", "System Normal");
        delay(500);
        lastDisplayContent = "";
    }
}

void controlHeater() {
    if (setTemp > 0.0) {
        unsigned long now = millis();
        float elapsed = (now - lastTime) / 1000.0;
        lastTime = now;

        float error = setTemp - currentTemp;
        integral += error * elapsed;
        float derivative = (error - previousError) / elapsed;
        previousError = error;

        float output = Kp * error + Ki * integral + Kd * derivative;
        output = constrain(output, 0, 255);
        analogWrite(heaterPin, (int)output);
        heaterOn = output > 0;

        if (currentTemp >= 50 && !fanStarted && !fanForced) {
            digitalWrite(fanPin, HIGH);
            fanOn = true;
            fanStarted = true;
        }

        if (abs(currentTemp - setTemp) < 1.0) {
            if (!heatDoneBeeped && heatStableStart == 0) {
                heatStableStart = now;
            }
            if (!heatDoneBeeped && (now - heatStableStart >= stableHoldTime)) {
#ifdef ENABLE_BUZZER
                tone(buzzerPin, 1000, 200); // 加熱完成簡單提示音
#endif
                heatDoneBeeped = true;
            }
        } else {
            heatStableStart = 0;
        }
    } else {
        analogWrite(heaterPin, 0);
        heaterOn = false;
        if (!fanForced) {
            digitalWrite(fanPin, LOW);
            fanOn = false;
            fanStarted = false;
        }
        heatDoneBeeped = false;
        heatStableStart = 0;
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
    snprintf(buf, sizeof(buf), "T:%.1f%cC Set:%.0f", currentTemp, 223, setTemp);
    showMessage(buf, "");
}

void displayCoordScreen() {
    char buf1[17], buf2[17];
    snprintf(buf1, sizeof(buf1), "X%ld Y%ld", posX, posY);
    snprintf(buf2, sizeof(buf2), "Z%ld E%ld", posZ, posE);
    showMessage(buf1, buf2);
}

void displayStatusScreen() {
    if (tempError) {
        showMessage("Sensor ERROR!", "Check & Press Btn");
    } else {
        char bar[11];
        int filled = constrain(progress / 10, 0, 10);
        for (int i = 0; i < 10; i++) {
            bar[i] = (i < filled) ? '#' : '-';
        }
        bar[10] = '\0';

        char line1[17];
        if (progress >= 100)
            snprintf(line1, sizeof(line1), "[%s]%3d%%", bar, progress);
        else
            snprintf(line1, sizeof(line1), "[%s] %3d%%", bar, progress);
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

    bool moving = (millis() - lastMoveTime) < 1000 && movingAxis != ' ';
    lcd.setCursor(12, 1);
    lcd.print(fanOn ? 'F' : ' ');
    lcd.print(heaterOn ? 'H' : ' ');
    if (moving) {
        lcd.print(movingAxis);
        lcd.print(movingDir > 0 ? '>' : '<');
    } else {
        lcd.print(' ');
        lcd.print(anim[animPos]);
        animPos = (animPos + 1) % 4;
    }
}

void updateProgress() { //根據公式 (posE - eStart) * 100 / eTotal 計算百分比
    if (eTotal > 0) {
        long delta = posE - eStart;
        if (delta > 0 && delta <= eTotal) {
            // 忽略倒退（如 E retraction），只有正擠出才計入進度
            progress = (int)(delta * 100L / eTotal);
        } else if (delta > eTotal) {
            progress = 100;
        }
    }
}

void checkButton() {
    updateButton();
    bool state = isPressed();
    static bool prevState = false;
    static unsigned long pressStartTime = 0;
    unsigned long now = millis();

    if (tempError) {
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
    if (displayMode != 2 && progress < 100 && eStartSynced) {
        unsigned long now = millis();
        if (now - lastDisplaySwitch >= autoSwitchDelay) {
            displayMode = 2;
            lastDisplaySwitch = now;
        }
    }
}

void forceStop() {
    setTemp = 0;
    analogWrite(heaterPin, 0);
    heaterOn = false;
    digitalWrite(fanPin, LOW);
    fanOn = false;
    showMessage("** Forced STOP **", "");
}


void moveAxis(int stepPin, int dirPin, long& pos, int target, int feedrate) {
    int distance = useAbsolute ? target - pos : target;
    int steps = abs(distance);
    int dir = (distance >= 0) ? HIGH : LOW;
    digitalWrite(dirPin, dir);
    digitalWrite(motorEnablePin, HIGH);

    // E 軸防過擠限制
    if (&pos == &posE && distance > 0) {
        extern int eMaxSteps;
        if (pos + steps > eMaxSteps) {
            steps = eMaxSteps - pos;
            if (steps <= 0) return;
        }
    }

    float stepsPerMM = 80.0;  // ← 根據實際馬達/結構設定
    long minDelay = (long)(60000000.0 / (feedrate * stepsPerMM));
    minDelay = max(50L, minDelay);  // 最小保護

    // 簡易加速度控制參數
    const int ACCEL_STEPS = 50;
    long startDelay = minDelay * 2;  // 起始較慢，逐步加速
    int rampSteps = min(steps / 2, ACCEL_STEPS);
    long delayDelta = rampSteps > 0 ? (startDelay - minDelay) / rampSteps : 0;
    long currentDelay = startDelay;

    for (int i = 0; i < steps; i++) {
        digitalWrite(stepPin, HIGH);
        delayMicroseconds(5);
        digitalWrite(stepPin, LOW);
        delayMicroseconds(currentDelay);

        if (rampSteps > 0) {
            if (i < rampSteps) {
                // 加速區間
                currentDelay = max(minDelay, currentDelay - delayDelta);
            } else if (i >= steps - rampSteps) {
                // 減速區間
                currentDelay = min(startDelay, currentDelay + delayDelta);
            }
        }
    }
    digitalWrite(motorEnablePin, LOW);

    pos = useAbsolute ? target : pos + target;
}


#ifdef ENABLE_HOMING
void homeAxis(int stepPin, int dirPin, int endstopPin, const char* label) {
    digitalWrite(motorEnablePin, HIGH);
    digitalWrite(dirPin, LOW);
    while (digitalRead(endstopPin) == HIGH) {
        digitalWrite(stepPin, HIGH);
        delayMicroseconds(800);
        digitalWrite(stepPin, LOW);
        delayMicroseconds(800);
    }
    digitalWrite(motorEnablePin, LOW);
    Serial.print(label); Serial.println(" Homed");
}
#endif


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

