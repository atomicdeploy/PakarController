$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern tm1637-display -Gate | Out-Null
$taskOutput=Join-Path $taskRoot 'build/native/tm1637_bus_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path $taskOutput -Parent) | Out-Null
# One tiny output reused for fallback and actual AVR register-branch fixtures.
foreach ($taskVariant in @('fallback','avr')) {
    [string[]]$taskFlags=@()
    if($taskVariant -eq 'avr'){$taskFlags=@('-D__AVR__')}
    & g++ -std=c++17 -Wall -Wextra -Werror @taskFlags -I (Join-Path $taskRoot 'tests/display_stubs') -I (Join-Path $taskRoot 'firmware/avr') (Join-Path $taskRoot 'tests/tm1637_bus_test.cpp') (Join-Path $taskRoot 'firmware/avr/SevenSegments.cpp') -o $taskOutput
    if($LASTEXITCODE -ne 0){throw "TM1637 $taskVariant fixture compilation failed"}
    & $taskOutput
    if($LASTEXITCODE -ne 0){throw "TM1637 $taskVariant fixture failed"}
}
