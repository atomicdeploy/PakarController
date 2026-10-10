#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
c++ -std=c++17 -Wall -Wextra -pedantic \
  -I "$root/libraries/BoardCore/src" \
  "$root/tests/protocol_test.cpp" -o "$root/build/protocol-test"
"$root/build/protocol-test"
c++ -std=c++17 -Wall -Wextra -Werror \
  -I "$root/tests/stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/avr" \
  "$root/tests/front_panel_test.cpp" "$root/firmware/avr/FrontPanel.cpp" \
  -o "$root/build/front-panel-test"
"$root/build/front-panel-test"
