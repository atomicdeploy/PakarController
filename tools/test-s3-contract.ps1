[CmdletBinding()]
param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern firmware-hardware | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/s3_menu_audio_test.exe'
$taskArguments=@('-std=c++17','-Wall','-Wextra','-Werror',
  '-I',(Join-Path $taskRoot 'libraries/BoardCore/src'),
  '-I',(Join-Path $taskRoot 'firmware/esp32s3/src'),
  (Join-Path $taskRoot 'tests/s3_menu_audio_test.cpp'),'-o',$taskOutput)
if($Plan){[pscustomobject]@{Executable='g++';Arguments=$taskArguments;Scope='Pure pin/menu-audio contracts, not ESP32 driver compilation or physical playback'};return}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
& g++ @taskArguments
if($LASTEXITCODE){throw 'S3 contract compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'S3 contract assertions failed'}
$taskBleStorage=Join-Path $taskRoot 'build/native/ble_storage_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'tests/ble_storage_stubs') -I (Join-Path $taskRoot 'libraries/BoardCore/src') (Join-Path $taskRoot 'tests/ble_storage_test.cpp') -o $taskBleStorage
if($LASTEXITCODE){throw 'BLE PSRAM storage fixture compilation failed'}
& $taskBleStorage
if($LASTEXITCODE){throw 'BLE PSRAM storage fixture failed'}
