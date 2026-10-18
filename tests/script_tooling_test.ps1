$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
. (Join-Path $taskRoot 'tools/donor-files.ps1')
$taskRow=[pscustomobject]@{origin='PU850-ESP';availability='pinned-blob';revision=('a'*40);sha256=('b'*64)}
$taskReview=[pscustomobject]@{source_blob=[pscustomobject]@{repository='atomicdeploy/pu850-esp';revision=('a'*40);sha256=('b'*64);git_sha=('c'*40)}}
if(-not(Test-PakarDonorSnapshot $taskRow $taskReview)){throw 'Exact pinned blob rejected'}
$taskReview.source_blob.revision='d'*40
if(Test-PakarDonorSnapshot $taskRow $taskReview){throw 'Changed source revision accepted'}
$taskReview.source_blob.revision='a'*40;$taskReview.source_blob.repository='other/repo'
if(Test-PakarDonorSnapshot $taskRow $taskReview){throw 'Wrong donor repository accepted'}
foreach($taskFile in (Get-ChildItem -LiteralPath (Join-Path $taskRoot 'tools') -Filter '*.ps1' -File)){
    $taskTokens=$null;$taskErrors=$null
    [void][Management.Automation.Language.Parser]::ParseFile($taskFile.FullName,[ref]$taskTokens,[ref]$taskErrors)
    if($taskErrors.Count){throw "PowerShell syntax: $($taskFile.Name): $taskErrors"}
}
. (Join-Path $taskRoot 'tools/script-runtime.ps1')
$taskBefore=(Get-Location).Path
try{Invoke-PakarNative -WorkingDirectory $taskRoot -FilePath (Get-Command node).Source -Arguments @('-e','process.exit(7)');throw 'Expected failed child'}
catch{if($_.Exception.Message -notmatch 'exit code 7'){throw}}
if((Get-Location).Path -ne $taskBefore){throw 'Failed command changed caller directory'}
# Exercise the successful native path too. GitHub's pwsh -Command wrapper
# propagates LASTEXITCODE; the intentionally rejected child above must not be
# the test script's final native status. Do not clear/ignore a production error.
Invoke-PakarNative -WorkingDirectory $taskRoot -FilePath (Get-Command node).Source -Arguments @('-e','process.exit(0)')
if($LASTEXITCODE -ne 0 -or (Get-Location).Path -ne $taskBefore){throw 'Successful native command did not preserve exit/caller directory'}
Write-Host 'PowerShell utilities parse; pinned-blob source identity fails closed; actual shared native runner preserves failed/successful exits and caller directory. No SDK/device contact.'
