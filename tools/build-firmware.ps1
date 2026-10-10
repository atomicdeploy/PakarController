param(
    [ValidateSet('polling', 'busy-wire')]
    [string] $LinkMode = $(if ($env:PAKAR_LINK_MODE) { $env:PAKAR_LINK_MODE } else { 'busy-wire' })
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$libraries = Join-Path $projectRoot 'libraries'
$build = Join-Path $projectRoot 'build'
$projectCli = Join-Path $PSScriptRoot 'arduino-cli.exe'
$cli = if (Test-Path -LiteralPath $projectCli) {
    $projectCli
} else {
    (Get-Command arduino-cli -ErrorAction Stop).Source
}
$busyFlag = if ($LinkMode -eq 'busy-wire') { 1 } else { 0 }
$suffix = if ($busyFlag) { '-busy-wire' } else { '' }
$avrOutput = Join-Path $build ("avr$suffix")
$espOutput = Join-Path $build ("esp8266$suffix")
$extraFlags = "-DPAKAR_USE_BUSY_WIRE=$busyFlag"

& $cli compile `
    --clean `
    --fqbn 'MiniCore:avr:328:bootloader=uart0,eeprom=keep,baudrate=115200,variant=modelP,BOD=2v7,LTO=Os_flto,clock=16MHz_external' `
    --libraries $libraries `
    --build-property "compiler.cpp.extra_flags=$extraFlags" `
    --output-dir $avrOutput `
    (Join-Path $projectRoot 'firmware/avr')
if ($LASTEXITCODE -ne 0) { throw 'AVR build failed' }

& $cli compile `
    --clean `
    --fqbn 'esp8266:esp8266:nodemcuv2:eesz=4M2M' `
    --libraries $libraries `
    --build-property "compiler.cpp.extra_flags=$extraFlags" `
    --output-dir $espOutput `
    (Join-Path $projectRoot 'firmware/esp8266')
if ($LASTEXITCODE -ne 0) { throw 'ESP8266 build failed' }

$mergedImage = Join-Path $avrOutput 'avr.ino.with_bootloader.bin'
if ((Get-Item -LiteralPath $mergedImage).Length -ne 32768) {
    throw 'Merged AVR application plus Urboot image is not exactly 32768 bytes'
}
Write-Host "Firmware builds passed in $LinkMode mode; merged AVR plus Urboot image is 32768 bytes."
