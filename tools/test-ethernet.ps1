[CmdletBinding()]
param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern api-contract -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern network-cloud | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/s3_ethernet_port_test.exe'
$taskArguments=@('-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror',
  '-DARDUINO_ARCH_ESP32','-DCONFIG_IDF_TARGET_ESP32S3',
  '-I',(Join-Path $taskRoot 'tests/ethernet_stubs'),
  (Join-Path $taskRoot 'tests/s3_ethernet_port_test.cpp'),
  (Join-Path $taskRoot 'firmware/esp32s3/src/EthernetPort.cpp'),'-o',$taskOutput)
if($Plan){return [pscustomobject]@{
  Executable='g++';Arguments=$taskArguments;Profiles=@(0,1);
  Scope='Optional native W5500 adapter lifecycle and IPv4 contracts using SDK doubles, not actual SDK/lwIP or hardware'
}}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
foreach($taskProfile in @(0,1)) {
  & g++ "-DPAKAR_S3_ETHERNET_ENABLED=$taskProfile" @taskArguments
  if($LASTEXITCODE){throw 'Ethernet adapter fixture compilation failed'}
  & $taskOutput
  if($LASTEXITCODE){throw 'Ethernet adapter fixture assertions failed'}
}
