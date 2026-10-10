[CmdletBinding()]
param(
    [ValidateSet('Analyze','Test','ApkPreview')][string]$Mode='Test',
    [string]$FlutterPath='C:\development\flutter\bin\flutter.bat',
    [ValidateRange(1,1024)][int]$MinimumFreeGiB=8,
    [ValidateRange(1,1024)][int]$MaximumCacheGiB=4,
    [switch]$Restore,
    [string]$CapturePath,
    [switch]$Plan
)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskMobile=Join-Path $taskRoot 'mobile'
$taskContext=& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern device-context -Gate
$taskStorage=& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate
foreach($taskGenerator in @('generate-font-assets.mjs','generate-interface-contract.mjs','generate-branding.mjs')){
    $taskGeneratorOutput=& node (Join-Path $PSScriptRoot $taskGenerator) --check 2>&1
    $taskGeneratorOutput | ForEach-Object {Write-Host $_}
    if($LASTEXITCODE -ne 0){throw "Shared mobile contract/assets stale: $taskGenerator"}
}
if(-not(Test-Path -LiteralPath $FlutterPath)){
    $taskSdk=Get-Command flutter -ErrorAction SilentlyContinue
    if($taskSdk){$FlutterPath=$taskSdk.Source}
    elseif(-not $Plan){throw 'Flutter absent: inspect mobile/README.md and configured existing SDK before installing another copy.'}
}
$taskCommands=[Collections.Generic.List[object]]::new()
if($Restore){$taskCommands.Add(@('pub','get','--offline','--enforce-lockfile'))}
$taskCommands.Add(@('analyze','--no-pub'))
if($Mode -ne 'Analyze'){$taskCommands.Add(@('test','--no-pub'))}
if($Mode -eq 'ApkPreview'){$taskCommands.Add(@('build','apk','--release','--no-pub'))}
if($Plan){return [pscustomobject]@{
    Executable=$FlutterPath;WorkingDirectory=$taskMobile;Commands=$taskCommands;
    Cache=(Join-Path $taskMobile 'build');CacheBudgetGiB=$MaximumCacheGiB;
    MinimumFreeGiB=$MinimumFreeGiB;ContextGate=$taskContext.GateChecked;
    StorageGate=$taskStorage.GateChecked;Signing='Existing debug identity: preview, not production'
}}
. (Join-Path $PSScriptRoot 'storage-hygiene.ps1') -FunctionsOnly
$taskCache=Get-DirectorySummary (Join-Path $taskMobile 'build')
if($taskCache.GiB -gt $MaximumCacheGiB){throw 'Mobile canonical cache over budget; inspect ownership before any cleanup. Never clean/rebuild or relocate around this guard.'}
$taskDrive=Get-PSDrive -PSProvider FileSystem | Where-Object {
    $taskMobile.StartsWith($_.Root,[StringComparison]::OrdinalIgnoreCase) -and $_.Name -ne 'Temp'
} | Sort-Object {$_.Root.Length} -Descending | Select-Object -First 1
if(-not $taskDrive -or $null -eq $taskDrive.Free -or $taskDrive.Free -lt $MinimumFreeGiB*1GB){
    throw 'Insufficient verified mobile build-volume free space.'
}
if(-not $Restore -and -not(Test-Path -LiteralPath (Join-Path $taskMobile '.dart_tool/package_config.json'))){
    throw 'Dependencies not resolved. Use -Restore for checksum-locked offline restoration; no automatic upgrade or new SDK.'
}
$taskNoProxy=[Environment]::GetEnvironmentVariable('NO_PROXY','Process')
$taskCapture=[Environment]::GetEnvironmentVariable('PAKAR_MOBILE_CAPTURE_PATH','Process')
if($CapturePath){
    if($Mode -eq 'Analyze' -or -not[IO.Path]::IsPathRooted($CapturePath)){throw 'Capture requires absolute output and Test/ApkPreview.'}
    $taskOutputRoot=[IO.Path]::GetFullPath((Join-Path $taskRoot '../../outputs/PakarController'))
    $taskCaptureFull=[IO.Path]::GetFullPath($CapturePath)
    if(-not $taskCaptureFull.StartsWith($taskOutputRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
        throw 'Mobile capture must stay in user deliverable outputs/PakarController.'
    }
    if(Test-Path -LiteralPath $taskCaptureFull){throw 'Capture exists; inspect it rather than overwriting evidence.'}
    New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($taskCaptureFull)) | Out-Null
}
try{
    $env:NO_PROXY='localhost,127.0.0.1,::1'
    if($CapturePath){$env:PAKAR_MOBILE_CAPTURE_PATH=$taskCaptureFull}else{Remove-Item Env:PAKAR_MOBILE_CAPTURE_PATH -ErrorAction SilentlyContinue}
    Push-Location $taskMobile
    try{
        foreach($taskArguments in $taskCommands){
            & $FlutterPath @taskArguments
            if($LASTEXITCODE -ne 0){throw "Mobile $Mode failed: $($taskArguments -join ' ')"}
        }
    }finally{Pop-Location}
}finally{
    if($null -eq $taskNoProxy){Remove-Item Env:NO_PROXY -ErrorAction SilentlyContinue}else{$env:NO_PROXY=$taskNoProxy}
    if($null -eq $taskCapture){Remove-Item Env:PAKAR_MOBILE_CAPTURE_PATH -ErrorAction SilentlyContinue}else{$env:PAKAR_MOBILE_CAPTURE_PATH=$taskCapture}
}
if($Mode -eq 'ApkPreview'){
    $taskApk=Join-Path $taskMobile 'build/app/outputs/flutter-apk/app-release.apk'
    if(-not(Test-Path -LiteralPath $taskApk)){throw 'Successful command did not produce the expected APK.'}
    [pscustomobject]@{Path=$taskApk;SHA256=(Get-FileHash -LiteralPath $taskApk).Hash.ToLowerInvariant();
        Signing='Preview only; production signing and physical device acceptance remain required'}
}
