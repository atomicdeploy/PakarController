$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'build/native'
New-Item -ItemType Directory -Force -Path $out | Out-Null
& g++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/pixel_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/esp8266/src" "$root/tests/led_grid_test.cpp" "$root/firmware/esp8266/src/LedGrid.cpp" -o "$out/led_grid_test.exe"
if($LASTEXITCODE){throw 'Actual renderer compilation failed'}
& "$out/led_grid_test.exe"
if($LASTEXITCODE){throw 'Actual renderer checks failed'}
& g++ -std=c++17 -Wall -Wextra -Werror -pthread -I "$root/firmware/esp8266/src" "$root/tests/led_scheduler_test.cpp" -o "$out/led_scheduler_test.exe"
if($LASTEXITCODE){throw 'Actual message queue compilation failed'}
& "$out/led_scheduler_test.exe"
if($LASTEXITCODE){throw 'Actual message queue checks failed'}
& g++ -std=c++17 -O2 -Wall -Wextra -Werror -I "$root/tests/pixel_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/esp8266/src" "$root/tests/led_grid_stress_test.cpp" "$root/firmware/esp8266/src/LedGrid.cpp" -o "$out/led_grid_stress_test.exe"
if($LASTEXITCODE){throw 'Actual renderer stress compilation failed'}
& "$out/led_grid_stress_test.exe"
if($LASTEXITCODE){throw 'Actual renderer stress checks failed'}
& node (Join-Path $root 'tests/cell_chase_wiring_test.mjs')
if($LASTEXITCODE){throw 'Cell chase firmware wiring checks failed'}
