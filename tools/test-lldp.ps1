[CmdletBinding()]
param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern ethernet -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/ethernet_lldp_test.exe'
$taskArguments=@('-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror',
  '-DARDUINO_ARCH_ESP32','-DCONFIG_IDF_TARGET_ESP32S3','-DPAKAR_S3_ETHERNET_ENABLED=1',
  '-I',(Join-Path $taskRoot 'tests/ethernet_stubs'),
  '-I',(Join-Path $taskRoot 'libraries/BoardCore/src'),
  (Join-Path $taskRoot 'tests/ethernet_lldp_test.cpp'),
  (Join-Path $taskRoot 'tests/ethernet_lldp_runtime.cpp'),
  (Join-Path $taskRoot 'firmware/esp32s3/src/EthernetLldp.cpp'),
  '-Wl,--wrap=esp_eth_update_input_path_info','-o',$taskOutput)
if($Plan){return [pscustomobject]@{Executable='g++';Arguments=$taskArguments;Scope='Actual LLDP codec/scoped glue and TCPIP job fixture using SDK doubles, not Ethernet link or switch visibility'}}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
& g++ @taskArguments
if($LASTEXITCODE){throw 'LLDP fixture compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'LLDP fixture assertions failed'}
