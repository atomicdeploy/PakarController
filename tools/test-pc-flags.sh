#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$root/build"
c++ -std=c++17 -Wall -Wextra -Werror \
  -DPAKAR_PC_MENU=0 -DPAKAR_PC_MACROS=0 -DPAKAR_PC_AUTOMATIONS=0 -DPAKAR_PC_ESP_LED=0 \
  -I "$root/tests/pc_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/avr" \
  "$root/tests/pc_feature_flags_test.cpp" "$root/firmware/avr/PcControllerRuntime.cpp" \
  "$root/firmware/avr/BoardSettings.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/ControllerEvents.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/AutomationStore.cpp" \
  -o "$root/build/pc-flags-test"
"$root/build/pc-flags-test"
for feature in I2C SENSORS PWM RF RELAYS STATUS_RGB AVR_LED; do
  if error="$(c++ -std=c++17 -x c++ -c -DPAKAR_PC_"$feature"=1 \
      "$root/libraries/BoardCore/src/BoardFeatures.h" -o "$root/build/pc-future.o" 2>&1)"; then
    echo "Unsafe future hardware feature compiled without its adapter: $feature" >&2
    exit 1
  fi
  case "$error" in
    *"Optional PCController hardware needs its adapter and pin profile"*) ;;
    *) echo "$error" >&2; exit 1 ;;
  esac
done
echo "All seven future hardware flags fail closed with the specific adapter/pin-profile diagnostic."
