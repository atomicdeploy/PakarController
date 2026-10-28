[CmdletBinding()]
param([string]$LibSshPath=(Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/LibSSH-ESP32/src'))
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern device-shell -Summary | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
if(-not(Test-Path -LiteralPath (Join-Path $LibSshPath 'libssh/server.h'))){throw 'Install pinned LibSSH-ESP32 before the explicit API compile check'}
$taskProperties=Get-Content -LiteralPath (Join-Path (Split-Path $LibSshPath -Parent) 'library.properties') -Raw
if($taskProperties -notmatch '(?m)^version=5\.10\.0\s*$'){throw 'This compile check requires the reviewed LibSSH-ESP32 5.10.0 pin'}
$taskOutput=Join-Path $taskRoot 'build/native/ssh_policy_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path $taskOutput -Parent)|Out-Null
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror (Join-Path $taskRoot 'tests/ssh_policy_test.cpp') -o $taskOutput
if($LASTEXITCODE){throw 'SSH policy fixture compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'SSH policy fixture failed'}
$taskScpOutput=Join-Path $taskRoot 'build/native/scp_download_test.exe'
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror (Join-Path $taskRoot 'tests/scp_download_test.cpp') -o $taskScpOutput
if($LASTEXITCODE){throw 'SCP state fixture compilation failed'}
& $taskScpOutput
if($LASTEXITCODE){throw 'SCP state fixture failed'}
Push-Location $taskRoot
try {
  & node tests/scp_client_test.mjs
  if($LASTEXITCODE){throw 'OpenSSH SCP protocol compatibility failed'}
} finally {Pop-Location}
& g++ -std=c++17 -fsyntax-only -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-deprecated-declarations '-I' (Join-Path $taskRoot 'tests/ssh_stubs') '-I' (Join-Path $taskRoot 'libraries/BoardCore/src') '-I' $LibSshPath (Join-Path $taskRoot 'tests/ssh_transport_api_test.cpp')
if($LASTEXITCODE){throw 'Actual SSH transport does not compile against the pinned libssh API'}
& g++ -std=c++17 -DPAKAR_SCP_ENABLED=1 -fsyntax-only -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-deprecated-declarations '-I' (Join-Path $taskRoot 'tests/ssh_stubs') '-I' (Join-Path $taskRoot 'libraries/BoardCore/src') '-I' $LibSshPath (Join-Path $taskRoot 'tests/ssh_transport_api_test.cpp')
if($LASTEXITCODE){throw 'Actual SCP-enabled SSH transport API compile failed'}
$taskSettingsOutput=Join-Path $taskRoot 'build/native/ssh_settings_test.exe'
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-deprecated-declarations '-I' (Join-Path $taskRoot 'tests/ssh_stubs') '-I' (Join-Path $taskRoot 'libraries/BoardCore/src') '-I' $LibSshPath (Join-Path $taskRoot 'tests/ssh_settings_test.cpp') -o $taskSettingsOutput
if($LASTEXITCODE){throw 'Actual SSH settings fixture compilation failed'}
& $taskSettingsOutput
if($LASTEXITCODE){throw 'Actual SSH settings fixture failed'}
$taskJson=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
$taskBindingOutput=Join-Path $taskRoot 'build/native/ssh_scp_binding_test.exe'
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -Wno-misleading-indentation '-I' (Join-Path $taskRoot 'tests/file_stubs') '-I' (Join-Path $taskRoot 'tests/access_stubs') '-I' $taskJson '-I' (Join-Path $taskRoot 'firmware/esp/src') (Join-Path $taskRoot 'tests/ssh_scp_binding_test.cpp') -o $taskBindingOutput
if($LASTEXITCODE){throw 'Actual SCP/public file binding compilation failed'}
& $taskBindingOutput
if($LASTEXITCODE){throw 'Actual SCP/public file binding fixture failed'}
Write-Host 'SSH policy and production transport API compile checks passed; no SSH handshake, target link or hardware acceptance implied.'
