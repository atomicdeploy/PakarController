[CmdletBinding()]
param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern bluetooth -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/ble_wire_test.exe'
$taskArguments=@('-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror',
  '-I',(Join-Path $taskRoot 'libraries/BoardCore/src'),
  (Join-Path $taskRoot 'tests/ble_wire_test.cpp'),'-o',$taskOutput)
if($Plan){[pscustomobject]@{Executable='g++';Arguments=$taskArguments;Scope='BLE frame/session fixtures only; not radio, Android or physical acceptance'};return}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
& g++ @taskArguments
if($LASTEXITCODE){throw 'BLE wire fixture compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'BLE wire assertions failed'}
& node (Join-Path $taskRoot 'tests/ble_firmware_test.mjs')
if($LASTEXITCODE){throw 'BLE firmware integration guards failed'}
