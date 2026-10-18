[CmdletBinding()]
param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern api-contract -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern lcd-display -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/lcd_display_test.exe'
$taskArguments=@('-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror','-I',(Join-Path $taskRoot 'libraries/BoardCore/src'),(Join-Path $taskRoot 'tests/lcd_display_test.cpp'),'-o',$taskOutput)
if($Plan){return [pscustomobject]@{Executable='g++';Arguments=$taskArguments;Scope='Actual LCD driver with bus doubles; no hardware'}}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
& g++ @taskArguments
if($LASTEXITCODE){throw 'LCD driver fixture compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'LCD driver fixture assertions failed'}
$taskJson=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
$taskManagementOutput=Join-Path $taskRoot 'build/native/lcd_management_test.exe'
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -DARDUINO_ARCH_ESP32 -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -I (Join-Path $taskRoot 'tests/access_stubs') -I (Join-Path $taskRoot 'libraries/BoardCore/src') -I $taskJson (Join-Path $taskRoot 'tests/lcd_management_test.cpp') -o $taskManagementOutput
if($LASTEXITCODE){throw 'LCD HTTP fixture compilation failed'}
& $taskManagementOutput
if($LASTEXITCODE){throw 'LCD HTTP fixture assertions failed'}
