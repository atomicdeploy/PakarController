param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern peer-binding -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'build/native/cloud_services_test.exe'
$json=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
New-Item -ItemType Directory -Force (Split-Path -Parent $out) | Out-Null
foreach($configuration in @('ESP8266','ESP32')) {
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror "-D$configuration" `
  -I (Join-Path $root 'tests/cloud_service_stubs') -I $json `
  -I (Join-Path $root 'libraries/BoardCore/src') -I (Join-Path $root 'firmware/esp8266/src') `
  (Join-Path $root 'tests/cloud_services_test.cpp') `
  (Join-Path $root 'firmware/esp8266/src/CloudServices.cpp') `
  (Join-Path $root 'firmware/esp8266/src/CloudConfig.cpp') -o $out
if($LASTEXITCODE){throw "Cloud service $configuration focused fixture compilation failed"}
& $out
if($LASTEXITCODE){throw "Cloud service $configuration focused assertions failed"}
}
