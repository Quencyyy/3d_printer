// gcode.cpp
#include "gcode.h"
#include "tunes.h"
#include <LiquidCrystal_I2C.h>

// 外部變數宣告
extern bool useAbsolute;
extern long posX, posY, posZ, posE;
extern long eStart, eTotal;
extern float setTemp, currentTemp;
extern bool fanStarted, fanForced, heatDoneBeeped;
extern float Kp, Ki, Kd;
extern int currentFeedrate;
extern bool eStartSynced;
extern const int fanPin;
extern bool fanOn;
extern bool heaterOn;
extern char movingAxis;
extern int movingDir;
extern unsigned long lastMoveTime;
extern const int stepPinX, dirPinX, stepPinY, dirPinY, stepPinZ, dirPinZ, stepPinE, dirPinE;
extern const int endstopX, endstopY, endstopZ;
extern int currentTune;
extern void playTune(int tune);
extern void saveSettingsToEEPROM();
extern void updateProgress();
extern float stepsPerMM_X, stepsPerMM_Y, stepsPerMM_Z, stepsPerMM_E;
extern LiquidCrystal_I2C lcd;
extern String lastDisplayContent;

static void displayM503LCD() {
    for (int t = 5; t > 0; --t) {
        char line1[17];
        char line2[17];
        snprintf(line1, sizeof(line1), "P%.0f I%.0f D%.0f %d", Kp, Ki, Kd, t);
        snprintf(line2, sizeof(line2), "X%.0f Y%.0f Z%.0f E%.0f", stepsPerMM_X,
                 stepsPerMM_Y, stepsPerMM_Z, stepsPerMM_E);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(line1);
        lcd.setCursor(0, 1);
        lcd.print(line2);
        delay(1000);
    }
    lcd.clear();
    lastDisplayContent = "";
}

#ifdef DEBUG_INPUT
static const char *debugCommands[] = {
    "G90",
    "G1 X10 Y10 F800",
    "M105",
    "M106",
    "M107",
    "M400",
    "M401 S1"
};
static const int debugCommandCount = sizeof(debugCommands) / sizeof(debugCommands[0]);
static int debugIndex = 0;
#endif

static String getGcodeInput() {
#ifdef DEBUG_INPUT
    if (debugIndex < debugCommandCount) {
        String cmd(debugCommands[debugIndex++]);
        Serial.print(F("DBG> "));
        Serial.println(cmd);
        return cmd;
    }
#endif
    if (Serial.available()) {
        return Serial.readStringUntil('\n');
    }
    return String();
}

