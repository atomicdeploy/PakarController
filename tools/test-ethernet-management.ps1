[CmdletBinding()]
param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern network-cloud | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/ethernet_management_test.exe'
$taskJson=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
$taskArguments=@('-std=c++17','-Wall','-Wextra','-Werror','-DESP32','-DARDUINO_ARCH_ESP32',
  '-DARDUINOJSON_ENABLE_ARDUINO_STRING=1',
  '-I',(Join-Path $taskRoot 'tests/management_stubs'),'-I',(Join-Path $taskRoot 'tests/ethernet_config_stubs'),
  '-I',(Join-Path $taskRoot 'tests/network_stubs'),'-I',$taskJson,
  (Join-Path $taskRoot 'tests/ethernet_management_test.cpp'),
  (Join-Path $taskRoot 'firmware/esp/src/CloudConfig.cpp'),
  (Join-Path $taskRoot 'firmware/esp/src/NetworkConfig.cpp'),'-o',$taskOutput)
if($Plan){[pscustomobject]@{Executable='g++';Arguments=$taskArguments;Scope='Actual shared Ethernet management/store fixture, not SDK/W5500/network acceptance'};return}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
foreach($taskEnabled in 0,1) {
  & g++ "-DPAKAR_S3_ETHERNET_ENABLED=$taskEnabled" @taskArguments
  if($LASTEXITCODE){throw 'Ethernet management fixture compilation failed'}
  & $taskOutput
  if($LASTEXITCODE){throw 'Ethernet management fixture assertions failed'}
}
