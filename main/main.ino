//#define ENABLE_HOMING

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
#include <Bounce2.h>
#include <EEPROM.h>
#include "pins.h"
#include "gcode.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);
Bounce debouncer = Bounce();

const int tempPin = A0;

const int endstopX = A1;
const int endstopY = A2;
const int endstopZ = A3;

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
    digitalWrite(fanPin, LOW);
  }

  if (tempError && !tempErrorNotified) {
    beepErrorAlert();
    tempErrorNotified = true;
  }
}

void beepErrorAlert() {
  for (int i = 0; i < 5; i++) {
    tone(buzzerPin, 1000, 150);
    delay(200);
  }
  noTone(buzzerPin);
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

    if (currentTemp >= 50 && !fanStarted && !fanForced) {
      digitalWrite(fanPin, HIGH);
      fanStarted = true;
    }

    if (abs(currentTemp - setTemp) < 1.0) {
      if (!heatDoneBeeped && heatStableStart == 0) {
        heatStableStart = now;
      }
      if (!heatDoneBeeped && (now - heatStableStart >= stableHoldTime)) {
        tone(buzzerPin, 1000, 200); // 加熱完成簡單提示音
        heatDoneBeeped = true;
      }
    } else {
      heatStableStart = 0;
    }
  } else {
    analogWrite(heaterPin, 0);
    if (!fanForced) {
      digitalWrite(fanPin, LOW);
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
    char buf[17];
    snprintf(buf, sizeof(buf), "Progress: %3d%%", progress);
    showMessage("Status:", buf);
  }
}

void updateLCD() {
  static int animPos = 0;
  static const char anim[] = "|/-\\";

  if (displayMode == 0) {
    displayTempScreen();
  } else if (displayMode == 1) {
    displayCoordScreen();
    return;
  } else {
    displayStatusScreen();
  }

  lcd.setCursor(15, 1);
  lcd.print(anim[animPos]);
  animPos = (animPos + 1) % 4;
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
  debouncer.update();
  bool state = debouncer.read() == LOW;

  if (tempError) {
    clearTempError();
    return;
  }

  if (state && !isLongPress && millis() - lastPressTime > 50) {
    if (millis() - lastPressTime > 3000) {
      if (confirmStop) {
        if (millis() - confirmStartTime >= 3000) {
          forceStop();
          confirmStop = false;
        }
      } else {
        confirmStop = true;
        confirmStartTime = millis();
        showMessage("Confirm Stop?", "Hold 3s again");
      }
      isLongPress = true;
    }
  }

  if (!state) {
    if (confirmStop && millis() - confirmStartTime < 5000) {
      confirmStop = false;
      showMessage("Cancelled", "");
      delay(300);
    } else if (!isLongPress) {
      displayMode = (displayMode + 1) % 3;
    }
    isLongPress = false;
    lastPressTime = millis();
  }
}

void forceStop() {
  setTemp = 0;
  analogWrite(heaterPin, 0);
  digitalWrite(fanPin, LOW);
  showMessage("** Forced STOP **", "");
}

void playMario() {
  int melody[] = {262, 262, 0, 262, 0, 196, 262, 0, 0, 0, 294, 0, 330};
  int durations[] = {200, 200, 100, 200, 100, 400, 400, 100, 100, 100, 400, 100, 600};

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tune: Mario");
  lcd.setCursor(0, 1);

  for (int i = 0; i < 13; i++) {
    if (melody[i] == 0) {
      noTone(buzzerPin);
    } else {
      tone(buzzerPin, melody[i], durations[i]);
    }
    delay(durations[i] + 50);
    lcd.print((char)255); // 進度條
  }
  noTone(buzzerPin);
  delay(500);
  lcd.clear();
}

void moveAxis(int stepPin, int dirPin, long& pos, int target, int feedrate) {
  int distance = target - (useAbsolute ? 0 : pos);
  int steps = abs(distance);
  int dir = (distance >= 0) ? HIGH : LOW;
  digitalWrite(dirPin, dir);

  // E 軸防過擠限制
  if (&pos == &posE && distance > 0) {
    extern int eMaxSteps;
    if (pos + steps > eMaxSteps) {
      steps = eMaxSteps - pos;
      if (steps <= 0) return;
    }
  }

  float stepsPerMM = 80.0;  // ← 根據實際馬達/結構設定
  long delayPerStep = (long)(60000000.0 / (feedrate * stepsPerMM));
  delayPerStep = max(50L, delayPerStep);  // 最小保護

  for (int i = 0; i < steps; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(delayPerStep);
  }

  pos = useAbsolute ? target : pos + distance;
}


#ifdef ENABLE_HOMING
void homeAxis(int stepPin, int dirPin, int endstopPin, const char* label) {
  digitalWrite(dirPin, LOW);
  while (digitalRead(endstopPin) == HIGH) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(800);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(800);
  }
  Serial.print(label); Serial.println(" Homed");
}
#endif


void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  debouncer.attach(buttonPin);
  debouncer.interval(25);

  pinMode(heaterPin, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  delay(1000);
  lcd.clear();

  Serial.begin(9600);
  loadSettingsFromEEPROM();
}

void loop() {
  unsigned long now = millis();
  if (now - lastLoopTime >= loopInterval) {
    lastLoopTime = now;
    readTemperature();
    controlHeater();
    checkButton();
    updateLCD();
    processGcode();
  }
}

