[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern peer-binding -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/host_demand_test.exe'
New-Item -ItemType Directory -Force (Split-Path -Parent $taskOutput) | Out-Null
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -DESP32 -I (Join-Path $taskRoot 'tests/serial_stubs') -I (Join-Path $taskRoot 'libraries/BoardCore/src') (Join-Path $taskRoot 'tests/host_demand_test.cpp') -o $taskOutput
if($LASTEXITCODE){throw 'Host demand fixture compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Host demand fixture failed'}
$taskUsb=Join-Path $taskRoot 'build/native/host_usb_test.exe'
foreach($taskMode in @(0,1,2)) {
  $taskUsbMode=if($taskMode -eq 2){0}else{1}
  $taskCdcBoot=if($taskMode -eq 2){0}else{1}
  & g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -DARDUINO_ARCH_ESP32 "-DPAKAR_S3_HOST_TRANSPORT=$taskMode" "-DARDUINO_USB_MODE=$taskUsbMode" "-DARDUINO_USB_CDC_ON_BOOT=$taskCdcBoot" -I (Join-Path $taskRoot 'tests/host_usb_stubs') -I (Join-Path $taskRoot 'libraries/BoardCore/src') (Join-Path $taskRoot 'tests/host_usb_test.cpp') -o $taskUsb
  if($LASTEXITCODE){throw "Host USB mode $taskMode fixture compilation failed"}
  & $taskUsb
  if($LASTEXITCODE){throw "Host USB mode $taskMode fixture failed"}
}
& node (Join-Path $taskRoot 'tests/host_demand_source_test.mjs')
if($LASTEXITCODE){throw 'Host demand integration source assertions failed'}
