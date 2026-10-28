$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern led-benchmark -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOut=Join-Path $taskRoot 'build/native/matrix_fire_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOut) | Out-Null
$taskJson=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'tests/correction_stubs') -I (Join-Path $taskRoot 'tests/access_stubs') -I $taskJson -I (Join-Path $taskRoot 'libraries/BoardCore/src') -I (Join-Path $taskRoot 'firmware/esp/src') (Join-Path $taskRoot 'tests/matrix_fire_test.cpp') -o $taskOut
if($LASTEXITCODE){throw 'Fire/appearance settings compilation failed'}
& $taskOut
if($LASTEXITCODE){throw 'Fire/appearance settings assertions failed'}
