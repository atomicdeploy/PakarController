#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build="$root/build"
libraries="$root/libraries"

mkdir -p "$build/avr" "$build/esp8266"

arduino-cli compile --clean \
  --fqbn 'MiniCore:avr:328:bootloader=uart0,eeprom=keep,baudrate=115200,variant=modelP,BOD=2v7,LTO=Os_flto,clock=16MHz_external' \
  --libraries "$libraries" \
  --output-dir "$build/avr" \
  "$root/firmware/avr"

arduino-cli compile --clean \
  --fqbn 'esp8266:esp8266:nodemcuv2:eesz=4M2M' \
  --libraries "$libraries" \
  --output-dir "$build/esp8266" \
  "$root/firmware/esp8266"

test "$(wc -c < "$build/avr/avr.ino.with_bootloader.bin")" -eq 32768
sha256sum "$build/avr/avr.ino.with_bootloader.bin" \
  "$build/esp8266/esp8266.ino.bin" > "$build/SHA256SUMS"

echo 'Firmware artifacts built successfully.'
