[CmdletBinding()]
param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern led-cell-test | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskProvenance=Get-Content -LiteralPath (Join-Path $taskRoot 'tests/idf_rmt_ring_provenance.json') -Raw | ConvertFrom-Json
$taskExcerpt=Join-Path $taskRoot 'tests/idf_rmt_ring_sdk.inc'
if((Get-FileHash -LiteralPath $taskExcerpt -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskProvenance.excerpt_sha256){
  throw 'Pinned IDF RMT excerpt hash changed; review exact upstream source before updating provenance'
}
$taskOutput=Join-Path $taskRoot 'build/native/idf_rmt_ring_test.exe'
foreach($taskOrder in @('bgr','rgb')){
  $taskFlags=if($taskOrder -eq 'rgb'){@('-DPAKAR_S3_LED_SWAP_RED_BLUE=0')}else{@()}
  $taskArgs=@('-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror')+$taskFlags+@(
    (Join-Path $taskRoot 'tests/idf_rmt_ring_test.cpp'),'-o',$taskOutput)
  if($Plan){[pscustomobject]@{Profile=$taskOrder;Executable='g++';Arguments=$taskArgs;Meaning='Actual pinned IDF functions plus host descriptor model, not hardware'};continue}
  New-Item -ItemType Directory -Force (Split-Path -Parent $taskOutput) | Out-Null
  & g++ @taskArgs
  if($LASTEXITCODE){throw "IDF RMT ring $taskOrder fixture compilation failed"}
  & $taskOutput
  if($LASTEXITCODE){throw "IDF RMT ring $taskOrder fixture failed"}
}
