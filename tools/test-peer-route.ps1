$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern bridge-repeater -Summary | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/peer_route_policy_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'libraries/BoardCore/src') (Join-Path $taskRoot 'tests/peer_route_policy_test.cpp') -o $taskOutput
if($LASTEXITCODE){throw 'Peer route test compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Peer route assertions failed'}
