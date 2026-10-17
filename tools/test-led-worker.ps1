param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern firmware-hardware | Out-Null
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'build/native/led_worker_test.exe'
New-Item -ItemType Directory -Force (Split-Path -Parent $out) | Out-Null
foreach($profile in @('default-bgr','optional-rgb')){
 $flags=@();if($profile -eq 'optional-rgb'){$flags=@('-DPAKAR_S3_LED_SWAP_RED_BLUE=0')}
 & g++ -std=c++17 -Wall -Wextra -Werror -pthread @flags -I "$root/tests/led_worker_stubs" `
  "$root/tests/led_worker_test.cpp" "$root/firmware/esp32s3/src/LedOutput.cpp" -o $out
 if($LASTEXITCODE){throw "Actual S3 LED worker $profile fixture compilation failed"}
 Write-Host "Actual worker wire profile: $profile"
 foreach($scenario in @('functional','timeout','stop-failure')){
  & $out $scenario
  if($LASTEXITCODE){throw "Actual S3 LED worker $profile $scenario fixture failed"}
 }
}
