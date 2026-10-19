param(
    [ValidateSet('polling', 'busy-wire', 'buffered')]
    [string] $LinkMode = $(if ($env:PAKAR_LINK_MODE) { $env:PAKAR_LINK_MODE } else { 'busy-wire' }),
    [ValidatePattern('^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$')]
    [string] $DeviceName,
    [ValidateRange(1, 32)] [int] $Jobs = [Math]::Min(32, [Environment]::ProcessorCount),
    [ValidateSet('both', 'avr', 'esp8266', 'esp32s3')] [string] $Target = 'both',
    [ValidateSet(4, 8, 16, 32)] [int] $S3FlashMB = 4,
    [ValidateSet('disabled', 'enabled', 'opi')] [string] $S3Psram = 'disabled',
    [ValidateSet('static', 'rmt-dma')] [string] $S3LedBackend = 'static',
    [ValidateSet('runtime', 'disabled')] [string] $S3Bluetooth = 'runtime',
    [ValidateSet(-1, 38, 48)] [int] $S3StatusRgbPin = -1,
    [ValidateRange(1,4)] [int] $ShellSessions = 2,
    [switch] $DisableSerialShell,
    [switch] $DisableTelnet,
    [switch] $InitializeFilesystem,
    [switch] $PauseAvrLink,
    [switch] $PauseLedOutput,
    [switch] $CellChaseOnBoot,
    [switch] $S3IspElectricalVerified,
    [switch] $S3Ethernet,
    [switch] $VerboseBuild,
    [switch] $Clean,
    [switch] $Plan
)

