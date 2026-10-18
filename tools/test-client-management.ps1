[CmdletBinding()]
param([switch]$Restore)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern client-management -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
Push-Location $taskRoot
try {
  if($Restore){
    & npm ci --prefix services/client-management --ignore-scripts --no-audit --no-fund
    if($LASTEXITCODE -ne 0){throw 'Locked service dependency restoration failed.'}
  }
  & node --test services/client-management/service.test.mjs
  if($LASTEXITCODE -ne 0){throw 'Actual client management service regression tests failed.'}
} finally {Pop-Location}
