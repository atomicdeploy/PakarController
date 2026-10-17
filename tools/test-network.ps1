[CmdletBinding()]
param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern network-cloud | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/network_config_test.exe'
$taskArguments=@('-std=c++17','-Wall','-Wextra','-Werror',
  '-I',(Join-Path $taskRoot 'tests/network_stubs'),
  (Join-Path $taskRoot 'tests/network_config_test.cpp'),
  (Join-Path $taskRoot 'firmware/esp8266/src/NetworkConfig.cpp'),'-o',$taskOutput)
if($Plan){[pscustomobject]@{Executable='g++';Arguments=$taskArguments;Scope='Shared network persistence/validation fixture, not ESP32 SDK or Wi-Fi acceptance'};return}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
foreach($taskPlatform in @('-DESP8266','-DESP32')) {
  & g++ $taskPlatform @taskArguments
  if($LASTEXITCODE){throw 'Network contract compilation failed'}
  & $taskOutput
  if($LASTEXITCODE){throw 'Network contract assertions failed'}
}
