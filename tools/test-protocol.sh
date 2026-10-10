#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
c++ -std=c++17 -Wall -Wextra -pedantic \
  -I "$root/libraries/BoardCore/src" \
  "$root/tests/protocol_test.cpp" -o "$root/build/protocol-test"
"$root/build/protocol-test"
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
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/libraries/BoardCore/src" \
  "$root/tests/access_policy_test.cpp" -o "$root/build/access-policy-test"
"$root/build/access-policy-test"
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
