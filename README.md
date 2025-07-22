# Minimal 3D Printer Firmware (G-code + LCD + Button Control)

本專案是一個陽春版 3D 印表機韌體，支援 G-code 解析、LCD 顯示、單鍵控制、PID 控溫、列印進度估算與 EEPROM 儲存。

## 目錄
- [支援功能](#支援功能)
- [G-code 指令說明](#g-code-指令說明)
- [實體按鈕操作說明](#實體按鈕操作說明)
- [LCD 顯示模式](#lcd-顯示模式)
- [系統參數](#系統參數)
- [編譯需求](#編譯需求)
- [檔案結構](#檔案結構)
- [STL 轉 G-code 標準操作流程（SOP）](#stl-轉-g-code-標準操作流程sop)
- [最小測試 G-code 範例](#最小測試-g-code-範例)
- [聯絡作者](#聯絡作者)

---

## 支援功能

- G-code 指令解析
- PID 控溫（M301 可調）
- 感測器錯誤保護與蜂鳴器提示
- LCD 三模式顯示 + 動畫（含進度條）
- 單鍵控制（短按/長按/雙擊）、強制停止、錯誤重置
- 列印進度推估（根據 E 軸）
- `M290` 指令設定進度總量
- EEPROM 參數儲存
- Watchdog 定時器提高系統可靠性
- 馬達移動支援簡易加速/減速

---

## G-code 指令說明

| 指令              | 功能描述                                        | 範例                             |
|-------------------|-------------------------------------------------|----------------------------------|
| `G90`             | 切換為絕對座標模式（預設）                     | `G90`                           |
| `G91`             | 切換為相對座標模式                             | `G91`                           |
| `G92`             | 設定目前座標（支援 X/Y/Z/E），E 會同步進度起點 | `G92 X0 Y0 Z0 E0`               |
| `G1`              | 移動軸位置（支援 X/Y/Z/E 及 F 速度）            | `G1 X10 Y10 Z5 E100 F1200`       |
| `G28`             | 回原點                                         | `G28`                           |
| `M104 Snnn`       | 設定目標溫度（不等待）                          | `M104 S200`                     |
| `M105`            | 回報目前溫度                                   | `M105`                          |
| `M301 Pn In Dn`   | 設定 PID 控溫參數並儲存至 EEPROM                | `M301 P20.0 I1.5 D60.0`         |
| `M400`            | 播放設定的音樂提示列印完成                      | `M400`                          |
| `M401 Sn`         | 選擇列印完成音樂（0~3）                         | `M401 S1`                       |
| `M92 Xn Yn Zn En` | 設定各軸每毫米步數（steps/mm）                  | `M92 X25 Y25 Z25 E25`       |
| `M290 En`         | 設定列印進度總量（E 軸長度）                    | `M290 E1200`                    |
| `M500`            | 將目前設定存入 EEPROM                           | `M500`                          |
| `M503`            | 列印目前 PID 與 steps/mm 等參數                 | `M503`                          |

可用音樂編號：0=Mario、1=Canon、2=StarWars、3=Tetris

若要使用音樂(蜂鳴器)功能，需將 `BUZZER_PIN` 腳位接出連接蜂鳴器，預設為 **D9**，
若未接線則不會發聲。

---

## 實體按鈕操作說明

| 操作方式           | 功能描述                                 |
|--------------------|------------------------------------------|
| **短按一次**       | 切換 LCD 顯示模式（三種畫面循環）       |
| **長按 3 秒**      | 進入強制停止確認畫面                     |
| **再次長按 3 秒**  | 確認強制停止，關閉加熱器                 |
| **錯誤畫面短按**   | 清除錯誤狀態（如感測器異常）             |

---

## LCD 顯示模式

| 模式名稱       | 顯示內容                                    |
|----------------|---------------------------------------------|
| 溫度顯示模式   | `T:200.0°C Set:220`                         |
| 座標顯示模式   | `X100 Y100` / `Z10 E500`                    |
| 狀態/進度顯示  | `[#####-----] 50%` 或 `Sensor ERROR!` 警示  |

- 每次短按按鈕將在上述三種模式間切換
- 右下角持續顯示動畫符號（| / - \）作為系統運作提示

---

## 系統參數

- 預設 `eTotal = -1`（未設定時不顯示進度，可用 `M290` 設定總量）
- E 軸有最大推擠保護：5000 步
- 控溫使用 PID 控制（`Kp`, `Ki`, `Kd` 可調）
- 預設加速步數 `ACCEL_STEPS = 50`

---

## 編譯需求

- Arduino IDE
- 支援板子：UNO / Nano / Mega 等
- 使用以下函式庫：
  - `LiquidCrystal_I2C`
  - `Bounce2`
  - `EEPROM`

---

## 檔案結構

| 檔案         | 說明                      |
|--------------|---------------------------|
| `main.ino`   | 主程式邏輯                |
| `gcode.cpp`  | G-code 指令解析模組       |
| `gcode.h`    | G-code 標頭檔              |

---

## STL 轉 G-code 標準操作流程（SOP）

### Step 1：安裝與啟動

1. 下載並安裝 [PrusaSlicer](https://www.prusa3d.com/page/prusaslicer_424/)
2. 啟動後選擇「自訂印表機」設定模式

### Step 2：設定你的機器

- **Printer Type**：選「自訂 FFF」
- **Print Bed Shape**：依實際範圍設定，如 `X=50 Y=50 Z=50`
- **Nozzle diameter**：預設 `0.4 mm`
- **Firmware Flavor**：`Marlin`

建議加入下列 G-code：

```gcode
; Start G-code
G90 ; Absolute mode
G92 E0 ; Reset extruder
M104 S200 ; Set nozzle temp
M105 ; Get current temp

; End G-code
M104 S0 ; Turn off heater
M400 ; Play finish tune
```

### Step 3：匯入 STL

1. 點擊 **Add** 匯入 `.stl` 模型
2. 依需要調整尺寸、位置與旋轉

### Step 4：切片設定

- 選擇列印品質（`Draft` 或 `Fine`）
- 選擇材質（如 PLA）
- 設定速度、支撐材與填充率

初學建議：

```
Layer height = 0.2mm
Infill = 15%
Print speed = 40mm/s
```

### Step 5：產出 G-code

1. 按 **Slice Now**
2. 確認預覽後點選 **Export G-code**
3. 開啟匯出的 `.gcode`，搜尋 `filament used`=xxx mm，將數字填入 `M290 Exxx`

### Step 6：上傳與執行

1. 將 `.gcode` 傳至 Arduino，或透過串列監控逐行發送
2. 推薦使用 [Pronterface](https://github.com/kliment/Printrun) 或其他串口工具傳送指令（官方下載頁：<https://github.com/kliment/Printrun/releases>）

```
[切片軟體]
   ↓ 輸出 .gcode
[上位機：Pronterface / 自製監控程式]
   ↓ 一行行透過 Serial 傳送
[Arduino 韌體]
   └── getGcodeInput()
         └── processGcode()
               ├── G1 → handleG1Axis() → moveAxis()
               ├── M104 → 設定溫度
                └── 其他 G / M 指令...
```

### 最小測試 G-code 範例

```gcode
G90
G92 X0 Y0 Z0 E0
G1 X10 Y10 F1200
G1 Z2
M104 S200
M105

M400
```

---

## DEBUG_INPUT 模式

在 `main.ino` 取消註解 `#define DEBUG_INPUT` 後，韌體會自動執行內建的 G-code 指令並模擬加熱與列印流程。
溫度將以線性方式上升至目標值並維持，實際上不啟動 MOSFET 及擠出馬達。
若已使用 `M290` 設定總進度，每次 E 軸指令後會在序列埠輸出目前百分比；
若未設定則提醒使用 `M290`。

## 聯絡作者

若有任何問題或建議，歡迎開 Issue 或 Pull Request！
