param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern firmware-hardware | Out-Null
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'build/native/led_worker_test.exe'
New-Item -ItemType Directory -Force (Split-Path -Parent $out) | Out-Null
foreach($profile in @('static-bgr','static-rgb','dma-bgr','dma-rgb')){
 $flags=@();if($profile -like '*-rgb'){$flags+=@('-DPAKAR_S3_LED_SWAP_RED_BLUE=0')}
 if($profile -like 'dma-*'){$flags+=@('-DPAKAR_S3_LED_USE_DMA=1')}
 & g++ -std=c++17 -Wall -Wextra -Werror -pthread @flags -I "$root/tests/led_worker_stubs" `
  "$root/tests/led_worker_test.cpp" "$root/firmware/esp32s3/src/LedOutput.cpp" -o $out
 if($LASTEXITCODE){throw "Actual S3 LED worker $profile fixture compilation failed"}
 Write-Host "Actual worker wire profile: $profile"
 $scenarios=if($profile -like 'dma-*'){@('functional','timeout','stop-failure','short-frame')}else{@('static')}
 foreach($scenario in $scenarios){
  & $out $scenario
  if($LASTEXITCODE){throw "Actual S3 LED worker $profile $scenario fixture failed"}
 }
}
