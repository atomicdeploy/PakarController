param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern network-cloud | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern terminal-ui | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern device-shell -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
foreach($taskName in @('telnet_shell','shell_tui','shell_wire')){
  $taskOutput=Join-Path $taskRoot "build/native/${taskName}_test.exe"
  $taskArgs=@('-std=c++17','-Wall','-Wextra','-Werror','-I',(Join-Path $taskRoot 'firmware/esp8266/src'),'-I',(Join-Path $taskRoot 'tests/access_stubs'),(Join-Path $taskRoot "tests/${taskName}_test.cpp"),'-o',$taskOutput)
  if($Plan){[pscustomobject]@{Executable='g++';Arguments=$taskArgs};continue}
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
  & g++ @taskArgs
  if($LASTEXITCODE){throw "$taskName fixture compilation failed"}
  & $taskOutput
  if($LASTEXITCODE){throw "$taskName fixture failed"}
}
$taskAccess=Join-Path $taskRoot 'build/native/access_config_test.exe'
$taskAccessArgs=@('-std=c++17','-Wall','-Wextra','-Werror','-I',(Join-Path $taskRoot 'tests/access_stubs'),'-I',(Join-Path $taskRoot 'libraries/BoardCore/src'),'-I',(Join-Path $taskRoot 'firmware/esp8266/src'),(Join-Path $taskRoot 'tests/access_config_test.cpp'),(Join-Path $taskRoot 'firmware/esp8266/src/AccessConfig.cpp'),'-o',$taskAccess)
$taskDiscovery=Join-Path $taskRoot 'build/native/discovery_mdns_test.exe'
$taskDiscoveryArgs=@('-std=c++17','-Wall','-Wextra','-Werror','-I',(Join-Path $taskRoot 'libraries/BoardCore/src'),(Join-Path $taskRoot 'tests/discovery_mdns_test.cpp'),'-o',$taskDiscovery)
if($Plan){[pscustomobject]@{Executable='g++';Arguments=$taskAccessArgs};[pscustomobject]@{Executable='g++';Arguments=$taskDiscoveryArgs};return}
& g++ @taskAccessArgs
if($LASTEXITCODE){throw 'Shell authority persistence fixture compilation failed'}
& $taskAccess
if($LASTEXITCODE){throw 'Shell authority persistence fixture failed'}
& g++ @taskDiscoveryArgs
if($LASTEXITCODE){throw 'Shell mDNS fixture compilation failed'}
& $taskDiscovery
if($LASTEXITCODE){throw 'Shell mDNS fixture failed'}
& node (Join-Path $taskRoot 'tests/shell_service_test.mjs')
if($LASTEXITCODE){throw 'Actual shell service lease fixture failed'}
