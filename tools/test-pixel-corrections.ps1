$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern games-streaming -Summary | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/pixel_corrections_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'libraries/BoardCore/src') (Join-Path $taskRoot 'tests/pixel_corrections_test.cpp') -o $taskOutput
if($LASTEXITCODE){throw 'Pixel corrections compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Pixel corrections assertions failed'}
$taskJson=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
$taskStoreOutput=Join-Path $taskRoot 'build/native/pixel_correction_store_test.exe'
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'tests/correction_stubs') -I (Join-Path $taskRoot 'tests/access_stubs') -I $taskJson -I (Join-Path $taskRoot 'libraries/BoardCore/src') -I (Join-Path $taskRoot 'firmware/esp/src') (Join-Path $taskRoot 'tests/pixel_correction_store_test.cpp') -o $taskStoreOutput
if($LASTEXITCODE){throw 'Pixel correction store compilation failed'}
& $taskStoreOutput
if($LASTEXITCODE){throw 'Pixel correction store assertions failed'}
