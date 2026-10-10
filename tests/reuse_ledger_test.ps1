$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$scratch = Join-Path ([IO.Path]::GetTempPath()) ('PakarController-ledger-test-' + [guid]::NewGuid())
$donor = Join-Path $scratch 'donor'
New-Item -ItemType Directory -Path $donor -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'reuse_fixture/source.h') -Destination $donor
& git -C $donor init --quiet
& git -C $donor add source.h
& git -C $donor -c user.name=LedgerFixture -c user.email=fixture@example.invalid commit --quiet -m fixture
if ($LASTEXITCODE -ne 0) { throw 'Fixture Git creation failed' }
$hash = (Get-FileHash -LiteralPath (Join-Path $donor 'source.h')).Hash.ToLowerInvariant()
$decisions = Join-Path $scratch 'decisions.csv'
$evidence = Join-Path $scratch 'evidence.csv'
$ledger = Join-Path $scratch 'ledger.csv'
@([pscustomobject]@{origin='Pealayer';path='source.h';review_status='adapted';reuse_target='test-target';notes='Fixture legacy decision'}) |
    Export-Csv -NoTypeInformation -LiteralPath $decisions
@([pscustomobject]@{origin='PCController';path='source.h';sha256=$hash;review_status='adapted';reuse_target='test-target';notes='Fixture manual evidence'}) |
    Export-Csv -NoTypeInformation -LiteralPath $evidence
$options = @{
    PealayerRoot=$donor; PCControllerRoot=$donor; PuzzlesRoot=$donor;
    PuzzleBoardRoot=$donor; ControllerBoardMiniRoot=$donor; Pu850Root=$donor; RayanLampRoot=$donor;
    DecisionsPath=$decisions; EvidencePath=$evidence; OutputPath=$ledger
}
function Assert-State([string]$origin, [string]$status) {
    $row = Import-Csv -LiteralPath $ledger | Where-Object { $_.origin -eq $origin -and $_.path -eq 'source.h' }
    if ($row.review_status -ne $status) { throw "Expected $origin $status; got $($row.review_status)" }
}
try {
    & (Join-Path $root 'tools/generate-reuse-ledger.ps1') @options | Out-Null
    Assert-State Pealayer legacy-unverified
    Assert-State PCController adapted
    Assert-State RayanLamp screened
    . (Join-Path $root 'tools/donor-files.ps1')
    foreach ($relative in @('node_modules','dist','vendor')) {
        New-Item -ItemType Directory -Path (Join-Path $donor $relative) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'reuse_fixture/source.h') -Destination (Join-Path $donor "$relative/source.h")
    }
    $sourcePaths = @(Get-RayanLampFiles ($donor + [IO.Path]::DirectorySeparatorChar))
    if ('vendor/source.h' -notin $sourcePaths -or 'dist/source.h' -in $sourcePaths -or 'node_modules/source.h' -in $sourcePaths) {
        throw 'Source snapshot must retain vendor and prune generated output before recursion'
    }
    $initialRows = @(Import-Csv -LiteralPath $ledger)
    if (@($initialRows | Where-Object {$_.origin -eq 'RayanLamp' -and $_.path -match '^\.git/'}).Count) { throw 'Generated Git internals were inventoried as donor source' }
    & (Join-Path $root 'tools/check-reuse-ledger.ps1') -LedgerPath $ledger -EvidencePath $evidence | Out-Null
    $rejected = $false
    try { & (Join-Path $root 'tools/check-reuse-ledger.ps1') -LedgerPath $ledger -EvidencePath $evidence -RequireComplete | Out-Null }
    catch { $rejected = $_.Exception.Message -match 'Production release blocked' }
    if (-not $rejected) { throw 'Unresolved ledger wrongly passed release gate' }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'reuse_fixture/changed.h') -Destination (Join-Path $donor 'source.h') -Force
    & (Join-Path $root 'tools/generate-reuse-ledger.ps1') @options | Out-Null
    Assert-State PCController stale-review
    # Simulate a sparse/missing tracked file without deleting its contents.
    Move-Item -LiteralPath (Join-Path $donor 'source.h') -Destination (Join-Path $scratch 'saved-source.h')
    & (Join-Path $root 'tools/generate-reuse-ledger.ps1') @options | Out-Null
    Assert-State PCController missing
    Write-Host 'Ledger tests passed: legacy hints are not reviews; hashes invalidate stale evidence; missing tracked files remain inventoried; strict release gate rejects unresolved scope.'
} finally {
    # This tiny evidence fixture is intentionally retained for debugging.
    Write-Host "Fixture evidence: $scratch"
}
