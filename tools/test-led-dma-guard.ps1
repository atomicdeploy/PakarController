param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern firmware-hardware | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/led_dma_guard_test.exe'
foreach($taskProfile in @('supported','unsupported')){
  $taskFlags=if($taskProfile -eq 'unsupported'){@('-DPAKAR_DMA_GUARD_UNSUPPORTED_TEST=1')}else{@()}
  $taskArgs=@('-std=c++17','-Wall','-Wextra','-Werror','-pthread')+$taskFlags+@(
    '-I',(Join-Path $taskRoot 'tests/led_dma_guard_stubs'),
    (Join-Path $taskRoot 'tests/led_dma_guard_test.cpp'),
    (Join-Path $taskRoot 'tests/led_dma_guard_runtime.cpp'),
    (Join-Path $taskRoot 'firmware/esp32s3/src/LedDmaGuard.cpp'),
    '-Wl,--wrap=gdma_new_ahb_channel','-o',$taskOutput)
  if($Plan){[pscustomobject]@{Profile=$taskProfile;Executable='g++';Arguments=$taskArgs};continue}
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
  & g++ @taskArgs
  if($LASTEXITCODE){throw "Scoped LED DMA $taskProfile fixture compilation failed"}
  & $taskOutput
  if($LASTEXITCODE){throw "Scoped LED DMA $taskProfile fixture failed"}
}
