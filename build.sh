#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build="$root/build"
libraries="$root/libraries"
link_mode="${1:-${PAKAR_LINK_MODE:-busy-wire}}"

case "$link_mode" in
  polling) busy_flag=0; suffix="" ;;
  busy-wire) busy_flag=1; suffix="-busy-wire" ;;
  *) echo "link mode must be polling or busy-wire" >&2; exit 2 ;;
esac

mkdir -p "$build/avr$suffix" "$build/esp8266$suffix"

arduino-cli compile --clean \
  --fqbn 'MiniCore:avr:328:bootloader=uart0,eeprom=keep,baudrate=115200,variant=modelP,BOD=2v7,LTO=Os_flto,clock=16MHz_external' \
  --libraries "$libraries" \
  --build-property "compiler.cpp.extra_flags=-DPAKAR_USE_BUSY_WIRE=$busy_flag" \
  --output-dir "$build/avr$suffix" \
  "$root/firmware/avr"

arduino-cli compile --clean \
  --fqbn 'esp8266:esp8266:nodemcuv2:eesz=4M2M' \
  --libraries "$libraries" \
  --build-property "compiler.cpp.extra_flags=-DPAKAR_USE_BUSY_WIRE=$busy_flag" \
  --output-dir "$build/esp8266$suffix" \
  "$root/firmware/esp8266"

test "$(wc -c < "$build/avr$suffix/avr.ino.with_bootloader.bin")" -eq 32768
sha256sum "$build/avr$suffix/avr.ino.with_bootloader.bin" \
  "$build/esp8266$suffix/esp8266.ino.bin" > "$build/SHA256SUMS$suffix"

echo "Firmware artifacts built successfully in $link_mode mode."
