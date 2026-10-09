$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputDirectory = Join-Path $projectRoot 'build/native'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$testBinary = Join-Path $outputDirectory 'protocol_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror `
    -I (Join-Path $projectRoot 'libraries/BoardCore/src') `
    (Join-Path $projectRoot 'tests/protocol_test.cpp') `
    -o $testBinary
if ($LASTEXITCODE -ne 0) { throw 'Protocol test compilation failed' }
& $testBinary
if ($LASTEXITCODE -ne 0) { throw 'Protocol tests failed' }
Write-Host 'Protocol round-trip, bounds, and corruption tests passed.'
