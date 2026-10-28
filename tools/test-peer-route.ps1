$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern bridge-repeater -Summary | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/peer_route_policy_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'libraries/BoardCore/src') (Join-Path $taskRoot 'tests/peer_route_policy_test.cpp') -o $taskOutput
if($LASTEXITCODE){throw 'Peer route test compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Peer route assertions failed'}
$taskJson=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
$taskRelayOutput=Join-Path $taskRoot 'build/native/peer_relay_test.exe'
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'tests/peer_relay_stubs') -I $taskJson -I (Join-Path $taskRoot 'libraries/BoardCore/src') (Join-Path $taskRoot 'tests/peer_relay_test.cpp') -o $taskRelayOutput
if($LASTEXITCODE){throw 'Actual peer relay fixture compilation failed'}
& $taskRelayOutput
if($LASTEXITCODE){throw 'Actual peer relay fixture failed'}
& node (Join-Path $taskRoot 'tests/peer_relay_probe_test.mjs')
if($LASTEXITCODE){throw 'Peer relay acceptance probe fixtures failed'}
