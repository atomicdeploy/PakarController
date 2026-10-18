param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern avr-boot-chain -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern avr-programming -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern api-contract -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/urboot_protocol_test.exe'
New-Item -ItemType Directory -Force (Split-Path -Parent $taskOutput) | Out-Null
& g++ -std=c++17 -Wall -Wextra -Werror (Join-Path $taskRoot 'tests/urboot_protocol_test.cpp') -o $taskOutput
if($LASTEXITCODE){throw 'Actual Urboot protocol fixture compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Actual Urboot protocol fixture failed'}
$taskIsp=Join-Path $taskRoot 'build/native/avr_isp_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'tests/isp_stubs') (Join-Path $taskRoot 'tests/avr_isp_test.cpp') (Join-Path $taskRoot 'firmware/esp8266/src/AvrIsp.cpp') -o $taskIsp
if($LASTEXITCODE){throw 'Actual ISP callback fixture compilation failed'}
& $taskIsp
if($LASTEXITCODE){throw 'Actual ISP preservation/callback fixture failed'}
& node (Join-Path $taskRoot 'tests/urboot_integration_test.mjs')
if($LASTEXITCODE){throw 'Urboot ownership/API integration guard failed'}
