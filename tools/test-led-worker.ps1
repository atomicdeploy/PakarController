param([ValidateSet('all','static-gpio47')][string]$Profile='all')
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern firmware-hardware | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern esp32-ws2811 -Gate | Out-Null
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'build/native/led_worker_test.exe'
New-Item -ItemType Directory -Force (Split-Path -Parent $out) | Out-Null
foreach($wireProfile in $(if($Profile -eq 'all'){@('static-bgr','static-rgb','dma-bgr','dma-rgb','static-bgr-calibrated','dma-bgr-calibrated','static-gpio47','dual','dual-pm','absent')}else{@($Profile)})){
 $flags=@();if($wireProfile -like '*-rgb'){$flags+=@('-DPAKAR_S3_LED_SWAP_RED_BLUE=0')}
 if($wireProfile -eq 'static-gpio47'){$flags+=@('-DPAKAR_S3_LED_DATA=47')}
 if($wireProfile -like '*-calibrated'){$flags+=@('-DPAKAR_S3_LED_RGB_GAINS=0xffb0f0')}
 if($wireProfile -like 'dma-*'){$flags+=@('-DPAKAR_S3_LED_RMT_ENABLED=1')}
 if($wireProfile -like 'static-*'){$flags+=@('-DPAKAR_S3_LED_GPIO_ENABLED=1')}
 if($wireProfile -like 'dual*'){$flags+=@('-DPAKAR_S3_LED_GPIO_ENABLED=1','-DPAKAR_S3_LED_RMT_ENABLED=1')}
 if($wireProfile -eq 'dual-pm'){$flags+='-DCONFIG_PM_ENABLE=1'}
 & g++ -std=c++17 -Wall -Wextra -Werror -pthread @flags -I "$root/tests/led_worker_stubs" -I "$root/libraries/BoardCore/src" `
  "$root/tests/led_worker_test.cpp" "$root/firmware/esp32s3/src/LedOutput.cpp" -o $out
 if($LASTEXITCODE){throw "Actual S3 LED worker $wireProfile fixture compilation failed"}
 Write-Host "Actual worker wire profile: $wireProfile"
 $scenarios=if($wireProfile -eq 'absent'){@('absent')}elseif($wireProfile -like 'dual*'){@('runtime-switch','runtime-resources')}elseif($wireProfile -like '*-calibrated'){@('color-gains')}else{
  @('color-gains','unavailable-saved','pin-failure-1','pin-failure-2','pin-failure-3','cadence','latest-after-hold')+$(if($wireProfile -like 'dma-*'){@('functional','timeout','stop-failure','short-frame')}else{@('static')})
 }
 foreach($scenario in $scenarios){
  & $out $scenario
  if($LASTEXITCODE){throw "Actual S3 LED worker $wireProfile $scenario fixture failed"}
 }
}
