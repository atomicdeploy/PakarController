[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)] [string] $Source,
    [Parameter(Mandatory=$true)] [string] $StageDirectory,
    [string] $ExpectedCommit,
    [switch] $Probe,
    [switch] $Plan
)
$ErrorActionPreference = 'Stop'
$taskReuse = & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate
. (Join-Path $PSScriptRoot 'windows-build-host.ps1')
. (Join-Path $PSScriptRoot 'package-probe-windows.ps1')
$taskSource = [IO.Path]::GetFullPath($Source)
$taskStage = [IO.Path]::GetFullPath($StageDirectory)
if ([IO.Path]::GetExtension($taskSource) -ne '.exe' -or -not (Test-Path -LiteralPath $taskSource -PathType Leaf)) {
    throw 'An existing built executable is required; staging never rebuilds.'
}
if ($taskStage -eq [IO.Path]::GetPathRoot($taskStage) -or $taskStage -eq [Environment]::GetFolderPath('UserProfile')) {
    throw 'A dedicated staging directory is required.'
}
$taskDestination = Join-Path $taskStage 'PakarController.exe'
# Production replacement remains a separate graceful-IPC workflow.
if ((Split-Path $taskStage -Leaf) -eq 'production') { throw 'Do not use staging to replace a running production app.' }
if ($taskSource -eq $taskDestination) { throw 'Source and stage must be different paths.' }
$taskIdentity = Get-PakarFileIdentity -Path $taskSource
$taskHash = $taskIdentity.sha256
$taskSame = (Test-Path -LiteralPath $taskDestination -PathType Leaf) -and
    (Get-Item -LiteralPath $taskSource).Length -eq (Get-Item -LiteralPath $taskDestination).Length -and
    (Get-FileHash -LiteralPath $taskDestination -Algorithm SHA256).Hash -eq $taskHash
if ($Plan) {
    [pscustomobject]@{Path=$taskDestination; SHA256=$taskHash; Changed=(-not $taskSame); Written=$false}
    return
}
$taskPending = Join-Path $taskStage 'PakarController.exe.pending'
if ($ExpectedCommit) { Assert-PakarPackageSourceStable (Join-Path $PSScriptRoot '..') $ExpectedCommit }
if ($Probe) {
    if ((Get-Item -LiteralPath $taskSource).VersionInfo.ProductName -ne 'PakarController') { throw 'Probe requires Pakar executable identity.' }
    $taskHelp = Invoke-PackageProbe $taskSource '--help' ([IO.Path]::GetDirectoryName($taskSource))
    if ($taskHelp -notmatch 'PakarController') { throw 'Package CLI probe failed identity check.' }
}
if ($taskSame) {
    [pscustomobject]@{Path=$taskDestination; SHA256=$taskHash; Changed=$false; Written=$false}
    return
}
if (Test-Path -LiteralPath $taskPending) { throw 'Pending stage exists; inspect it before retrying.' }
New-Item -ItemType Directory -Force -Path $taskStage | Out-Null
Copy-Item -LiteralPath $taskSource -Destination $taskPending
if ((Get-FileHash -LiteralPath $taskPending -Algorithm SHA256).Hash -ne $taskHash) { throw 'Staged artifact checksum mismatch.' }
if ($ExpectedCommit) { Assert-PakarPackageSourceStable (Join-Path $PSScriptRoot '..') $ExpectedCommit }
Move-Item -LiteralPath $taskPending -Destination $taskDestination -Force
[pscustomobject]@{Path=$taskDestination; SHA256=$taskHash; Changed=$true; Written=$true}
