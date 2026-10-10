#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
c++ -std=c++17 -Wall -Wextra -pedantic \
  -I "$root/libraries/BoardCore/src" \
  "$root/tests/protocol_test.cpp" -o "$root/build/protocol-test"
"$root/build/protocol-test"
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
