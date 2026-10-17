param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern firmware-hardware | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern api-contract -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern cooperative-tasks -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/avr_reset_telemetry_test.exe'
New-Item -ItemType Directory -Force (Split-Path -Parent $taskOutput) | Out-Null
& g++ -std=c++17 -Wall -Wextra -Werror -DPAKAR_RESET_NATIVE_TEST=1 `
  -I (Join-Path $taskRoot 'tests/pc_stubs') -I (Join-Path $taskRoot 'libraries/BoardCore/src') `
  (Join-Path $taskRoot 'tests/avr_reset_telemetry_test.cpp') (Join-Path $taskRoot 'firmware/avr/ResetTelemetry.cpp') -o $taskOutput
if($LASTEXITCODE){throw 'Actual AVR reset journal fixture compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Actual AVR reset journal fixture failed'}
