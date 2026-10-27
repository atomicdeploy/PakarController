$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern firmware-hardware | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$root=Split-Path -Parent $PSScriptRoot
$binary=Join-Path $root 'build/native/radio_adapter_test.exe'
New-Item -ItemType Directory -Force (Split-Path -Parent $binary) | Out-Null
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -DPAKAR_PC_RF=1 `
  -I "$root/tests/radio_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/avr" `
  "$root/tests/radio_adapter_test.cpp" -o $binary
if($LASTEXITCODE){throw 'Radio adapter fixture compilation failed'}
& $binary
if($LASTEXITCODE){throw 'Radio adapter fixture failed'}
