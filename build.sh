#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build="$root/build"
libraries="$root/libraries"
link_mode="${1:-${PAKAR_LINK_MODE:-busy-wire}}"
target="${2:-both}"
clean_args=()
# Explicit troubleshooting only, never part of a routine build.
if [[ "${3:-}" == "--clean" ]]; then clean_args=(--clean); fi
case "$target" in both|avr|esp8266) ;; *) echo "target must be both, avr or esp8266" >&2; exit 2 ;; esac
if [[ -n "${3:-}" && "${3:-}" != "--clean" ]]; then echo "third argument must be --clean" >&2; exit 2; fi

case "$link_mode" in
  polling) busy_flag=0; suffix="" ;;
  busy-wire) busy_flag=1; suffix="-busy-wire" ;;
  *) echo "link mode must be polling or busy-wire" >&2; exit 2 ;;
esac

if [[ "$target" != "esp8266" ]]; then
mkdir -p "$build/avr$suffix"
arduino-cli compile "${clean_args[@]}" \
  --fqbn 'MiniCore:avr:328:bootloader=uart0,eeprom=keep,baudrate=115200,variant=modelP,BOD=2v7,LTO=Os_flto,clock=16MHz_external' \
  --libraries "$libraries" \
  --build-property "compiler.cpp.extra_flags=-DPAKAR_USE_BUSY_WIRE=$busy_flag" \
  --build-path "$build/cache/avr$suffix" \
  --output-dir "$build/avr$suffix" \
  "$root/firmware/avr"
test "$(wc -c < "$build/avr$suffix/avr.ino.with_bootloader.bin")" -eq 32768
fi

if [[ "$target" != "avr" ]]; then
node "$root/tools/generate-web-appearance.mjs" --check
mkdir -p "$build/esp8266$suffix"
arduino-cli compile "${clean_args[@]}" \
  --fqbn 'esp8266:esp8266:nodemcuv2:eesz=4M2M' \
  --libraries "$libraries" \
  --build-property "compiler.cpp.extra_flags=-DPAKAR_USE_BUSY_WIRE=$busy_flag" \
  --build-path "$build/cache/esp8266$suffix" \
  --output-dir "$build/esp8266$suffix" \
  "$root/firmware/esp8266"
fi

if [[ "$target" == "both" ]]; then
sha256sum "$build/avr$suffix/avr.ino.with_bootloader.bin" \
  "$build/esp8266$suffix/esp8266.ino.bin" > "$build/SHA256SUMS$suffix"
fi

echo "Firmware artifacts built successfully in $link_mode mode."
