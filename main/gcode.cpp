// gcode.cpp
#include "gcode.h"
#include "tunes.h"
#include "state.h"
#include <LiquidCrystal_I2C.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "config.h"

// Unified serial response helper
void sendOk(const String &msg = "") {
    Serial.print(F("ok"));
    if (msg.length()) {
        Serial.print(' ');
        Serial.print(msg);
    }
    Serial.println();
}

// 外部變數宣告
extern bool useAbsoluteXYZ;
extern bool useRelativeE;
extern float feedrateMultiplier;
extern float flowrateMultiplier;
extern int currentFeedrate;
extern const int stepPinX, dirPinX, stepPinY, dirPinY, stepPinZ, dirPinZ, stepPinE, dirPinE;
extern const int endstopX, endstopY, endstopZ;
extern const int motorEnablePin;
extern void playTune(int tune);
extern void saveSettingsToEEPROM();
extern void updateProgress();
extern float stepsPerMM_X, stepsPerMM_Y, stepsPerMM_Z, stepsPerMM_E;
extern LiquidCrystal_I2C lcd;
extern char lastDisplayContent[33];
extern void showMessage(const char*, const char*);

#ifdef SIMULATE_GCODE_INPUT
static const char *debugCommands[] = {
    "M104 S200",
    "M290 E100",
    "G90",
    "G92 X0 Y0 Z0 E0",
    "G1 X10 Y10 F800",
    "G1 E50 F600",
    "G1 X20 Y20 F800",
    "G1 E100 F600",
    "M105",
    "M400",
};
static const int debugCommandCount = sizeof(debugCommands) / sizeof(debugCommands[0]);
static int debugIndex = 0;
#endif

