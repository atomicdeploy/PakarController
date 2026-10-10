[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)] [string] $Source,
    [Parameter(Mandatory=$true)] [string] $StageDirectory,
    [switch] $Plan
)
$ErrorActionPreference = 'Stop'
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
$taskHash = (Get-FileHash -LiteralPath $taskSource -Algorithm SHA256).Hash
$taskSame = (Test-Path -LiteralPath $taskDestination -PathType Leaf) -and
    (Get-Item -LiteralPath $taskSource).Length -eq (Get-Item -LiteralPath $taskDestination).Length -and
    (Get-FileHash -LiteralPath $taskDestination -Algorithm SHA256).Hash -eq $taskHash
if ($Plan -or $taskSame) {
    [pscustomobject]@{Path=$taskDestination; SHA256=$taskHash; Changed=(-not $taskSame); Written=$false}
    return
}
$taskPending = Join-Path $taskStage 'PakarController.exe.pending'
if (Test-Path -LiteralPath $taskPending) { throw 'Pending stage exists; inspect it before retrying.' }
New-Item -ItemType Directory -Force -Path $taskStage | Out-Null
Copy-Item -LiteralPath $taskSource -Destination $taskPending
if ((Get-FileHash -LiteralPath $taskPending -Algorithm SHA256).Hash -ne $taskHash) { throw 'Staged artifact checksum mismatch.' }
Move-Item -LiteralPath $taskPending -Destination $taskDestination -Force
[pscustomobject]@{Path=$taskDestination; SHA256=$taskHash; Changed=$true; Written=$true}
