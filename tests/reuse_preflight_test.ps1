$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
function Assert-Reuse($value,$message){if(-not $value){throw $message}}
function Assert-Rejected([scriptblock]$action,[string]$pattern){
    $taskRejected=$false
    try{& $action | Out-Null}catch{Assert-Reuse ($_.Exception.Message -match $pattern) "Wrong rejection: $_"; $taskRejected=$true}
    Assert-Reuse $taskRejected "Expected rejection: $pattern"
}
$taskConsult=Join-Path $taskRoot 'tools/consult-reuse.ps1'
$taskIndex=Get-Content (Join-Path $taskRoot 'docs/reuse-capabilities.json') -Raw | ConvertFrom-Json
Assert-Reuse (@($taskIndex.concerns.id | Select-Object -Unique).Count -eq $taskIndex.concerns.Count) 'Unique concern IDs'
foreach($taskConcern in $taskIndex.concerns){
    $taskLookup=& $taskConsult -Concern $taskConcern.id -DonorRoots @{}
    Assert-Reuse ($taskLookup.Matched -gt 0 -and $taskLookup.Inventoried -eq 1868) "Inventory coverage: $($taskConcern.id)"
    Assert-Reuse ($taskLookup.Meaning -match 'NOT accepted') 'No lookup-as-acceptance claim'
}
Assert-Rejected {& $taskConsult -Concern invented -DonorRoots @{}} 'Unknown'
Assert-Rejected {& $taskConsult -Concern preferences -Gate -DonorRoots @{}} 'No reviewed gate'
Assert-Rejected {& $taskConsult -Concern build-storage -RequireLive -DonorRoots @{}} 'unavailable or stale'
foreach($taskConcern in @('build-storage','capture')){
    $taskGate=& $taskConsult -Concern $taskConcern -Gate -DonorRoots @{}
    Assert-Reuse $taskGate.GateChecked "Cached reviewed-slice gate: $taskConcern"
}
# Tiny stable owned fixtures, not copies of full donor repositories or packages.
$taskFixture=Join-Path $taskRoot 'build/native/reuse-preflight-fixture'
New-Item -ItemType Directory -Force -Path $taskFixture | Out-Null
$taskLedgerPath=Join-Path $taskFixture 'ledger.csv'
$taskEvidencePath=Join-Path $taskFixture 'evidence.json'
$taskIndexPath=Join-Path $taskFixture 'index.json'
$taskSource=Import-Csv (Join-Path $taskRoot 'docs/legacy-source-ledger.csv') | Where-Object { $_.origin -eq 'Pealayer' -and $_.path -eq 'scripts/package-probe-windows.ps1'}
$taskEntry=(Get-Content (Join-Path $taskRoot 'docs/file-comparison-evidence.json') -Raw | ConvertFrom-Json).files | Where-Object { $_.origin -eq 'Pealayer' -and $_.path -eq $taskSource.path}
$taskSource | Export-Csv -LiteralPath $taskLedgerPath -NoTypeInformation
$taskFixtureIndex=@{schema=1; concerns=@(@{id='fixture';origins=@('Pealayer');path_pattern='.*';required_reviews=@("Pealayer|$($taskSource.path)");issue=3})}
[IO.File]::WriteAllText($taskIndexPath,($taskFixtureIndex | ConvertTo-Json -Depth 10))
function Write-FixtureEvidence($entry){[IO.File]::WriteAllText($taskEvidencePath,(@{schema=1;files=@($entry)} | ConvertTo-Json -Depth 15))}
function Invoke-Fixture{& $taskConsult -Concern fixture -Gate -DonorRoots @{} -LedgerPath $taskLedgerPath -EvidencePath $taskEvidencePath -IndexPath $taskIndexPath}
Write-FixtureEvidence $taskEntry
Assert-Reuse (Invoke-Fixture).GateChecked 'Valid exact source/target/test fixture'
$taskChanged=$taskEntry | ConvertTo-Json -Depth 15 | ConvertFrom-Json
$taskChanged.source_read='screened'; Write-FixtureEvidence $taskChanged
Assert-Rejected {Invoke-Fixture} 'not fully reviewed'
$taskChanged=$taskEntry | ConvertTo-Json -Depth 15 | ConvertFrom-Json
$taskChanged.sha256='0'*64; Write-FixtureEvidence $taskChanged
Assert-Rejected {Invoke-Fixture} 'not fully reviewed'
$taskChanged=$taskEntry | ConvertTo-Json -Depth 15 | ConvertFrom-Json
$taskChanged.targets=@(); Write-FixtureEvidence $taskChanged
Assert-Rejected {Invoke-Fixture} 'not fully reviewed'
$taskChanged=$taskEntry | ConvertTo-Json -Depth 15 | ConvertFrom-Json
$taskChanged.targets[0].sha256='0'*64; Write-FixtureEvidence $taskChanged
Assert-Rejected {Invoke-Fixture} 'target/test changed'
$taskChanged=$taskEntry | ConvertTo-Json -Depth 15 | ConvertFrom-Json
$taskChanged.targets[0].path='../escape.ps1'; Write-FixtureEvidence $taskChanged
Assert-Rejected {Invoke-Fixture} 'Escaping reuse target'
Write-FixtureEvidence $taskEntry
. (Join-Path $taskRoot 'tools/windows-build-host.ps1')
$taskPriorTarget=$env:CARGO_TARGET_DIR
try{
    $env:CARGO_TARGET_DIR='relative-cache'
    Assert-Reuse ((Get-PakarCargoTargetDirectory $taskRoot) -eq (Join-Path $taskRoot 'relative-cache')) 'Actual donor relative cache resolution'
}finally{if($null -ne $taskPriorTarget){$env:CARGO_TARGET_DIR=$taskPriorTarget}else{Remove-Item Env:CARGO_TARGET_DIR -ErrorAction SilentlyContinue}}
. (Join-Path $taskRoot 'tools/package-probe-windows.ps1')
$taskProbe=Invoke-PackageProbe $env:COMSPEC '/d /c echo Pakar-probe' $taskRoot
Assert-Reuse ($taskProbe.Trim() -eq 'Pakar-probe') 'Actual imported short-lived process stdout'
Assert-Rejected {Invoke-PackageProbe $env:COMSPEC '/d /c exit 7' $taskRoot} 'failed \(7\)'
Assert-Rejected {Assert-PakarPackageSourceStable $taskRoot ('0'*40)} 'source commit changed'
Write-Host 'Reuse routing, cached gates, stale/incomplete/escaping rejection, donor cache resolver and actual imported process probe tests passed; no compiler/build/package copy.'
