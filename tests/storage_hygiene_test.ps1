$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
function Assert-Hygiene($condition, $message) { if (-not $condition) { throw $message } }
. (Join-Path $taskRoot 'tools/storage-hygiene.ps1') -FunctionsOnly
. (Join-Path $taskRoot 'tools/windows-build-host.ps1')
$taskAssets = Join-Path $taskRoot 'assets/themes'
$taskFiles = @(Get-ChildItem -LiteralPath $taskAssets -File -Recurse -Force)
$taskBefore = @($taskFiles | Select-Object FullName,LastWriteTimeUtc) | ConvertTo-Json
$taskSummary = Get-DirectorySummary $taskAssets
Assert-Hygiene ($taskSummary.Exists -and $taskSummary.Files -eq $taskFiles.Count) 'Actual imported recursive summary'
Assert-Hygiene ($taskSummary.GiB -eq [math]::Round(($taskFiles | Measure-Object Length -Sum).Sum / 1GB,3)) 'Logical byte sum'
$taskMissing = Get-DirectorySummary (Join-Path $taskRoot 'tests/nonexistent-storage-fixture')
Assert-Hygiene (-not $taskMissing.Exists -and $taskMissing.Files -eq 0 -and $taskMissing.GiB -eq 0) 'Missing directory stays honest'
$taskPalette = Join-Path $taskAssets 'palettes.json'
Assert-Hygiene (Test-PakarFileContentEqual $taskPalette $taskPalette) 'Actual imported equal-content helper'
Assert-Hygiene (-not (Test-PakarFileContentEqual $taskPalette (Join-Path $taskAssets 'appearance.js'))) 'Different file rejection'
Assert-Hygiene (-not (Test-PakarFileContentEqual $taskPalette (Join-Path $taskRoot 'tests/nonexistent-storage-fixture'))) 'Missing file rejection'
$taskAfter = @(Get-ChildItem -LiteralPath $taskAssets -File -Recurse -Force | Select-Object FullName,LastWriteTimeUtc) | ConvertTo-Json
Assert-Hygiene ($taskBefore -eq $taskAfter) 'Inspection must not rewrite assets'
$taskSource = Get-Content (Join-Path $taskRoot 'tools/storage-hygiene.ps1') -Raw
Assert-Hygiene ($taskSource -notmatch '\b(Remove-Item|Copy-Item|Move-Item|Set-Content|WriteAllText)\b') 'Read-only import must never become an alternate cleanup path'
Write-Host 'Actual donor storage summary and content-equality tests passed; no compiler, payload copy or cleanup.'
