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
$taskEmptyPath = Join-Path $taskRoot 'build/tests/storage-hygiene/empty'
New-Item -ItemType Directory -Path $taskEmptyPath -Force | Out-Null
$taskEmpty = Get-DirectorySummary $taskEmptyPath
Assert-Hygiene ($taskEmpty.Exists -and $taskEmpty.Files -eq 0 -and $taskEmpty.GiB -eq 0) 'Empty directory works under strict mode in Windows PowerShell and pwsh'
$taskPalette = Join-Path $taskAssets 'palettes.json'
Assert-Hygiene (Test-PakarFileContentEqual $taskPalette $taskPalette) 'Actual imported equal-content helper'
Assert-Hygiene (-not (Test-PakarFileContentEqual $taskPalette (Join-Path $taskAssets 'appearance.js'))) 'Different file rejection'
Assert-Hygiene (-not (Test-PakarFileContentEqual $taskPalette (Join-Path $taskRoot 'tests/nonexistent-storage-fixture'))) 'Missing file rejection'
$taskAfter = @(Get-ChildItem -LiteralPath $taskAssets -File -Recurse -Force | Select-Object FullName,LastWriteTimeUtc) | ConvertTo-Json
Assert-Hygiene ($taskBefore -eq $taskAfter) 'Inspection must not rewrite assets'
$taskSource = Get-Content (Join-Path $taskRoot 'tools/storage-hygiene.ps1') -Raw
Assert-Hygiene ($taskSource -match 'if \(\$PruneObsoleteDebug\)') 'Destruction requires explicit user-authorized debug-only opt-in'
Assert-Hygiene ($taskSource -match 'Remove-Item -LiteralPath \$taskDebug -Recurse -Force') 'Only the validated exact debug child is removed'
Assert-Hygiene ($taskSource -notmatch 'Remove-Item -LiteralPath \$(taskRoot|taskCanonical|taskTarget)\b') 'Release/cache/source roots cannot be cleanup targets'
foreach($taskGate in @('ReparsePoint','check-ignore','ls-files','taskUnexpected','taskActive','if ($Plan)')){
    Assert-Hygiene ($taskSource.Contains($taskGate)) "Mandatory donor cleanup gate: $taskGate"
}
$taskReadOnly=$taskSource.Substring($taskSource.IndexOf('$taskCache ='))
Assert-Hygiene ($taskReadOnly -notmatch '\b(Remove-Item|Copy-Item|Move-Item|Set-Content|WriteAllText)\b') 'Default inventory stays read-only'
$taskPreviousTarget=$env:CARGO_TARGET_DIR
try{
    $env:CARGO_TARGET_DIR=Join-Path $taskRoot 'mobile/build'
    $taskRejected=$false
    try{& (Join-Path $taskRoot 'tools/storage-hygiene.ps1') -PruneObsoleteDebug -Plan}catch{
        $taskRejected=$_.Exception.Message -match 'owned canonical desktop/target'
    }
    Assert-Hygiene $taskRejected 'Real cleanup dispatch refuses an override before traversal/deletion'
}finally{
    [Environment]::SetEnvironmentVariable('CARGO_TARGET_DIR',$taskPreviousTarget,'Process')
}
Write-Host 'Actual donor storage summary and content-equality tests passed; no compiler, payload copy or cleanup.'
