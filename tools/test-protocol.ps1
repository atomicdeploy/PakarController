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
$accessBinary = Join-Path $outputDirectory 'access_policy_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'libraries/BoardCore/src') (Join-Path $projectRoot 'tests/access_policy_test.cpp') -o $accessBinary
if ($LASTEXITCODE -ne 0) { throw 'Access test compilation failed' }
& $accessBinary
if ($LASTEXITCODE -ne 0) { throw 'Access tests failed' }
$accessConfigBinary = Join-Path $outputDirectory 'access_config_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'tests/access_stubs') -I (Join-Path $projectRoot 'libraries/BoardCore/src') -I (Join-Path $projectRoot 'firmware/esp8266/src') (Join-Path $projectRoot 'tests/access_config_test.cpp') (Join-Path $projectRoot 'firmware/esp8266/src/AccessConfig.cpp') -o $accessConfigBinary
if ($LASTEXITCODE -ne 0) { throw 'Access persistence test compilation failed' }
& $accessConfigBinary
if ($LASTEXITCODE -ne 0) { throw 'Access persistence tests failed' }
$asyncBinary = Join-Path $outputDirectory 'async_http_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'tests/access_stubs') -I (Join-Path $projectRoot 'firmware/esp8266/src') (Join-Path $projectRoot 'tests/async_http_test.cpp') -o $asyncBinary
if ($LASTEXITCODE -ne 0) { throw 'Async HTTP test compilation failed' }
& $asyncBinary
if ($LASTEXITCODE -ne 0) { throw 'Async HTTP tests failed' }
$rangeBinary = Join-Path $outputDirectory 'firmware_range_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'tests/access_stubs') -I (Join-Path $projectRoot 'firmware/esp8266/src') (Join-Path $projectRoot 'tests/firmware_range_test.cpp') -o $rangeBinary
if ($LASTEXITCODE -ne 0) { throw 'Firmware range test compilation failed' }
& $rangeBinary
if ($LASTEXITCODE -ne 0) { throw 'Firmware range tests failed' }
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
