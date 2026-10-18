param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern api-contract -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern web-presentation -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/async_http_test.exe'
New-Item -ItemType Directory -Force (Split-Path -Parent $taskOutput) | Out-Null
& g++ -std=c++17 -pthread -Wall -Wextra -Werror -I (Join-Path $taskRoot 'tests/access_stubs') `
  -I (Join-Path $taskRoot 'firmware/esp8266/src') (Join-Path $taskRoot 'tests/async_http_test.cpp') -o $taskOutput
if($LASTEXITCODE){throw 'Actual async HTTP actor fixture compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Actual async HTTP actor fixture failed'}
