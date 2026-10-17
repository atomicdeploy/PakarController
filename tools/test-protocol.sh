#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
c++ -std=c++17 -Wall -Wextra -pedantic \
  -I "$root/libraries/BoardCore/src" \
  "$root/tests/protocol_test.cpp" -o "$root/build/protocol-test"
"$root/build/protocol-test"
c++ -std=c++17 -Wall -Wextra -Werror "$root/tests/mndp_test.cpp" -o "$root/build/mndp-test"
"$root/build/mndp-test"
c++ -std=c++17 -Wall -Wextra -Werror "$root/tests/remote_policy_test.cpp" -o "$root/build/remote-policy-test"
"$root/build/remote-policy-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/isp_stubs" "$root/tests/avr_isp_test.cpp" "$root/firmware/esp8266/src/AvrIsp.cpp" -o "$root/build/avr-isp-test"
"$root/build/avr-isp-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/access_stubs" -I "$HOME/Arduino/libraries/ArduinoJson/src" "$root/tests/client_telemetry_test.cpp" -o "$root/build/client-telemetry-test"
"$root/build/client-telemetry-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/remote_stubs" -I "$HOME/Arduino/libraries/ArduinoJson/src" -I "$root/libraries/BoardCore/src" "$root/tests/remote_firmware_test.cpp" "$root/firmware/esp8266/src/RemoteFirmware.cpp" -lcrypto -o "$root/build/remote-firmware-test"
"$root/build/remote-firmware-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/firmware/esp8266/src" "$root/tests/telnet_shell_test.cpp" -o "$root/build/telnet-shell-test"
"$root/build/telnet-shell-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/busy_stubs" -I "$root/firmware/esp8266/src" "$root/tests/busy_line_test.cpp" -o "$root/build/busy-line-test"
"$root/build/busy-line-test"
c++ -std=c++17 -Wall -Wextra -Werror -DF_CPU=16000000UL -I "$root/libraries/BoardCore/src" -I "$root/tests/tone_stubs" -I "$root/firmware/avr" "$root/tests/tone_player_test.cpp" "$root/firmware/avr/TonePlayer.cpp" "$root/firmware/avr/BootMelody.cpp" -o "$root/build/tone-player-test"
"$root/build/tone-player-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/access_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/esp8266/src" \
  "$root/tests/device_identity_test.cpp" "$root/firmware/esp8266/src/DeviceIdentity.cpp" -o "$root/build/device-identity-test"
"$root/build/device-identity-test"
for identity_flags in -DPAKAR_TABLE_ENABLED=1 -DPAKAR_TABLE_ENABLED=0 -DPAKAR_IDENTITY_CUSTOM=1; do
  c++ -std=c++17 -Wall -Wextra -Werror "$identity_flags" -I "$root/tests/access_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/esp8266/src" "$root/tests/application_identity_test.cpp" -o "$root/build/application-identity-test"
  "$root/build/application-identity-test"
done
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/libraries/BoardCore/src" \
  "$root/tests/access_policy_test.cpp" -o "$root/build/access-policy-test"
"$root/build/access-policy-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/libraries/BoardCore/src" "$root/tests/device_context_test.cpp" -o "$root/build/device-context-test"
"$root/build/device-context-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/access_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/esp8266/src" \
  "$root/tests/access_config_test.cpp" "$root/firmware/esp8266/src/AccessConfig.cpp" -o "$root/build/access-config-test"
"$root/build/access-config-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/access_stubs" -I "$root/firmware/esp8266/src" \
  "$root/tests/async_http_test.cpp" -o "$root/build/async-http-test"
"$root/build/async-http-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/access_stubs" -I "$root/firmware/esp8266/src" \
  "$root/tests/firmware_range_test.cpp" -o "$root/build/firmware-range-test"
"$root/build/firmware-range-test"
c++ -std=c++17 -Wall -Wextra -Werror \
  -I "$root/tests/stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/avr" \
  "$root/tests/front_panel_test.cpp" "$root/firmware/avr/FrontPanel.cpp" \
  -o "$root/build/front-panel-test"
"$root/build/front-panel-test"
c++ -std=c++17 -Wall -Wextra -Werror \
  -I "$root/tests/pc_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/avr" \
  "$root/tests/pc_contract_test.cpp" "$root/firmware/avr/PcControllerRuntime.cpp" \
  "$root/firmware/avr/BoardSettings.cpp" "$root/firmware/avr/SevenSegments.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/MacroRing.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/Keys.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/ControllerEvents.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/AutomationStore.cpp" \
  -o "$root/build/pc-contract-test"
"$root/build/pc-contract-test"
c++ -std=c++17 -Wall -Wextra -Werror \
  -I "$root/tests/pc_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/avr" -I "$root/firmware/esp8266/src" \
  "$root/tests/esp_front_panel_test.cpp" "$root/firmware/avr/PcControllerRuntime.cpp" \
  "$root/firmware/avr/BoardSettings.cpp" "$root/firmware/avr/SevenSegments.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/MacroRing.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/Keys.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/ControllerEvents.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/AutomationStore.cpp" -o "$root/build/esp-front-panel-test"
"$root/build/esp-front-panel-test"
for rgb_case in s3_status_rgb_test s3_status_rgb_config_test s3_passive_avr_rx_test; do
  c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/access_stubs" -I "$root/firmware/esp32s3/src" \
    "$root/tests/$rgb_case.cpp" -o "$root/build/$rgb_case"
  "$root/build/$rgb_case"
done
pwsh -NoProfile -File "$root/tests/device_network_script_test.ps1"
pwsh -NoProfile -File "$root/tests/avr_rx_script_test.ps1"
pwsh -NoProfile -File "$root/tools/consult-reuse.ps1" -Concern tm1637-display -Gate > /dev/null
for display_flags in '' '-D__AVR__'; do
  c++ -std=c++17 -Wall -Wextra -Werror $display_flags -I "$root/tests/display_stubs" -I "$root/firmware/avr" \
    "$root/tests/tm1637_bus_test.cpp" "$root/firmware/avr/SevenSegments.cpp" -o "$root/build/tm1637-bus-test"
  "$root/build/tm1637-bus-test"
done
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/pixel_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/esp8266/src" \
  "$root/tests/led_grid_test.cpp" "$root/firmware/esp8266/src/LedGrid.cpp" -o "$root/build/led-grid-test"
"$root/build/led-grid-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/libraries/BoardCore/src" -I "$root/firmware/esp32s3/src" \
  "$root/tests/s3_menu_audio_test.cpp" -o "$root/build/s3-menu-audio-test"
"$root/build/s3-menu-audio-test"
pwsh -NoProfile -File "$root/tools/test-uart-flow.ps1"
pwsh -NoProfile -File "$root/tools/consult-reuse.ps1" -Concern peer-binding -Gate > /dev/null
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/access_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/esp8266/src" \
  "$root/tests/web_access_fence_test.cpp" "$root/firmware/esp8266/src/AccessConfig.cpp" -o "$root/build/web-access-fence-test"
"$root/build/web-access-fence-test"
pwsh -NoProfile -File "$root/tools/test-tasks.ps1"
