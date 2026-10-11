param(
    [ValidateSet('polling', 'busy-wire', 'buffered')]
    [string] $LinkMode = $(if ($env:PAKAR_LINK_MODE) { $env:PAKAR_LINK_MODE } else { 'busy-wire' }),
    [ValidatePattern('^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$')]
    [string] $DeviceName,
    [ValidateRange(1, 32)] [int] $Jobs = 2,
    [ValidateSet('both', 'avr', 'esp8266')] [string] $Target = 'both',
    [switch] $Clean,
    [switch] $Plan
)

$ErrorActionPreference = 'Stop'
if ($LinkMode -eq 'buffered' -and $Target -ne 'avr') {
    throw 'Buffered mode currently targets the paired AVR only; ESP8266 requires busy-wire or polling.'
}
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern api-contract -Gate | Out-Null
& node (Join-Path $PSScriptRoot 'check-living-contract.mjs') | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0) { throw 'Living API policy failed before firmware build.' }
$projectRoot = Split-Path -Parent $PSScriptRoot
if ($Target -ne 'esp8266') {
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern uart-flow-control -Gate | Out-Null
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern tm1637-display -Gate | Out-Null
}
if ($Target -ne 'avr') {
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern web-presentation -Gate | Out-Null
    & node (Join-Path $PSScriptRoot 'generate-font-assets.mjs') --check | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw 'Shared language/font assets are stale.' }
    & node (Join-Path $PSScriptRoot 'generate-interface-contract.mjs') --check | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw 'Shared interface catalog is stale.' }
    & node (Join-Path $PSScriptRoot 'generate-branding.mjs') --check | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw 'Application firmware/Web artwork is stale.' }
    & node (Join-Path $PSScriptRoot 'generate-web-appearance.mjs') --check | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw 'Shared Web appearance is stale; regenerate before ESP build.' }
}
$libraries = Join-Path $projectRoot 'libraries'
$build = Join-Path $projectRoot 'build'
$projectCli = Join-Path $PSScriptRoot 'arduino-cli.exe'
$cli = if (Test-Path -LiteralPath $projectCli) {
    $projectCli
} elseif ($Plan) {
    # Planning and storage tests do not require installing an unrelated SDK.
    'arduino-cli'
} else {
    (Get-Command arduino-cli -ErrorAction Stop).Source
}
$busyFlag = if ($LinkMode -eq 'busy-wire') { 1 } else { 0 }
$bufferedFlag = if ($LinkMode -eq 'buffered') { 1 } else { 0 }
$suffix = if ($busyFlag) { '-busy-wire' } elseif ($bufferedFlag) { '-buffered' } else { '' }
$avrOutput = Join-Path $build ("avr$suffix")
$espOutput = Join-Path $build ("esp8266$suffix")
$extraFlags = "-DPAKAR_USE_BUSY_WIRE=$busyFlag -DPAKAR_UART_BUFFERED=$bufferedFlag"
$espFlags = $extraFlags
if ($DeviceName) {
    if ($DeviceName -in @('local', 'localhost')) { throw 'Reserved device hostname' }
    $espFlags += " -DPAKAR_INITIAL_NAME=$DeviceName"
}

foreach ($board in @('avr', 'esp8266')) {
    if ($Target -ne 'both' -and $Target -ne $board) { continue }
    $output = if ($board -eq 'avr') { $avrOutput } else { $espOutput }
    $flags = if ($board -eq 'avr') { $extraFlags } else { $espFlags }
    $fqbn = if ($board -eq 'avr') {
        'MiniCore:avr:328:bootloader=uart0,eeprom=keep,baudrate=115200,variant=modelP,BOD=2v7,LTO=Os_flto,clock=16MHz_external'
    } else { 'esp8266:esp8266:nodemcuv2:eesz=4M2M' }
    # One reusable cache per board/link-mode; never force a cold compile by default.
    $cache = Join-Path $build "cache/$board$suffix"
    $arguments = @('compile', '--jobs', "$Jobs", '--fqbn', $fqbn,
        '--libraries', $libraries, '--build-property', "compiler.cpp.extra_flags=$flags",
        '--build-path', $cache, '--output-dir', $output,
        (Join-Path $projectRoot "firmware/$board"))
    if ($Clean) { $arguments += '--clean' }
    if ($Plan) {
        [pscustomobject]@{ Board=$board; Executable=$cli; Arguments=$arguments }
        continue
    }
    & $cli @arguments
    if ($LASTEXITCODE -ne 0) { throw "$board build failed" }
    if ($board -eq 'avr') {
        $mergedImage = Join-Path $avrOutput 'avr.ino.with_bootloader.bin'
        if ((Get-Item -LiteralPath $mergedImage).Length -ne 32768) {
            throw 'Merged AVR application plus Urboot image is not exactly 32768 bytes'
        }
    }
}
if (-not $Plan) { Write-Host "Requested firmware builds passed in $LinkMode mode." }
