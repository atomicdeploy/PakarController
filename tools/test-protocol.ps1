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
$shellBinary = Join-Path $outputDirectory 'telnet_shell_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'firmware/esp8266/src') (Join-Path $projectRoot 'tests/telnet_shell_test.cpp') -o $shellBinary
if ($LASTEXITCODE -ne 0) { throw 'Telnet shell compilation failed' }
& $shellBinary
if ($LASTEXITCODE -ne 0) { throw 'Telnet shell tests failed' }
$busyBinary = Join-Path $outputDirectory 'busy_line_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'tests/busy_stubs') -I (Join-Path $projectRoot 'firmware/esp8266/src') (Join-Path $projectRoot 'tests/busy_line_test.cpp') -o $busyBinary
if ($LASTEXITCODE -ne 0) { throw 'Busy line compilation failed' }
& $busyBinary
if ($LASTEXITCODE -ne 0) { throw 'Busy line tests failed' }
$toneBinary = Join-Path $outputDirectory 'tone_player_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -DF_CPU=16000000UL -I (Join-Path $projectRoot 'tests/tone_stubs') -I (Join-Path $projectRoot 'firmware/avr') (Join-Path $projectRoot 'tests/tone_player_test.cpp') (Join-Path $projectRoot 'firmware/avr/TonePlayer.cpp') (Join-Path $projectRoot 'firmware/avr/BootMelody.cpp') -o $toneBinary
if ($LASTEXITCODE -ne 0) { throw 'Tone player compilation failed' }
& $toneBinary
if ($LASTEXITCODE -ne 0) { throw 'Tone player tests failed' }
$identityBinary = Join-Path $outputDirectory 'device_identity_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'tests/access_stubs') -I (Join-Path $projectRoot 'libraries/BoardCore/src') -I (Join-Path $projectRoot 'firmware/esp8266/src') (Join-Path $projectRoot 'tests/device_identity_test.cpp') (Join-Path $projectRoot 'firmware/esp8266/src/DeviceIdentity.cpp') -o $identityBinary
if ($LASTEXITCODE -ne 0) { throw 'Identity test compilation failed' }
& $identityBinary
if ($LASTEXITCODE -ne 0) { throw 'Identity tests failed' }
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
