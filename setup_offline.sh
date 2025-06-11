#!/bin/bash
export ARDUINO_DATA_DIR=/workspace/.arduino15
export ARDUINO_USER_DIR=/workspace/arduino-sketches

set -e

# 解壓 Arduino CLI
mkdir -p bin
tar -xzf arduino-cli_1.2.2_Linux_64bit.tar.gz -C ./bin
chmod +x ./bin/arduino-cli

# 解壓 AVR Core
mkdir -p "$ARDUINO_DATA_DIR/packages/arduino/hardware/avr/1.8.6"
tar -xzf arduino-avr-core.tar.gz \
  -C "$ARDUINO_DATA_DIR/packages/arduino/hardware/avr/1.8.6" \
  --strip-components=1

# 解壓 AVR Toolchain
mkdir -p "$ARDUINO_DATA_DIR/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7"
tar -xzf avr-toolchain.tar.gz \
  -C "$ARDUINO_DATA_DIR/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7" \
  --strip-components=1

# 初始化設定
./bin/arduino-cli config init

# 編譯你的 main 專案
./bin/arduino-cli compile --fqbn arduino:avr:uno ./main
