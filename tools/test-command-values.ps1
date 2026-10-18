param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern peer-binding -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'build/native/command_values_test.exe'
$json=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
New-Item -ItemType Directory -Force (Split-Path -Parent $out) | Out-Null
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror `
  -I (Join-Path $root 'tests/access_stubs') -I $json `
  -I (Join-Path $root 'libraries/BoardCore/src') -I (Join-Path $root 'firmware/esp8266/src') `
  (Join-Path $root 'tests/command_values_test.cpp') -o $out
if($LASTEXITCODE){throw 'Command value fixture compilation failed'}
& $out
if($LASTEXITCODE){throw 'Command value fixture assertions failed'}
