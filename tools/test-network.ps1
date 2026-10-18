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
& g++ -DESP32 -DPAKAR_S3_WIFI_MODEM_SLEEP=1 @taskArguments
if($LASTEXITCODE){throw 'Opt-in modem-sleep contract compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Opt-in modem-sleep contract assertions failed'}
$taskDiscovery=Join-Path $taskRoot 'build/native/discovery_mdns_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror (Join-Path $taskRoot 'tests/discovery_mdns_test.cpp') -o $taskDiscovery
if($LASTEXITCODE){throw 'mDNS registration fixture compilation failed'}
& $taskDiscovery
if($LASTEXITCODE){throw 'mDNS registration fixture failed'}
