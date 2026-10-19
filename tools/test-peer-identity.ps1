[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'script-runtime.ps1')
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern peer-identity -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern peer-binding -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOut=Join-Path $taskRoot 'build/native'
New-Item -ItemType Directory -Force -Path $taskOut | Out-Null
$taskIncludes=@('-I',(Join-Path $taskRoot 'libraries/BoardCore/src'))
$taskJson=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
foreach($taskMode in @('enabled','disabled')) {
  $taskBinary=Join-Path $taskOut ('peer_compatibility_'+$taskMode+'.exe')
  $taskArgs=@('-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror')+$taskIncludes
  if($taskMode -eq 'disabled'){$taskArgs+='-DPAKAR_ENFORCE_PEER_VERSIONS=0'}
  Invoke-PakarNative -WorkingDirectory $taskRoot -FilePath 'g++' -Arguments ($taskArgs+@('tests/peer_compatibility_test.cpp','-o',$taskBinary))
  Invoke-PakarNative -WorkingDirectory $taskRoot -FilePath $taskBinary
}
$taskBinary=Join-Path $taskOut 'client_telemetry_test.exe'
Invoke-PakarNative -WorkingDirectory $taskRoot -FilePath 'g++' -Arguments (@('-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror')+$taskIncludes+@('-I','tests/access_stubs','-I',$taskJson,'tests/client_telemetry_test.cpp','-o',$taskBinary))
Invoke-PakarNative -WorkingDirectory $taskRoot -FilePath $taskBinary
& (Join-Path $PSScriptRoot 'test-web-access.ps1')
