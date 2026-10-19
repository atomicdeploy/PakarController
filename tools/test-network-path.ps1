[CmdletBinding()]
param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern api-contract -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern network-cloud | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/network_path_test.exe'
$taskArguments=@('-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror',
  '-DESP32','-DARDUINO_ARCH_ESP32','-DCONFIG_IDF_TARGET_ESP32S3',
  '-I',(Join-Path $taskRoot 'tests/network_path_stubs'),'-I',(Join-Path $taskRoot 'tests/ethernet_stubs'),'-I',$taskRoot,
  (Join-Path $taskRoot 'tests/network_path_test.cpp'),
  (Join-Path $taskRoot 'tests/ethernet_lldp_runtime.cpp'),
  (Join-Path $taskRoot 'firmware/esp32s3/src/EthernetLldp.cpp'),
  (Join-Path $taskRoot 'firmware/esp32s3/src/EthernetPort.cpp'),'-Wl,--wrap=esp_eth_update_input_path_info','-o',$taskOutput)
if($Plan){return [pscustomobject]@{
  Executable='g++';Arguments=$taskArguments;Profiles=@(0,1);
  Scope='Actual shared NetworkPath and discovery interface policy with SDK doubles; not actual SDK/lwIP/multicast/hardware'
}}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
foreach($taskProfile in @(0,1)) {
  & g++ "-DPAKAR_S3_ETHERNET_ENABLED=$taskProfile" @taskArguments
  if($LASTEXITCODE){throw 'NetworkPath fixture compilation failed'}
  & $taskOutput
  if($LASTEXITCODE){throw 'NetworkPath fixture assertions failed'}
}
