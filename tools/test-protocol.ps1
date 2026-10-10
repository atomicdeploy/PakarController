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
$panelBinary = Join-Path $outputDirectory 'front_panel_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror `
    -I (Join-Path $projectRoot 'tests/stubs') `
    -I (Join-Path $projectRoot 'libraries/BoardCore/src') `
    -I (Join-Path $projectRoot 'firmware/avr') `
    (Join-Path $projectRoot 'tests/front_panel_test.cpp') `
    (Join-Path $projectRoot 'firmware/avr/FrontPanel.cpp') `
    -o $panelBinary
if ($LASTEXITCODE -ne 0) { throw 'Front-panel test compilation failed' }
& $panelBinary
if ($LASTEXITCODE -ne 0) { throw 'Front-panel tests failed' }
