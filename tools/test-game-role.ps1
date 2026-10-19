param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern firmware-hardware | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$root=Split-Path -Parent $PSScriptRoot
$binary=Join-Path $root 'build/native/game_role_test.exe'
foreach($mode in @(1,2)) {
  & g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror "-DPAKAR_GAME_MODE=$mode" -I "$root/libraries/BoardCore/src" "$root/tests/device_context_test.cpp" -o $binary
  if($LASTEXITCODE){throw "Game role $mode compilation failed"}
  & $binary
  if($LASTEXITCODE){throw "Game role $mode contract failed"}
  & g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror "-DPAKAR_GAME_MODE=$mode" -I "$root/tests/pixel_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/esp8266/src" "$root/tests/game_role_renderer_test.cpp" "$root/firmware/esp8266/src/LedGrid.cpp" -o $binary
  if($LASTEXITCODE){throw "Game role $mode renderer compilation failed"}
  & $binary
  if($LASTEXITCODE){throw "Game role $mode saved-scene migration failed"}
}
& g++ -std=c++17 -fsyntax-only -DPAKAR_GAME_MODE=3 -I "$root/libraries/BoardCore/src" "$root/tests/device_context_test.cpp" 2>$null
if(!$LASTEXITCODE){throw 'Unsupported baked game was accepted'}
$global:LASTEXITCODE=0 # The expected negative compile is not a failing runner.
Write-Host 'Single baked role: both valid profiles and unsupported-role compile rejection passed; source fixture only.'