static String getGcodeInput() {
#ifdef SIMULATE_GCODE_INPUT
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

static void handleMoveCommand(const String &gcode, bool allowExtrude) {
    int fIndex = gcode.indexOf('F');
    if (fIndex != -1) {
        int fend = gcode.indexOf(' ', fIndex);
        String fStr = (fend != -1) ? gcode.substring(fIndex + 1, fend) : gcode.substring(fIndex + 1);
        int parsed = fStr.toInt();
        if (parsed > 0) currentFeedrate = parsed;
    }

    auto parseAxis = [&](char a, float &out)->bool {
        int idx = gcode.indexOf(a);
        if (idx == -1) return false;
        int end = gcode.indexOf(' ', idx);
        String valStr = (end != -1) ? gcode.substring(idx + 1, end) : gcode.substring(idx + 1);
        out = valStr.toFloat();
        return true;
    };

    float tx = 0, ty = 0, tz = 0, te = 0;
    bool hx = parseAxis('X', tx);
    bool hy = parseAxis('Y', ty);
    bool hz = parseAxis('Z', tz);
    bool he = allowExtrude ? parseAxis('E', te) : false;

    if (useAbsoluteXYZ) {
        if (!hx) tx = printer.posX;
        if (!hy) ty = printer.posY;
        if (!hz) tz = printer.posZ;
    }

    if (allowExtrude) {
        if (useRelativeE) {
            if (!he) te = 0;
        } else {
            if (!he) te = printer.posE;
        }
    } else {
        te = useRelativeE ? 0 : printer.posE;
    }

    float distX = useAbsoluteXYZ ? tx - printer.posX : tx;
    float distY = useAbsoluteXYZ ? ty - printer.posY : ty;
    float distZ = useAbsoluteXYZ ? tz - printer.posZ : tz;
    float distE = 0;
    if (allowExtrude) {
        distE = useRelativeE ? te : (useAbsoluteXYZ ? te - printer.posE : te);
        distE *= flowrateMultiplier;
    }

    float targetE = useRelativeE ? distE : (useAbsoluteXYZ ? printer.posE + distE : distE);

    printer.remStepX = lroundf(fabsf(distX * stepsPerMM_X));
    printer.remStepY = lroundf(fabsf(distY * stepsPerMM_Y));
    printer.remStepZ = lroundf(fabsf(distZ * stepsPerMM_Z));
    printer.remStepE = lroundf(fabsf(distE * stepsPerMM_E));
    printer.signX = (distX >= 0) ? 1 : -1;
    printer.signY = (distY >= 0) ? 1 : -1;
    printer.signZ = (distZ >= 0) ? 1 : -1;
    printer.signE = (distE >= 0) ? 1 : -1;

    printer.nextX = useAbsoluteXYZ ? tx : distX;
    printer.nextY = useAbsoluteXYZ ? ty : distY;
    printer.nextZ = useAbsoluteXYZ ? tz : distZ;
    if (allowExtrude) {
        printer.nextE = useRelativeE ? distE : (useAbsoluteXYZ ? targetE : distE);
    } else {
        printer.nextE = useRelativeE ? 0 : printer.posE;
    }
    printer.hasNextMove = true;

    moveAxes(tx, ty, tz, targetE, lroundf(currentFeedrate * feedrateMultiplier));

    printer.hasNextMove = false;
    printer.remStepX = printer.remStepY = printer.remStepZ = printer.remStepE = 0;

    String moveMsg = F("Move");
    if (hx) { moveMsg += " X"; moveMsg += printer.posX; }
    if (hy) { moveMsg += " Y"; moveMsg += printer.posY; }
    if (hz) { moveMsg += " Z"; moveMsg += printer.posZ; }
    if (allowExtrude && (he || distE != 0)) { moveMsg += " E"; moveMsg += printer.posE; }
    sendOk(moveMsg);
}

void processGcode() {
    if (printer.waitingForHeat) {
        if (fabs(printer.currentTemp - printer.setTemp) < 1.0 && printer.heatDoneBeeped) {
            printer.waitingForHeat = false;
            sendOk(F("Target temp reached"));
        }
        return;
    }
    String gcode = getGcodeInput();
    if (gcode.length()) {
        gcode.trim();

        if (gcode.startsWith("G90")) {          // G90 - 進入絕對座標模式

            useAbsoluteXYZ = true;
            useRelativeE = false;
            sendOk(F("G90 Absolute mode"));
        } else if (gcode.startsWith("G91")) {   // G91 - 進入相對座標模式
            useAbsoluteXYZ = false;
            useRelativeE = true;
            sendOk(F("G91 Relative mode"));
        } else if (gcode.startsWith("M82")) {   // M82 - Extruder absolute mode
            useRelativeE = false;
            sendOk(F("M82 E absolute"));
        } else if (gcode.startsWith("M83")) {   // M83 - Extruder relative mode
            useRelativeE = true;
            sendOk(F("M83 E relative"));
        } else if (gcode.startsWith("G92")) {   // G92 - 手動設定目前座標（包含 E 也會同步進度 eStart）
            if (gcode.indexOf('X') != -1) printer.posX = gcode.substring(gcode.indexOf('X') + 1).toFloat();
            if (gcode.indexOf('Y') != -1) printer.posY = gcode.substring(gcode.indexOf('Y') + 1).toFloat();
            if (gcode.indexOf('Z') != -1) printer.posZ = gcode.substring(gcode.indexOf('Z') + 1).toFloat();
            if (gcode.indexOf('E') != -1) {
                printer.posE = gcode.substring(gcode.indexOf('E') + 1).toFloat();
                printer.eStart = printer.posE;  // 同步進度起點，避免重設座標後估算錯誤
                sendOk(F("G92 E origin reset"));
            } else {
                sendOk(F("G92 Origin set"));
            }
        } else if (gcode.startsWith("M104")) {  // M104 Snnn - 設定加熱目標溫度（不等待）
            int sIndex = gcode.indexOf('S');
            if (sIndex != -1) {
                float target = gcode.substring(sIndex + 1).toFloat();
                if (!isnan(target)) {
                    printer.setTemp = target;
                    printer.heatDoneBeeped = false;
                    sendOk(String("Set temperature to ") + printer.setTemp);
                }
            }
        } else if (gcode.startsWith("M109")) {  // M109 Snnn - 設定溫度並等待
            int sIndex = gcode.indexOf('S');
            if (sIndex != -1) {
                float target = gcode.substring(sIndex + 1).toFloat();
                if (!isnan(target)) {
                    printer.setTemp = target;
                    printer.heatDoneBeeped = false;
                    printer.waitingForHeat = true;
                    sendOk(String("Heating to ") + printer.setTemp);
                }
            }
        } else if (gcode.startsWith("M105")) {  // M105 - 回報目前溫度
            String msg = String("T:") + String(printer.currentTemp, 1) + " /" + String(printer.setTemp, 1) + " B:0.0 /0.0";
            sendOk(msg);
        } else if (gcode.startsWith("M114")) {  // M114 - 回報目前座標
            String msg = String("X:") + printer.posX + " Y:" + printer.posY + " Z:" + printer.posZ + " E:" + printer.posE;
            sendOk(msg);
        } else if (gcode.startsWith("M0")) {    // M0 - 暫停等待按鈕
            enterPauseMode();
            sendOk(F("Paused"));
        } else if (gcode.startsWith("G4")) {    // G4 Snn or Pnn - 延遲
            long ms = 0;
            int sIndex = gcode.indexOf('S');
            int pIndex = gcode.indexOf('P');
            if (sIndex != -1) {
                ms = (long)(gcode.substring(sIndex + 1).toFloat() * 1000.0);
            } else if (pIndex != -1) {
                ms = gcode.substring(pIndex + 1).toInt();
            }
            if (ms > 0) delay(ms);
            sendOk(String("Dwell ") + ms + F(" ms"));
        } else if (gcode.startsWith("M301")) {  // M301 Pn In Dn - 設定 PID 控制參數
            int pIndex = gcode.indexOf('P');
            int iIndex = gcode.indexOf('I');
            int dIndex = gcode.indexOf('D');

            float temp;
            if (pIndex != -1) {
                temp = gcode.substring(pIndex + 1, (iIndex != -1 ? iIndex : gcode.length())).toFloat();
                if (!isnan(temp)) printer.Kp = temp;
            }
            if (iIndex != -1) {
                temp = gcode.substring(iIndex + 1, (dIndex != -1 ? dIndex : gcode.length())).toFloat();
                if (!isnan(temp)) printer.Ki = temp;
            }
            if (dIndex != -1) {
                temp = gcode.substring(dIndex + 1).toFloat();
                if (!isnan(temp)) printer.Kd = temp;
            }

            saveSettingsToEEPROM();
            String pidMsg = String("Kp:") + printer.Kp + " Ki:" + printer.Ki + " Kd:" + printer.Kd;
            sendOk(pidMsg);
        } else if (gcode.startsWith("M400")) {  // M400 - 播放選定音樂，列印完成提示
#ifndef NO_TUNES
            playTune(DEFAULT_TUNE);
#endif
            sendOk(F("Print Complete"));
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
            sendOk(F("Steps per mm updated"));
        } else if (gcode.startsWith("M290")) { // M290 En - 設定進度總量
            int eIndex = gcode.indexOf('E');
            if (eIndex != -1) {
                long val = gcode.substring(eIndex + 1).toInt();
                if (val > 0) {
                    printer.eTotal = val;
                    printer.eStart = printer.posE;
                    printer.eStartSynced = true;
                    printer.progress = 0;
                    sendOk(String("eTotal set to ") + printer.eTotal);
                }
            }
        } else if (gcode.startsWith("M220")) { // M220 Snnn - 調整移動速度倍率
            int sIndex = gcode.indexOf('S');
            if (sIndex != -1) {
                float val = gcode.substring(sIndex + 1).toFloat();
                if (!isnan(val)) {
                    feedrateMultiplier = val / 100.0f;
                    sendOk(String("Feedrate scale ") + val + "%");
                }
            }
        } else if (gcode.startsWith("M221")) { // M221 Snnn - 調整擠出倍率
            int sIndex = gcode.indexOf('S');
            if (sIndex != -1) {
                float val = gcode.substring(sIndex + 1).toFloat();
                if (!isnan(val)) {
                    flowrateMultiplier = val / 100.0f;
                    sendOk(String("Flow scale ") + val + "%");
                }
            }
        } else if (gcode.startsWith("M500")) {  // M500 - 儲存設定到 EEPROM
            saveSettingsToEEPROM();
            sendOk(F("Settings saved"));
        } else if (gcode.startsWith("M503")) {  // M503 - 印出目前參數
            sendOk(F("Current settings"));
            Serial.print(F("Kp = ")); Serial.println(printer.Kp);
            Serial.print(F("Ki = ")); Serial.println(printer.Ki);
            Serial.print(F("Kd = ")); Serial.println(printer.Kd);
            Serial.print(F("Steps/mm X:")); Serial.println(stepsPerMM_X);
            Serial.print(F("Steps/mm Y:")); Serial.println(stepsPerMM_Y);
            Serial.print(F("Steps/mm Z:")); Serial.println(stepsPerMM_Z);
            Serial.print(F("Steps/mm E:")); Serial.println(stepsPerMM_E);
        } else if (gcode.startsWith("M84")) {  // M84 - 馬達釋放
            digitalWrite(motorEnablePin, HIGH);
            sendOk(F("Motors disabled"));
        } else if (gcode.startsWith("G0")) {    // G0 - 快速移動，不擠料
            handleMoveCommand(gcode, false);
        } else if (gcode.startsWith("G1")) {    // G1 - 執行軸移動
            handleMoveCommand(gcode, true);
        } else if (gcode.startsWith("G28")) {   // G28 - 執行回原點並可指定軸
            bool hx = gcode.indexOf('X') != -1;
            bool hy = gcode.indexOf('Y') != -1;
            bool hz = gcode.indexOf('Z') != -1;
            if (!hx && !hy && !hz) {
                hx = hy = hz = true; // 預設全部軸
            }
            if (hx) {
                homeAxis(stepPinX, dirPinX, endstopX, "X");
                printer.posX = 0.0f;
            }
            if (hy) {
                homeAxis(stepPinY, dirPinY, endstopY, "Y");
                printer.posY = 0.0f;
            }
            if (hz) {
                homeAxis(stepPinZ, dirPinZ, endstopZ, "Z");
                printer.posZ = 0.0f;
            }
            sendOk(F("G28 Done"));
        } else {  // 其他未知指令
            Serial.print(F("ERR: Unknown cmd "));
            Serial.println(gcode);
        }
    }
}


