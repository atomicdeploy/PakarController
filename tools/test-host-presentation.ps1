[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern games-streaming -Summary | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/host_presentation_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
$taskJson=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'tests/correction_stubs') -I (Join-Path $taskRoot 'tests/access_stubs') -I $taskJson -I (Join-Path $taskRoot 'libraries/BoardCore/src') -I (Join-Path $taskRoot 'firmware/esp/src') (Join-Path $taskRoot 'tests/host_presentation_test.cpp') -o $taskOutput
if($LASTEXITCODE){throw 'Host presentation compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Host presentation assertions failed'}
& node (Join-Path $taskRoot 'tests/host_presentation_routes_test.mjs')
if($LASTEXITCODE){throw 'Host presentation/AVR latency route assertions failed'}
& node (Join-Path $taskRoot 'tests/host_mini_apps_test.mjs')
if($LASTEXITCODE){throw 'Host mini-app examples failed'}
