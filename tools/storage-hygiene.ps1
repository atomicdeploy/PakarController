[CmdletBinding()]
param([double]$CacheWarningGiB = 8, [switch]$FunctionsOnly,
    [switch]$PruneObsoleteDebug, [switch]$Plan)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Actual Get-DirectorySummary from Pealayer scripts/storage-hygiene.ps1.
# Adaptation: empty directories return zero; read errors fail rather than hide
# unmeasured storage. Logical sizes are not physical disk usage or SSD wear.
function Get-DirectorySummary([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        return [pscustomobject]@{ Path = $Path; Files = 0; GiB = 0; Exists = $false }
    }
    $measurement = Get-ChildItem -LiteralPath $Path -File -Recurse -Force -ErrorAction Stop |
        Measure-Object -Property Length -Sum
    $bytes = if ($measurement.Count) { $measurement.Sum } else { 0 }
    [pscustomobject]@{
        Path = $Path
        Files = $measurement.Count
        GiB = [math]::Round($bytes / 1GB, 3)
        Exists = $true
    }
}
if ($FunctionsOnly) { return }
if ($CacheWarningGiB -le 0) { throw 'A positive cache warning budget is required.' }
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'windows-build-host.ps1')
if ($PruneObsoleteDebug) {
    # Adapt Pealayer's exact-path/ignored/unknown-child/process prune gates.
    # Never prune the target root: release and rollback are not debug artifacts.
    $taskCanonical = [IO.Path]::GetFullPath((Join-Path $taskRoot 'desktop/target'))
    if ((Get-PakarCargoTargetDirectory $taskRoot) -ne $taskCanonical) {
        throw 'Cleanup only supports the owned canonical desktop/target; inspect any override.'
    }
    $taskDebug = Join-Path $taskCanonical 'debug'
    if (-not (Test-Path -LiteralPath $taskDebug)) { return }
    $taskResolved = (Resolve-Path -LiteralPath $taskDebug).Path
    if ($taskResolved -ne $taskDebug) { throw 'Unexpected resolved debug target.' }
    foreach ($taskAncestor in @($taskRoot, (Join-Path $taskRoot 'desktop'), $taskCanonical)) {
        if ((Get-Item -LiteralPath $taskAncestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw 'Cleanup refuses reparse-point ancestors.'
        }
    }
    $taskItems = @(Get-ChildItem -LiteralPath $taskDebug -Recurse -Force)
    if ((Get-Item -LiteralPath $taskDebug -Force).Attributes -band [IO.FileAttributes]::ReparsePoint -or
        @($taskItems | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) {
        throw 'Cleanup refuses reparse points.'
    }
    & git -C $taskRoot check-ignore --quiet -- desktop/target/debug
    if ($LASTEXITCODE -ne 0) { throw 'Debug artifacts must be ignored.' }
    if (@(& git -C $taskRoot ls-files -- desktop/target/debug).Count) { throw 'Tracked debug files must be preserved.' }
    $taskAllowed = @('.fingerprint','build','deps','examples','incremental',
        '.cargo-artifact-lock','.cargo-build-lock','.cargo-lock','pakarcontroller.exe','pakarcontroller.d','pakarcontroller.pdb')
    $taskUnexpected = @(Get-ChildItem -LiteralPath $taskDebug -Force | Where-Object Name -notin $taskAllowed)
    if ($taskUnexpected.Count) { throw "Preserve unknown debug-root evidence: $($taskUnexpected.Name -join ', ')" }
    $taskActive = @(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -in @('cargo.exe','rustc.exe','rustdoc.exe','cl.exe','link.exe') -or
        ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($taskDebug+'\',[StringComparison]::OrdinalIgnoreCase)) -or
        ($_.CommandLine -and $_.CommandLine.Contains($taskDebug))
    })
    if ($taskActive.Count) { throw "Debug cleanup blocked by active owner/compiler: $($taskActive.ProcessId -join ', ')" }
    $taskBefore = Get-DirectorySummary $taskDebug
    if ($Plan) { return [pscustomobject]@{Target=$taskDebug;Files=$taskBefore.Files;LogicalGiB=$taskBefore.GiB;Deleted=$false} }
    Remove-Item -LiteralPath $taskDebug -Recurse -Force
    [pscustomobject]@{Target=$taskDebug;Files=$taskBefore.Files;LogicalGiB=$taskBefore.GiB;Deleted=$true}
    return
}
$taskCache = Get-DirectorySummary (Get-PakarCargoTargetDirectory $taskRoot)
$taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskRoot '../../outputs/PakarController'))
@(
    $taskCache
    Get-DirectorySummary (Join-Path $taskRoot 'build/cache')
    Get-DirectorySummary (Join-Path $taskRoot 'mobile/build')
    Get-DirectorySummary (Join-Path $taskOutputRoot 'staging')
) | Format-List Path,Files,GiB,Exists
if ($taskCache.GiB -gt $CacheWarningGiB) {
    Write-Warning "Pakar Cargo output is $($taskCache.GiB) logical GiB; inspect profiles/owners before any explicitly authorized cleanup."
}
# No prune parameter: blocked cleanup must not be rerouted through this import.
