[CmdletBinding()]
param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern network-cloud | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/ethernet_config_test.exe'
$taskArguments=@('-std=c++17','-Wall','-Wextra','-Werror','-DESP32',
  '-I',(Join-Path $taskRoot 'tests/ethernet_config_stubs'),
  (Join-Path $taskRoot 'tests/ethernet_config_test.cpp'),'-o',$taskOutput)
if($Plan){[pscustomobject]@{Executable='g++';Arguments=$taskArguments;Scope='Actual independent Ethernet profile fixture, not SDK/W5500/network acceptance'};return}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
& g++ @taskArguments
if($LASTEXITCODE){throw 'Ethernet configuration fixture compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Ethernet configuration fixture assertions failed'}
