[CmdletBinding()]
param([double]$CacheWarningGiB = 8, [switch]$FunctionsOnly)

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
