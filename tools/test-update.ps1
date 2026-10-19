[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern remote-ota -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/update_progress_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
foreach($taskPlatform in @(@(),@('-DARDUINO_ARCH_ESP8266','-I',(Join-Path $taskRoot 'tests/atomic_stubs')))) {
& g++ -std=c++17 -pthread -O2 -g0 -Wall -Wextra -Werror @taskPlatform (Join-Path $taskRoot 'tests/update_progress_test.cpp') -o $taskOutput
if($LASTEXITCODE){throw 'Update progress fixture compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Update progress fixture failed'}
}
$taskPanelOutput=Join-Path $taskRoot 'build/native/front_panel_test.exe'
foreach($taskFixture in @('upgrade_lifecycle_test','local_ota_receipt_test')) {
  $taskFixtureOutput=Join-Path $taskRoot ('build/native/'+$taskFixture+'.exe')
  $taskJson=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
  & g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 -I (Join-Path $taskRoot 'tests/remote_stubs') -I $taskJson (Join-Path $taskRoot ('tests/'+$taskFixture+'.cpp')) -o $taskFixtureOutput
  if($LASTEXITCODE){throw "$taskFixture compilation failed"}
  & $taskFixtureOutput
  if($LASTEXITCODE){throw "$taskFixture assertions failed"}
}
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'tests/stubs') -I (Join-Path $taskRoot 'libraries/BoardCore/src') -I (Join-Path $taskRoot 'firmware/avr') (Join-Path $taskRoot 'tests/front_panel_test.cpp') (Join-Path $taskRoot 'firmware/avr/FrontPanel.cpp') -o $taskPanelOutput
if($LASTEXITCODE){throw 'Actual AVR progress presentation fixture compilation failed'}
& $taskPanelOutput
if($LASTEXITCODE){throw 'Actual AVR progress presentation fixture failed'}
& node (Join-Path $taskRoot 'tests/update_tool_test.mjs')
if($LASTEXITCODE){throw 'Automatic updater fixture failed'}
& node (Join-Path $taskRoot 'tests/update_recovery_test.mjs')
if($LASTEXITCODE){throw 'Explicit maintenance recovery fixture failed'}
& node (Join-Path $taskRoot 'tests/upgrade_web_test.mjs')
if($LASTEXITCODE){throw 'Actual Web lifecycle parser fixture failed'}
