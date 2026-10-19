[CmdletBinding()]
param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern updates-telemetry | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/filesystem_telemetry_test.exe'
$taskJson=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
$taskArguments=@('-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror','-I',$taskJson,
  (Join-Path $taskRoot 'tests/filesystem_telemetry_test.cpp'),'-o',$taskOutput)
if($Plan){return [pscustomobject]@{Executable='g++';Arguments=$taskArguments;Scope='Actual pure diagnostic filesystem cache/JSON and boot/runtime source guards, not SDK/filesystem/network/LED timing'}}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
& g++ @taskArguments
if($LASTEXITCODE){throw 'Filesystem telemetry fixture compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Filesystem telemetry fixture assertions failed'}
& node (Join-Path $taskRoot 'tests/filesystem_telemetry_test.mjs')
if($LASTEXITCODE){throw 'Filesystem telemetry source guards failed'}
