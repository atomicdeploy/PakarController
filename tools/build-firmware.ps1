param(
    [ValidateSet('polling', 'busy-wire', 'buffered')]
    [string] $LinkMode = $(if ($env:PAKAR_LINK_MODE) { $env:PAKAR_LINK_MODE } else { 'busy-wire' }),
    [ValidatePattern('^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$')]
    [string] $DeviceName,
    [ValidateRange(1, 32)] [int] $Jobs = 2,
    [ValidateSet('both', 'avr', 'esp8266', 'esp32s3')] [string] $Target = 'both',
    [ValidateSet(4, 8, 16, 32)] [int] $S3FlashMB = 4,
    [ValidateSet('disabled', 'enabled', 'opi')] [string] $S3Psram = 'disabled',
    [ValidateSet(-1, 38, 48)] [int] $S3StatusRgbPin = -1,
    [switch] $InitializeFilesystem,
    [switch] $PauseAvrLink,
    [switch] $S3IspElectricalVerified,
    [switch] $VerboseBuild,
    [switch] $Clean,
    [switch] $Plan
)

$ErrorActionPreference = 'Stop'
if ($Target -eq 'esp32s3' -and -not $PSBoundParameters.ContainsKey('LinkMode') -and -not $env:PAKAR_LINK_MODE) { $LinkMode = 'buffered' }
if ($Target -eq 'esp32s3' -and $LinkMode -ne 'buffered') { throw 'ESP32-S3 UART integration requires the explicit paired buffered profile.' }
if ($PauseAvrLink -and $Target -ne 'esp32s3') { throw 'Quiet AVR-link commissioning currently targets ESP32-S3 only.' }
if ($S3IspElectricalVerified -and $Target -ne 'esp32s3') { throw 'The S3 ISP electrical acknowledgment is target-specific.' }
if ($LinkMode -eq 'buffered' -and $Target -notin @('avr', 'esp32s3')) {
    throw 'Buffered mode currently targets the paired AVR only; ESP8266 requires busy-wire or polling.'
}
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern api-contract -Gate | Out-Null
& node (Join-Path $PSScriptRoot 'check-living-contract.mjs') | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0) { throw 'Living API policy failed before firmware build.' }
$projectRoot = Split-Path -Parent $PSScriptRoot
if ($Target -in @('both', 'avr')) {
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern cooperative-tasks -Gate | Out-Null
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern uart-flow-control -Gate | Out-Null
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern tm1637-display -Gate | Out-Null
}
if ($Target -eq 'esp32s3') {
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern uart-flow-control -Gate | Out-Null
}
if ($Target -ne 'avr') {
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern web-presentation -Gate | Out-Null
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern web-streaming -Gate | Out-Null
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

foreach ($board in @('avr', 'esp8266', 'esp32s3')) {
    if ($board -eq 'esp32s3' -and $Target -ne 'esp32s3') { continue }
    if ($Target -ne 'both' -and $Target -ne $board) { continue }
    $output = if ($board -eq 'avr') { $avrOutput } elseif ($board -eq 'esp32s3') { Join-Path $build 'esp32s3-buffered' } else { $espOutput }
    $flags = if ($board -eq 'avr') { $extraFlags } else { $espFlags }
    $fqbn = if ($board -eq 'avr') {
        'MiniCore:avr:328:bootloader=uart0,eeprom=keep,baudrate=115200,variant=modelP,BOD=2v7,LTO=Os_flto,clock=16MHz_external'
    } elseif ($board -eq 'esp32s3') {
        $partitions = if ($S3FlashMB -eq 4) { 'min_spiffs' } else { 'default_8MB' }
        # Explicit hardware assumptions; deploy only after reading actual flash.
        # 16/32 MB hardware deliberately uses the proven first 8 MB layout.
        # The CLI expands this property to argv: embedded literal quotes become
        # part of GCC's search path. The canonical build roots contain no spaces.
        if ($projectRoot -match '\s') { throw 'S3 shared-source include root requires a whitespace-free checkout path.' }
        $flags += " -I$($projectRoot.Replace('\','/')) -DARDUINO_USB_CDC_ON_BOOT=1 -DARDUINO_USB_MODE=1"
        if ($S3StatusRgbPin -ne -1) { $flags += " -DPAKAR_S3_STATUS_RGB_PIN=$S3StatusRgbPin" }
        if ($InitializeFilesystem) { $flags += ' -DPAKAR_INITIALIZE_FILESYSTEM=1' }
        if ($PauseAvrLink) { $flags += ' -DPAKAR_AVR_LINK_ENABLED=0' }
        if ($S3IspElectricalVerified) { $flags += ' -DPAKAR_S3_ISP_ELECTRICAL_VERIFIED=1' }
        "esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,FlashSize=${S3FlashMB}M,PartitionScheme=$partitions,PSRAM=$S3Psram"
    } else { 'esp8266:esp8266:nodemcuv2:eesz=4M2M' }
    # One reusable cache per board/link-mode; never force a cold compile by default.
    $cache = Join-Path $build "cache/$board$suffix"
    $arguments = @('compile', '--jobs', "$Jobs", '--fqbn', $fqbn,
        '--libraries', $libraries, '--build-property', "compiler.cpp.extra_flags=$flags",
        '--build-path', $cache, '--output-dir', $output,
        (Join-Path $projectRoot "firmware/$board"))
    if ($board -eq 'esp32s3') {
        # Arduino's initial sketch dependency scan also uses the C recipe.
        $arguments += @('--build-property', "compiler.c.extra_flags=$flags")
        $arguments += @('--build-property', 'compiler.optimization_flags=-Os -g0')
    }
    if ($Clean) { $arguments += '--clean' }
    if ($VerboseBuild) { $arguments += '--verbose' }
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
