$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern pu-foundation -Gate -Summary | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/diagnostic_text_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'libraries/BoardCore/src') (Join-Path $taskRoot 'tests/diagnostic_text_test.cpp') -o $taskOutput
if($LASTEXITCODE){throw 'Diagnostic text compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Diagnostic text assertions failed'}
