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
case "$target" in both|avr|esp8266|esp32s3) ;; *) echo "target must be both, avr, esp8266 or esp32s3" >&2; exit 2 ;; esac
if [[ -n "${3:-}" && "${3:-}" != "--clean" ]]; then echo "third argument must be --clean" >&2; exit 2; fi

case "$link_mode" in
  polling) busy_flag=0; buffered_flag=0; suffix="" ;;
  busy-wire) busy_flag=1; buffered_flag=0; suffix="-busy-wire" ;;
  buffered) busy_flag=0; buffered_flag=1; suffix="-buffered" ;;
  *) echo "link mode must be polling, busy-wire or buffered" >&2; exit 2 ;;
esac
if [[ "$link_mode" == "buffered" && "$target" != "avr" && "$target" != "esp32s3" ]]; then
  echo "buffered mode targets paired AVR only; ESP8266 needs busy-wire or polling" >&2; exit 2
fi
if [[ "$target" == "esp32s3" ]]; then
  # Reuse the same reviewed S3 command and warm per-target cache on both hosts.
  pwsh -NoProfile -File "$root/tools/build-firmware.ps1" -Target esp32s3 \
    -LinkMode "$link_mode" -S3FlashMB "${PAKAR_S3_FLASH_MB:-16}" \
    -S3Psram "${PAKAR_S3_PSRAM:-opi}"
  exit $?
fi

if [[ "$target" != "esp8266" ]]; then
pwsh -NoProfile -File "$root/tools/consult-reuse.ps1" -Concern uart-flow-control -Gate > /dev/null
pwsh -NoProfile -File "$root/tools/consult-reuse.ps1" -Concern tm1637-display -Gate > /dev/null
mkdir -p "$build/avr$suffix"
arduino-cli compile "${clean_args[@]}" \
  --fqbn 'MiniCore:avr:328:bootloader=uart0,eeprom=keep,baudrate=115200,variant=modelP,BOD=2v7,LTO=Os_flto,clock=16MHz_external' \
  --libraries "$libraries" \
  --build-property "compiler.cpp.extra_flags=-DPAKAR_USE_BUSY_WIRE=$busy_flag -DPAKAR_UART_BUFFERED=$buffered_flag" \
  --build-path "$build/cache/avr$suffix" \
  --output-dir "$build/avr$suffix" \
  "$root/firmware/avr"
test "$(wc -c < "$build/avr$suffix/avr.ino.with_bootloader.bin")" -eq 32768
fi

if [[ "$target" != "avr" ]]; then
pwsh -NoProfile -File "$root/tools/consult-reuse.ps1" -Concern web-presentation -Gate > /dev/null
node "$root/tools/generate-font-assets.mjs" --check
node "$root/tools/generate-branding.mjs" --check
node "$root/tools/generate-web-appearance.mjs" --check
node "$root/tools/generate-interface-contract.mjs" --check
mkdir -p "$build/esp8266$suffix"
arduino-cli compile "${clean_args[@]}" \
  --fqbn 'esp8266:esp8266:nodemcuv2:eesz=4M2M' \
  --libraries "$libraries" \
  --build-property "compiler.cpp.extra_flags=-DPAKAR_USE_BUSY_WIRE=$busy_flag -DPAKAR_UART_BUFFERED=$buffered_flag" \
  --build-path "$build/cache/esp8266$suffix" \
  --output-dir "$build/esp8266$suffix" \
  "$root/firmware/esp8266"
fi

if [[ "$target" == "both" ]]; then
sha256sum "$build/avr$suffix/avr.ino.with_bootloader.bin" \
  "$build/esp8266$suffix/esp8266.ino.bin" > "$build/SHA256SUMS$suffix"
fi

echo "Firmware artifacts built successfully in $link_mode mode."
