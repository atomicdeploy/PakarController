$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern cooperative-tasks -Gate | Out-Null
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskOutput = Join-Path $taskRoot 'build/native/tasks_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
& g++ -std=c++17 -Wall -Wextra -Werror `
    -I (Join-Path $taskRoot 'tests/task_stubs') `
    -I (Join-Path $taskRoot 'libraries/BoardCore/src') `
    (Join-Path $taskRoot 'tests/tasks_test.cpp') `
    (Join-Path $taskRoot 'libraries/BoardCore/src/Tasks.cpp') -o $taskOutput
if ($LASTEXITCODE -ne 0) { throw 'Cooperative Tasks compilation failed' }
& $taskOutput
if ($LASTEXITCODE -ne 0) { throw 'Cooperative Tasks tests failed' }