$ErrorActionPreference = 'Stop'
if(($PSBoundParameters.ContainsKey('ShellSessions') -or $DisableSerialShell -or $DisableTelnet) -and $Target -ne 'esp32s3'){throw 'Shell build switches currently use the S3 application-only profile; other targets retain macro defaults.'}
. (Join-Path $PSScriptRoot 'script-runtime.ps1')
if ($Target -eq 'esp32s3' -and -not $PSBoundParameters.ContainsKey('LinkMode') -and -not $env:PAKAR_LINK_MODE) { $LinkMode = 'buffered' }
if ($Target -eq 'esp32s3' -and $LinkMode -ne 'buffered') { throw 'ESP32-S3 UART integration requires the explicit paired buffered profile.' }
if ($PauseAvrLink -and $Target -ne 'esp32s3') { throw 'Quiet AVR-link commissioning currently targets ESP32-S3 only.' }
if ($PauseLedOutput -and $Target -ne 'esp32s3') { throw 'Quiet LED commissioning currently targets ESP32-S3 only.' }
if ($CellChaseOnBoot -and ($Target -ne 'esp32s3' -or $PauseLedOutput)) { throw 'Cell-chase boot testing requires ESP32-S3 with LED output enabled.' }
if ($S3IspElectricalVerified -and $Target -ne 'esp32s3') { throw 'The S3 ISP electrical acknowledgment is target-specific.' }
if ($S3Ethernet -and $Target -ne 'esp32s3') { throw 'Native W5500 Ethernet requires ESP32-S3.' }
if ($S3Bluetooth -ne 'runtime' -and $Target -ne 'esp32s3') { throw 'Bluetooth build selection requires ESP32-S3.' }
if ($S3LedBackend -ne 'static' -and $Target -ne 'esp32s3') { throw 'The RMT-DMA LED backend requires ESP32-S3.' }
if ($LinkMode -eq 'buffered' -and $Target -notin @('avr', 'esp32s3')) {
    throw 'Buffered mode currently targets the paired AVR only; ESP8266 requires busy-wire or polling.'
}
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern api-contract -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern pu-tooling -Gate | Out-Null
& node (Join-Path $PSScriptRoot 'check-living-contract.mjs') | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0) { throw 'Living API policy failed before firmware build.' }
$projectRoot = Split-Path -Parent $PSScriptRoot
if ($Target -in @('both', 'avr')) {
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern avr-boot-chain -Gate | Out-Null
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern cooperative-tasks -Gate | Out-Null
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern uart-flow-control -Gate | Out-Null
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern tm1637-display -Gate | Out-Null
}
if ($Target -eq 'esp32s3') {
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern uart-flow-control -Gate | Out-Null
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern lcd-display -Gate | Out-Null
    if ($S3Ethernet) {
        & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern ethernet -Gate | Out-Null
    }
}
if ($Target -ne 'avr') {
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern device-shell -Gate | Out-Null
    # Route evidence is a separate reviewed artifact from the file ledger.
    # Fail before compiling/flashing if a changed sketch has stale route hashes.
    & node (Join-Path $PSScriptRoot '../tests/pu_routes_test.mjs') | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw 'PU route evidence is stale; re-review affected routes and regenerate their exact target bindings.' }
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern remote-ota -Gate | Out-Null
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
    & node (Join-Path $PSScriptRoot 'generate-basic-ui.mjs') --check | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw 'Basic cell app is stale; regenerate before ESP build.' }
    & node (Join-Path $PSScriptRoot 'generate-web-compressed.mjs') --check | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw 'Compressed ESP8266 Web UI is stale; regenerate before ESP build.' }
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
        if ($PauseLedOutput) { $flags += ' -DPAKAR_LED_START_ENABLED=0' }
        if ($CellChaseOnBoot) { $flags += ' -DPAKAR_LED_START_CELL_CHASE=1' }
        if ($S3IspElectricalVerified) { $flags += ' -DPAKAR_S3_ISP_ELECTRICAL_VERIFIED=1' }
        if ($S3Ethernet) { $flags += ' -DPAKAR_S3_ETHERNET_ENABLED=1' }
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
        $taskLinkFlags='compiler.c.elf.extra_flags=-Wl,--wrap=dns_clear_cache,--wrap=gdma_new_ahb_channel,--wrap=esp_intr_alloc_intrstatus,--wrap=esp_partition_write,--wrap=esp_partition_write_raw,--wrap=esp_partition_erase_range,--wrap=esp_partition_read,--wrap=esp_partition_read_raw,--wrap=esp_image_verify'
        if($S3Ethernet){$taskLinkFlags+=',--wrap=esp_eth_update_input_path_info'}
        $arguments += @('--build-property',$taskLinkFlags)
    }
    if ($Clean) { $arguments += '--clean' }
    $ledProfile = $null
    if ($board -eq 'esp32s3') {
        $ledProfile = [pscustomobject]@{
            Path = Join-Path $build 'config/esp32s3/LedBackend.h'
            Content = "#pragma once`n#define PAKAR_S3_LED_USE_DMA $(if ($S3LedBackend -eq 'rmt-dma') { 1 } else { 0 })`n#ifndef PAKAR_S3_BLE_ENABLED`n#define PAKAR_S3_BLE_ENABLED $(if ($S3Bluetooth -eq 'disabled') { 0 } else { 1 })`n#endif`n#define PAKAR_TELNET_SESSIONS $ShellSessions`n#define PAKAR_SERIAL_SHELL_ENABLED $(if ($DisableSerialShell) { 0 } else { 1 })`n#define PAKAR_TELNET_ENABLED $(if ($DisableTelnet) { 0 } else { 1 })`n"
        }
    }
    if ($VerboseBuild) { $arguments += '--verbose' }
    if ($Plan) {
        [pscustomobject]@{ Board=$board; Executable=$cli; Arguments=$arguments; LedBackendProfile=$ledProfile }
        continue
    }
    $ledProfileLease = $null
    try {
        if ($ledProfile) {
            New-Item -ItemType Directory -Force (Split-Path -Parent $ledProfile.Path) | Out-Null
            # Hold one canonical profile/cache owner throughout compilation.
            # Another backend build must fail, not rewrite this header mid-build.
            $ledProfileLease = [IO.File]::Open($ledProfile.Path + '.lock',
                [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
            if (-not (Test-Path -LiteralPath $ledProfile.Path) -or
                [IO.File]::ReadAllText($ledProfile.Path) -cne $ledProfile.Content) {
                [IO.File]::WriteAllText($ledProfile.Path, $ledProfile.Content, [Text.UTF8Encoding]::new($false))
            }
        }
        Write-PakarStep "Firmware $board · $LinkMode · reusable cache (CLI may invalidate source/options); no automatic flash"
        Invoke-PakarNative -WorkingDirectory $projectRoot -FilePath $cli -Arguments $arguments
    } finally {
        if ($ledProfileLease) { $ledProfileLease.Dispose() }
    }
    if ($board -eq 'avr') {
        & node (Join-Path $PSScriptRoot 'prepare-avr-image.mjs') $avrOutput $cache
        if ($LASTEXITCODE -ne 0) { throw 'AVR Urboot vector-chain packaging failed; do not deploy raw merged output.' }
        $mergedImage = Join-Path $avrOutput 'avr.ino.with_bootloader.bin'
        if ((Get-Item -LiteralPath $mergedImage).Length -ne 32768) {
            throw 'Merged AVR application plus Urboot image is not exactly 32768 bytes'
        }
    }
}
if (-not $Plan) { Write-Host "Requested firmware builds passed in $LinkMode mode." }
