[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern games-streaming -Summary | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/key_minigame_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
$taskArgs=@('-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror','-I',(Join-Path $taskRoot 'libraries/BoardCore/src'),(Join-Path $taskRoot 'tests/key_minigame_test.cpp'),'-o',$taskOutput)
& g++ @taskArgs
if($LASTEXITCODE){throw 'Minigame core compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Minigame assertions failed'}
$taskExcluded=@('ILLUMINATED','COLOR_MATCH','SIMON','MEMORY_PAIRS','CLEAR_ALL','COLOR_ORDER','SWITCH_PATTERN','REACTION_CHASE','TOGGLE') | ForEach-Object {"-DPAKAR_GAME_$_=0"}
& g++ @taskArgs @taskExcluded -DTEST_EXCLUDED_MODES -Wno-unused-function
if($LASTEXITCODE){throw 'Mode-exclusion compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Mode-exclusion assertions failed'}
