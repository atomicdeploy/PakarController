#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
c++ -std=c++17 -Wall -Wextra -pedantic \
  -I "$root/libraries/BoardCore/src" \
  "$root/tests/protocol_test.cpp" -o "$root/build/protocol-test"
"$root/build/protocol-test"
