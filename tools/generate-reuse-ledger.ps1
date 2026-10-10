[CmdletBinding()]
param(
    [string]$PealayerRoot = 'C:\Users\David\AppData\Local\Programs\Pealayer\source\Pealayer',
    [string]$PCControllerRoot = 'C:\Users\David\AppData\Local\Programs\PCController\source\PCController',
    [string]$PuzzlesRoot = 'C:\Users\David\Desktop\Projects\ControllerBoardMini\Puzzles',
    [string]$DecisionsPath,
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($DecisionsPath)) { $DecisionsPath = Join-Path $PSScriptRoot '..\docs\reuse-decisions.csv' }
if ([string]::IsNullOrWhiteSpace($OutputPath)) { $OutputPath = Join-Path $PSScriptRoot '..\docs\legacy-source-ledger.csv' }

function Get-GitFiles {
    param([string]$Root)

    $commit = (& git -C $Root rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw "Unable to read Git revision for $Root" }
    $files = & git -C $Root ls-files
    if ($LASTEXITCODE -ne 0) { throw "Unable to enumerate Git files for $Root" }
    return @{ Commit = $commit; Files = $files }
}

function Get-Category {
    param([string]$Origin, [string]$RelativePath)

    $path = $RelativePath.Replace('\', '/').ToLowerInvariant()
    if ($path -match '(^|/)(test|tests|fixtures?)(/|$)' -or $path -match '(test|spec)\.[^.]+$') { return 'tests' }
    if ($path -match '(^|/)(docs?|notes?)(/|$)' -or $path -match '\.(md|txt|pdf)$') { return 'documentation' }
    if ($path -match '(^|/)(assets?|icons?|images?|themes?)(/|$)' -or $path -match '\.(png|jpe?g|svg|ico|webp|json)$') { return 'design-assets' }
    if ($path -match '(^|/)(scripts?|tools?|ci|\.github)(/|$)' -or $path -match '\.(ps1|cmd|bat|sh|ya?ml)$') { return 'tooling-ci' }
    if ($path -match '(firmware|arduino|local-lib|locallib|\.ino$|\.cpp$|\.h$)') { return 'firmware-hardware' }
    if ($path -match '(web_ui|web/src|components?|views?|styles?|palette|theme|icons?|preferences?|tray|status_bar|dialog)') { return 'ui-design' }
    if ($path -match '(api|protocol|transport|websocket|mqtt|mdns|ssdp|discovery|serial|bridge)') { return 'communications' }
    if ($path -match '(cargo|go\.mod|package(-lock)?\.json|requirements|dependencies)') { return 'dependencies' }
    return 'application-core'
}

function Add-LedgerRows {
    param(
        [System.Collections.Generic.List[object]]$Rows,
        [string]$Origin,
        [string]$Root,
        [string]$Revision,
        [string[]]$RelativePaths
    )

    foreach ($relative in $RelativePaths) {
        if ([string]::IsNullOrWhiteSpace($relative)) { continue }
        $normalized = $relative.Replace('\', '/')
        $fullPath = Join-Path $Root $relative
        if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) { continue }
        $item = Get-Item -LiteralPath $fullPath
        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $fullPath).Hash.ToLowerInvariant()
        $category = Get-Category -Origin $Origin -RelativePath $normalized
        $signals = [System.Collections.Generic.List[string]]::new()
        if ($item.Length -le 2MB -and $item.Extension -notmatch '^\.(png|jpe?g|gif|webp|ico|pdf|zip|gz|woff2?|ttf|otf|exe|dll|bin|hex)$') {
            try {
                $content = [IO.File]::ReadAllText($fullPath)
                foreach ($candidate in @(
                    'phosphor', 'palette', 'theme', 'panel', 'preference', 'action', 'event',
                    'tray', 'status', 'discovery', 'mdns', 'ssdp', 'telnet', 'shell',
                    'websocket', 'mqtt', 'serial', 'protocol', 'firmware', 'updater', 'ota',
                    'tm1637', 'seven.segment', 'shift.register', 'buzzer', 'melod', 'eeprom', 'isp'
                )) {
                    if ($content -match $candidate) { $signals.Add($candidate.Replace('.', '-')) }
                }
            } catch {
                $signals.Add('unreadable-text')
            }
        }
        $Rows.Add([pscustomobject][ordered]@{
            origin = $Origin
            revision = $Revision
            path = $normalized
            sha256 = $hash
            bytes = $item.Length
            category = $category
            screening_signals = ($signals -join ';')
            review_status = 'screened'
            reuse_target = ''
            notes = "Content-hashed file-by-file screening complete; category=$category. Manual adaptation is recorded separately when applicable."
        })
    }
}

$rows = [System.Collections.Generic.List[object]]::new()

$pealayer = Get-GitFiles -Root $PealayerRoot
Add-LedgerRows -Rows $rows -Origin 'Pealayer' -Root $PealayerRoot -Revision $pealayer.Commit -RelativePaths $pealayer.Files

$pccontroller = Get-GitFiles -Root $PCControllerRoot
Add-LedgerRows -Rows $rows -Origin 'PCController' -Root $PCControllerRoot -Revision $pccontroller.Commit -RelativePaths $pccontroller.Files

$puzzleFiles = Get-ChildItem -LiteralPath $PuzzlesRoot -Recurse -File |
    Where-Object { $_.FullName -notmatch '[\\/](build|\.git|\.pio)[\\/]' } |
    ForEach-Object { $_.FullName.Substring($PuzzlesRoot.Length).TrimStart('\', '/') }
$puzzleRevision = if (Test-Path -LiteralPath (Join-Path $PuzzlesRoot '.git')) {
    (& git -C $PuzzlesRoot rev-parse HEAD).Trim()
} else {
    'filesystem-snapshot'
}
Add-LedgerRows -Rows $rows -Origin 'ControllerBoardMini/Puzzles' -Root $PuzzlesRoot -Revision $puzzleRevision -RelativePaths $puzzleFiles

if (Test-Path -LiteralPath $DecisionsPath) {
    $decisions = Import-Csv -LiteralPath $DecisionsPath
    $decisionIndex = @{}
    foreach ($decision in $decisions) {
        $decisionIndex["$($decision.origin)|$($decision.path)"] = $decision
    }
    foreach ($row in $rows) {
        $decision = $decisionIndex["$($row.origin)|$($row.path)"]
        if ($null -eq $decision) { continue }
        $row.review_status = $decision.review_status
        $row.reuse_target = $decision.reuse_target
        $row.notes = $decision.notes
    }
}

$outputDirectory = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$rows | Sort-Object origin, path | Export-Csv -NoTypeInformation -Encoding utf8 -LiteralPath $OutputPath

$summary = $rows | Group-Object origin | Sort-Object Name | ForEach-Object {
    [pscustomobject]@{ Origin = $_.Name; Files = $_.Count }
}
$summary | Format-Table -AutoSize
Write-Host "Ledger: $OutputPath"
