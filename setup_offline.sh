#!/bin/bash
export ARDUINO_DATA_DIR=/workspace/.arduino15
export ARDUINO_USER_DIR=/workspace/arduino-sketches

set -e  # 有錯就終止

# 解壓 Arduino CLI
mkdir -p bin
tar -xzf arduino-cli_1.2.2_Linux_64bit.tar.gz -C ./bin
chmod +x ./bin/arduino-cli

# 解壓 AVR Core 到對應資料夾
mkdir -p "$ARDUINO_DATA_DIR/packages/arduino/hardware/avr/1.8.6"
tar -xzf arduino-avr-core.tar.gz \
    -C "$ARDUINO_DATA_DIR/packages/arduino/hardware/avr/1.8.6" \
    --strip-components=1

# 初始化設定
./bin/arduino-cli config init

# 編譯 /workspace/main（你的專案）
./bin/arduino-cli compile --fqbn arduino:avr:uno /workspace/main
