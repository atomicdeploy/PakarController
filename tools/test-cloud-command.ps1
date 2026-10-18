param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern peer-binding -Gate | Out-Null
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'build/native/cloud_command_guard_test.exe'
New-Item -ItemType Directory -Force (Split-Path -Parent $out) | Out-Null
foreach($configuration in @('ESP8266','ESP32')) {
    # One existing canonical output, reused sequentially; no debug/second cache.
    & g++ -std=c++17 -Wall -Wextra -Werror "-D$configuration" `
        -I (Join-Path $root 'libraries/BoardCore/src') `
        (Join-Path $root 'tests/cloud_command_guard_test.cpp') -o $out
    if($LASTEXITCODE){throw "Cloud command $configuration fixture compilation failed"}
    & $out
    if($LASTEXITCODE){throw "Cloud command $configuration assertions failed"}
}
