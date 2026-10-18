$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern board-silent -Gate | Out-Null
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskOutput = Join-Path $taskRoot 'build/native'
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
$taskTone = Join-Path $taskOutput 'tone_player_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -DF_CPU=16000000UL `
    -I (Join-Path $taskRoot 'libraries/BoardCore/src') -I (Join-Path $taskRoot 'tests/tone_stubs') -I (Join-Path $taskRoot 'firmware/avr') `
    (Join-Path $taskRoot 'tests/tone_player_test.cpp') (Join-Path $taskRoot 'firmware/avr/TonePlayer.cpp') (Join-Path $taskRoot 'firmware/avr/BootMelody.cpp') -o $taskTone
if ($LASTEXITCODE) { throw 'Board sound fixture compilation failed' }
& $taskTone
if ($LASTEXITCODE) { throw 'Board sound fixture failed' }
$taskContract = Join-Path $taskOutput 'pc_contract_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror `
    -I (Join-Path $taskRoot 'tests/pc_stubs') -I (Join-Path $taskRoot 'libraries/BoardCore/src') -I (Join-Path $taskRoot 'firmware/avr') `
    (Join-Path $taskRoot 'tests/pc_contract_test.cpp') (Join-Path $taskRoot 'firmware/avr/PcControllerRuntime.cpp') `
    (Join-Path $taskRoot 'firmware/avr/BoardSettings.cpp') (Join-Path $taskRoot 'firmware/avr/SevenSegments.cpp') `
    (Join-Path $taskRoot 'libraries/BoardCore/src/pccontroller/MacroRing.cpp') (Join-Path $taskRoot 'libraries/BoardCore/src/pccontroller/Keys.cpp') `
    (Join-Path $taskRoot 'libraries/BoardCore/src/pccontroller/ControllerEvents.cpp') (Join-Path $taskRoot 'libraries/BoardCore/src/pccontroller/AutomationStore.cpp') -o $taskContract
if ($LASTEXITCODE) { throw 'Board settings fixture compilation failed' }
& $taskContract
if ($LASTEXITCODE) { throw 'Board settings fixture failed' }
& node (Join-Path $taskRoot 'tests/board_silent_test.mjs')
if ($LASTEXITCODE) { throw 'Board sound Web transaction fixture failed' }
