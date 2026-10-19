param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern device-shell -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/shell_properties_test.exe'
foreach($taskProfile in @('ordinary','esp8266','esp32s3')){
  $taskFlags=if($taskProfile -eq 'esp8266'){@('-DARDUINO_ARCH_ESP8266=1')}elseif($taskProfile -eq 'esp32s3'){@('-DARDUINO_ARCH_ESP32=1')}else{@()}
  $taskArgs=@('-std=c++17','-Wall','-Wextra','-Werror')+$taskFlags+@('-I',(Join-Path $taskRoot 'tests/shell_properties_stubs'),(Join-Path $taskRoot 'tests/shell_properties_test.cpp'),'-o',$taskOutput)
  if($Plan){[pscustomobject]@{Profile=$taskProfile;Executable='g++';Arguments=$taskArgs};continue}
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
  & g++ @taskArgs
  if($LASTEXITCODE){throw "Shell metadata $taskProfile fixture compilation failed"}
  & $taskOutput
  if($LASTEXITCODE){throw "Shell metadata $taskProfile fixture failed"}
}