void processGcode() {
    String gcode = getGcodeInput();
    if (gcode.length()) {
        gcode.trim();

        if (gcode.startsWith("G90")) {          // G90 - 進入絕對座標模式
            
            useAbsolute = true;
            Serial.println("[G90] Absolute mode");
        } else if (gcode.startsWith("G91")) {   // G91 - 進入相對座標模式
            useAbsolute = false;
            Serial.println("[G91] Relative mode");
        } else if (gcode.startsWith("G92")) {   // G92 - 手動設定目前座標（包含 E 也會同步進度 eStart）
            if (gcode.indexOf('X') != -1) posX = gcode.substring(gcode.indexOf('X') + 1).toInt();
            if (gcode.indexOf('Y') != -1) posY = gcode.substring(gcode.indexOf('Y') + 1).toInt();
            if (gcode.indexOf('Z') != -1) posZ = gcode.substring(gcode.indexOf('Z') + 1).toInt();
            if (gcode.indexOf('E') != -1) {
                posE = gcode.substring(gcode.indexOf('E') + 1).toInt();
                eStart = posE;  // 同步進度起點，避免重設座標後估算錯誤
                Serial.println("[G92] E origin reset.");
            } else {
                Serial.println("[G92] Origin set.");
            }
        } else if (gcode.startsWith("M104")) {  // M104 Snnn - 設定加熱目標溫度（不等待）
            int sIndex = gcode.indexOf('S');
            if (sIndex != -1) {
                float target = gcode.substring(sIndex + 1).toFloat();
                if (!isnan(target)) {
                    setTemp = target;
                    heatDoneBeeped = false;
                    Serial.print("Set temperature to ");
                    Serial.println(setTemp);
                }
            }
        } else if (gcode.startsWith("M105")) {  // M105 - 回報目前溫度
            Serial.print("T:");
            Serial.println(currentTemp);
        } else if (gcode.startsWith("M106")) {  // M106 - 強制開風扇
            fanForced = true;
            digitalWrite(fanPin, HIGH);
            fanOn = true;
            Serial.println("Fan ON");
        } else if (gcode.startsWith("M107")) {  // M107 - 關閉風扇（取消強制風扇）
            fanForced = false;
            digitalWrite(fanPin, LOW);
            fanOn = false;
            fanStarted = false;
            Serial.println("Fan OFF");
        } else if (gcode.startsWith("M301")) {  // M301 Pn In Dn - 設定 PID 控制參數
            int pIndex = gcode.indexOf('P');
            int iIndex = gcode.indexOf('I');
            int dIndex = gcode.indexOf('D');

            float temp;
            if (pIndex != -1) {
                temp = gcode.substring(pIndex + 1, (iIndex != -1 ? iIndex : gcode.length())).toFloat();
                if (!isnan(temp)) Kp = temp;
            }
            if (iIndex != -1) {
                temp = gcode.substring(iIndex + 1, (dIndex != -1 ? dIndex : gcode.length())).toFloat();
                if (!isnan(temp)) Ki = temp;
            }
            if (dIndex != -1) {
                temp = gcode.substring(dIndex + 1).toFloat();
                if (!isnan(temp)) Kd = temp;
            }

            saveSettingsToEEPROM();
            Serial.println("[M301] PID updated:");
            Serial.print("Kp = "); Serial.println(Kp);
            Serial.print("Ki = "); Serial.println(Ki);
            Serial.print("Kd = "); Serial.println(Kd);
        } else if (gcode.startsWith("M400")) {  // M400 - 播放選定音樂，列印完成提示
#ifdef ENABLE_BUZZER
            playTune(currentTune);
#endif
            Serial.println("[M400] Print Complete");
        } else if (gcode.startsWith("M401")) {  // M401 Sn - 設定列印完成音樂
            int sIndex = gcode.indexOf('S');
            if (sIndex != -1) {
                int val = gcode.substring(sIndex + 1).toInt();
                if (val >= 0 && val < TUNE_COUNT) {
                    currentTune = val;
                    Serial.print("[M401] Tune set to ");
                    Serial.println(val);
                } else {
                    Serial.println("[M401] Invalid tune");
                }
            } else {
                Serial.print("[M401] Current tune: ");
                Serial.println(currentTune);
            }
        } else if (gcode.startsWith("M92")) {   // M92 - 設定各軸 steps/mm
            int idx;
            float val;

            idx = gcode.indexOf('X');
            if (idx != -1) {
                int end = gcode.indexOf(' ', idx);
                val = gcode.substring(idx + 1, end != -1 ? end : gcode.length()).toFloat();
                if (!isnan(val)) stepsPerMM_X = val;
            }
            idx = gcode.indexOf('Y');
            if (idx != -1) {
                int end = gcode.indexOf(' ', idx);
                val = gcode.substring(idx + 1, end != -1 ? end : gcode.length()).toFloat();
                if (!isnan(val)) stepsPerMM_Y = val;
            }
            idx = gcode.indexOf('Z');
            if (idx != -1) {
                int end = gcode.indexOf(' ', idx);
                val = gcode.substring(idx + 1, end != -1 ? end : gcode.length()).toFloat();
                if (!isnan(val)) stepsPerMM_Z = val;
            }
            idx = gcode.indexOf('E');
            if (idx != -1) {
                int end = gcode.indexOf(' ', idx);
                val = gcode.substring(idx + 1, end != -1 ? end : gcode.length()).toFloat();
                if (!isnan(val)) stepsPerMM_E = val;
            }
            Serial.println("[M92] Steps per mm updated");
        } else if (gcode.startsWith("M500")) {  // M500 - 儲存設定到 EEPROM
            saveSettingsToEEPROM();
            Serial.println("[M500] Settings saved");
        } else if (gcode.startsWith("M503")) {  // M503 - 印出目前參數
            Serial.println("[M503] Current settings:");
            Serial.print("Kp = "); Serial.println(Kp);
            Serial.print("Ki = "); Serial.println(Ki);
            Serial.print("Kd = "); Serial.println(Kd);
            Serial.print("Steps/mm X:"); Serial.println(stepsPerMM_X);
            Serial.print("Steps/mm Y:"); Serial.println(stepsPerMM_Y);
            Serial.print("Steps/mm Z:"); Serial.println(stepsPerMM_Z);
            Serial.print("Steps/mm E:"); Serial.println(stepsPerMM_E);
            displayM503LCD();
        } else if (gcode.startsWith("G1")) {    // G1 Xn Yn Zn En - 執行軸移動
                int fIndex = gcode.indexOf('F');
                if (fIndex != -1) {
                    int fend = gcode.indexOf(' ', fIndex);
                    String fStr = (fend != -1) ? gcode.substring(fIndex + 1, fend) : gcode.substring(fIndex + 1);
                    int parsed = fStr.toInt();
                    if (parsed > 0)
                        currentFeedrate = parsed;
                }
                handleG1Axis('X', stepPinX, dirPinX, posX, gcode);
                handleG1Axis('Y', stepPinY, dirPinY, posY, gcode);
                handleG1Axis('Z', stepPinZ, dirPinZ, posZ, gcode);
                handleG1Axis('E', stepPinE, dirPinE, posE, gcode);
        } else if (gcode.startsWith("G28")) {   // G28 - 執行回原點（需開啟 ENABLE_HOMING）
#ifdef ENABLE_HOMING
            homeAxis(stepPinX, dirPinX, endstopX, "X");
            homeAxis(stepPinY, dirPinY, endstopY, "Y");
            homeAxis(stepPinZ, dirPinZ, endstopZ, "Z");
#else
            Serial.println("[Homing disabled] Please home manually.");
#endif
        } else {  // 其他未知指令
            Serial.print("Unknown command: [");
            Serial.print(gcode);
            Serial.println("]");
        }
    }
}

void handleG1Axis(char axis, int stepPin, int dirPin, long& pos, String& gcode) {
    int idx = gcode.indexOf(axis);
    if (idx != -1) {
        int end = gcode.indexOf(' ', idx);
        String valStr = (end != -1) ? gcode.substring(idx + 1, end) : gcode.substring(idx + 1);
        int val = valStr.toInt();

        // E 軸同步判斷與進度更新
        if (&pos == &posE) {
            if (!eStartSynced) {
                eStart = posE;
                eStartSynced = true;
            }
            updateProgress();
        }

        // 設定移動方向供顯示使用
        int distance = useAbsolute ? val - pos : val;
        movingAxis = axis;
        movingDir = (distance >= 0) ? 1 : -1;
        lastMoveTime = millis();

        // 呼叫含速度的移動
        moveAxis(stepPin, dirPin, pos, val, currentFeedrate, axis);

        Serial.print("Move "); Serial.print(axis); Serial.print(" to ");
        Serial.print(useAbsolute ? val : pos);
        Serial.println();
    }
}

