$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern tm1637-display -Gate | Out-Null
$taskNative=Join-Path $taskRoot 'build/native'
New-Item -ItemType Directory -Force -Path $taskNative | Out-Null
& g++ -std=c++17 -Wall -Wextra -Werror (Join-Path $taskRoot 'tests/semantic_panel_test.cpp') -o (Join-Path $taskNative 'semantic_panel_test.exe')
if($LASTEXITCODE){throw 'Semantic panel fixture compilation failed'}
& (Join-Path $taskNative 'semantic_panel_test.exe')
if($LASTEXITCODE){throw 'Semantic panel fixture failed'}
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'tests/stubs') -I (Join-Path $taskRoot 'libraries/BoardCore/src') -I (Join-Path $taskRoot 'firmware/avr') (Join-Path $taskRoot 'tests/front_panel_test.cpp') (Join-Path $taskRoot 'firmware/avr/FrontPanel.cpp') -o (Join-Path $taskNative 'front_panel_test.exe')
if($LASTEXITCODE){throw 'AVR presentation fixture compilation failed'}
& (Join-Path $taskNative 'front_panel_test.exe')
if($LASTEXITCODE){throw 'AVR presentation fixture failed'}
$taskSources=@('firmware/avr/PcControllerRuntime.cpp','firmware/avr/BoardSettings.cpp','firmware/avr/SevenSegments.cpp','libraries/BoardCore/src/pccontroller/MacroRing.cpp','libraries/BoardCore/src/pccontroller/Keys.cpp','libraries/BoardCore/src/pccontroller/ControllerEvents.cpp','libraries/BoardCore/src/pccontroller/AutomationStore.cpp') | ForEach-Object { Join-Path $taskRoot $_ }
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'tests/pc_stubs') -I (Join-Path $taskRoot 'libraries/BoardCore/src') -I (Join-Path $taskRoot 'firmware/avr') -I (Join-Path $taskRoot 'firmware/esp/src') (Join-Path $taskRoot 'tests/esp_front_panel_test.cpp') @taskSources -o (Join-Path $taskNative 'esp_front_panel_test.exe')
if($LASTEXITCODE){throw 'Paired menu fixture compilation failed'}
& (Join-Path $taskNative 'esp_front_panel_test.exe')
if($LASTEXITCODE){throw 'Paired menu fixture failed'}
