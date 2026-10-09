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

& $cli compile `
    --clean `
    --fqbn 'MiniCore:avr:328:bootloader=uart0,eeprom=keep,baudrate=115200,variant=modelP,BOD=2v7,LTO=Os_flto,clock=16MHz_external' `
    --libraries $libraries `
    --output-dir (Join-Path $build 'avr') `
    (Join-Path $projectRoot 'firmware/avr')
if ($LASTEXITCODE -ne 0) { throw 'AVR build failed' }

& $cli compile `
    --clean `
    --fqbn 'esp8266:esp8266:nodemcuv2:eesz=4M2M' `
    --libraries $libraries `
    --output-dir (Join-Path $build 'esp8266') `
    (Join-Path $projectRoot 'firmware/esp8266')
if ($LASTEXITCODE -ne 0) { throw 'ESP8266 build failed' }

$mergedImage = Join-Path $build 'avr/avr.ino.with_bootloader.bin'
if ((Get-Item -LiteralPath $mergedImage).Length -ne 32768) {
    throw 'Merged AVR application plus Urboot image is not exactly 32768 bytes'
}
Write-Host 'Firmware builds passed; merged AVR plus Urboot image is 32768 bytes.'
