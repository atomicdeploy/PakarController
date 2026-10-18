[CmdletBinding()]
param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern network-cloud | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/cloud_management_test.exe'
$taskJson=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
$taskArguments=@('-std=c++17','-Wall','-Wextra','-Werror','-DARDUINOJSON_ENABLE_ARDUINO_STRING=1',
  '-I',(Join-Path $taskRoot 'tests/management_stubs'),'-I',(Join-Path $taskRoot 'tests/cloud_stubs'),
  '-I',(Join-Path $taskRoot 'tests/network_stubs'),'-I',$taskJson,
  (Join-Path $taskRoot 'tests/cloud_management_test.cpp'),
  (Join-Path $taskRoot 'firmware/esp8266/src/CloudConfig.cpp'),
  (Join-Path $taskRoot 'firmware/esp8266/src/NetworkConfig.cpp'),'-o',$taskOutput)
if($Plan){[pscustomobject]@{Executable='g++';Arguments=$taskArguments;Scope='Actual shared configuration handler/store fixture; not SDK/cloud/hardware acceptance'};return}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
foreach($taskPlatform in @(@('-DESP8266'),@('-DESP32','-DARDUINO_ARCH_ESP32'))) {
  & g++ @taskPlatform @taskArguments
  if($LASTEXITCODE){throw 'Shared configuration fixture compilation failed'}
  & $taskOutput
  if($LASTEXITCODE){throw 'Shared configuration fixture assertions failed'}
}
