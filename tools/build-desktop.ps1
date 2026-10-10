[CmdletBinding()]
param(
    [ValidateSet('Check', 'Test', 'Release')] [string] $Mode = 'Check',
    [ValidateRange(1, 32)] [int] $Jobs = 2,
    [ValidateRange(1, 1024)] [int] $MinimumFreeGiB = 8,
    [ValidateRange(1, 1024)] [int] $MaximumCacheGiB = 8,
    [string] $ExpectedCommit,
    [switch] $Plan
)
$ErrorActionPreference = 'Stop'
$taskReuse = & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate
& node (Join-Path $PSScriptRoot 'generate-interface-contract.mjs') --check | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0) { throw 'Shared interface catalog is stale.' }
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& node (Join-Path $PSScriptRoot 'check-crate-boundary.mjs') | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0) { throw 'Donor crate/resource guard failed before native build.' }
. (Join-Path $PSScriptRoot 'windows-build-host.ps1')
$taskManifest = Join-Path $taskRoot 'desktop/Cargo.toml'
$taskTarget = Get-PakarCargoTargetDirectory -RepositoryRoot $taskRoot
$taskVerb = if ($Mode -eq 'Check') { 'check' } elseif ($Mode -eq 'Test') { 'test' } else { 'build' }
$taskArguments = @($taskVerb, '--locked', '--manifest-path', $taskManifest,
    '--target-dir', $taskTarget, '--jobs', "$Jobs", '--bin', 'pakarcontroller')
if ($Mode -eq 'Release') { $taskArguments += '--release' }
if ($Plan) {
    [pscustomobject]@{Executable='cargo'; Arguments=$taskArguments; Cache=$taskTarget;
        MinimumFreeGiB=$MinimumFreeGiB; MaximumCacheGiB=$MaximumCacheGiB; ReuseGate=$taskReuse.GateChecked}
    return
}
$taskDrive = Get-PSDrive -PSProvider FileSystem | Where-Object {
    $taskTarget.StartsWith($_.Root, [StringComparison]::OrdinalIgnoreCase) -and $_.Name -ne 'Temp'
} | Sort-Object { $_.Root.Length } -Descending | Select-Object -First 1
if (-not $taskDrive -or $null -eq $taskDrive.Free) { throw 'Cannot verify build-volume free space.' }
if ($taskDrive.Free -lt $MinimumFreeGiB * 1GB) {
    throw "Build refused: less than $MinimumFreeGiB GiB free. Inspect storage before compiling."
}
if (Test-Path -LiteralPath $taskTarget) {
    $taskSize = (Get-ChildItem -LiteralPath $taskTarget -File -Recurse -Force | Measure-Object Length -Sum).Sum
    if ($taskSize -gt $MaximumCacheGiB * 1GB) {
        throw "Build refused: canonical cache exceeds $MaximumCacheGiB GiB. Review obsolete artifacts; do not repeatedly clean/rebuild."
    }
}
# Cargo freshness does the work; never cargo clean or create another target tree.
if ($ExpectedCommit) { Assert-PakarPackageSourceStable $taskRoot $ExpectedCommit }
& cargo @taskArguments
if ($LASTEXITCODE -ne 0) { throw "Desktop $Mode failed" }
if ($ExpectedCommit) { Assert-PakarPackageSourceStable $taskRoot $ExpectedCommit }
