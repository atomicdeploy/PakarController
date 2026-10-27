[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern drs-snake -Summary | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/snake_game_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'libraries/BoardCore/src') (Join-Path $taskRoot 'tests/snake_game_test.cpp') -o $taskOutput
if($LASTEXITCODE){throw 'Snake core compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Snake core assertions failed'}
& node --test (Join-Path $taskRoot 'tests/snake_host_test.mjs')
if($LASTEXITCODE){throw 'Snake host assertions failed'}
