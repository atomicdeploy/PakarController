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
& (Join-Path $PSScriptRoot 'test-panel.ps1')
& (Join-Path $PSScriptRoot 'test-led-pipeline.ps1')
& (Join-Path $PSScriptRoot 'test-led-worker.ps1')
& (Join-Path $PSScriptRoot 'test-led-rmt-ring.ps1')
$mndpBinary = Join-Path $outputDirectory 'mndp_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror (Join-Path $projectRoot 'tests/mndp_test.cpp') -o $mndpBinary
if ($LASTEXITCODE -ne 0) { throw 'MNDP packet compilation failed' }
& $mndpBinary
if ($LASTEXITCODE -ne 0) { throw 'MNDP packet tests failed' }
$pixelBinary = Join-Path $outputDirectory 'led_grid_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'tests/pixel_stubs') -I (Join-Path $projectRoot 'libraries/BoardCore/src') -I (Join-Path $projectRoot 'firmware/esp8266/src') (Join-Path $projectRoot 'tests/led_grid_test.cpp') (Join-Path $projectRoot 'firmware/esp8266/src/LedGrid.cpp') -o $pixelBinary
if ($LASTEXITCODE -ne 0) { throw 'LED pixel snapshot compilation failed' }
& $pixelBinary
if ($LASTEXITCODE -ne 0) { throw 'LED pixel snapshot tests failed' }
$remoteBinary = Join-Path $outputDirectory 'remote_policy_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror (Join-Path $projectRoot 'tests/remote_policy_test.cpp') -o $remoteBinary
if ($LASTEXITCODE -ne 0) { throw 'Remote policy compilation failed' }
& $remoteBinary
if ($LASTEXITCODE -ne 0) { throw 'Remote policy tests failed' }
$ispBinary = Join-Path $outputDirectory 'avr_isp_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'tests/isp_stubs') (Join-Path $projectRoot 'tests/avr_isp_test.cpp') (Join-Path $projectRoot 'firmware/esp8266/src/AvrIsp.cpp') -o $ispBinary
if ($LASTEXITCODE -ne 0) { throw 'ISP driver compilation failed' }
& $ispBinary
if ($LASTEXITCODE -ne 0) { throw 'ISP driver tests failed' }
& (Join-Path $PSScriptRoot 'test-urboot.ps1')
$clientBinary = Join-Path $outputDirectory 'client_telemetry_test.exe'
$jsonInclude = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
foreach($taskClientPlatform in @('generic','esp8266')) {
    $taskClientFlags=@()
    if($taskClientPlatform -eq 'esp8266'){$taskClientFlags=@('-DARDUINO_ARCH_ESP8266')}
    & g++ -std=c++17 -Wall -Wextra -Werror @taskClientFlags -I (Join-Path $projectRoot 'tests/access_stubs') -I $jsonInclude -I (Join-Path $projectRoot 'libraries/BoardCore/src') (Join-Path $projectRoot 'tests/client_telemetry_test.cpp') -o $clientBinary
    if ($LASTEXITCODE -ne 0) { throw "Client telemetry compilation failed ($taskClientPlatform; ArduinoJson 7.4.3 is required)" }
    & $clientBinary
    if ($LASTEXITCODE -ne 0) { throw "Client telemetry tests failed ($taskClientPlatform)" }
}
$remoteStateBinary = Join-Path $outputDirectory 'remote_firmware_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'tests/remote_stubs') -I $jsonInclude -I (Join-Path $projectRoot 'libraries/BoardCore/src') (Join-Path $projectRoot 'tests/remote_firmware_test.cpp') (Join-Path $projectRoot 'firmware/esp8266/src/RemoteFirmware.cpp') -lbcrypt -o $remoteStateBinary
if ($LASTEXITCODE -ne 0) { throw 'Remote firmware state-machine compilation failed' }
& $remoteStateBinary
if ($LASTEXITCODE -ne 0) { throw 'Remote firmware state-machine tests failed' }
& (Join-Path $PSScriptRoot 'test-remote-s3.ps1')
& (Join-Path $PSScriptRoot 'test-s3-dns.ps1')
& (Join-Path $PSScriptRoot 'test-s3-flash-guard.ps1')
# These focused runners retain their own reviewed-slice gates and reuse the
# canonical build/native outputs. ArduinoJson is installed by firmware CI.
& (Join-Path $PSScriptRoot 'test-cloud-command.ps1')
& (Join-Path $PSScriptRoot 'test-cloud-config.ps1')
& (Join-Path $PSScriptRoot 'test-cloud-services.ps1')
& (Join-Path $PSScriptRoot 'test-cloud-management.ps1')
& (Join-Path $PSScriptRoot 'test-command-values.ps1')
& (Join-Path $PSScriptRoot 'test-ethernet.ps1')
& (Join-Path $PSScriptRoot 'test-lldp.ps1')
& (Join-Path $PSScriptRoot 'test-ethernet-config.ps1')
& (Join-Path $PSScriptRoot 'test-ethernet-management.ps1')
& (Join-Path $PSScriptRoot 'test-network-path.ps1')
& node (Join-Path $projectRoot 'tests/ethernet_discovery_test.mjs')
if ($LASTEXITCODE -ne 0) { throw 'Ethernet discovery integration tests failed' }
& node (Join-Path $projectRoot 'tests/esp_only_maintenance_test.mjs')
if ($LASTEXITCODE -ne 0) { throw 'ESP-only maintenance integration tests failed' }
$shellBinary = Join-Path $outputDirectory 'telnet_shell_test.exe'
& (Join-Path $PSScriptRoot 'test-shell.ps1')
$busyBinary = Join-Path $outputDirectory 'busy_line_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'tests/busy_stubs') -I (Join-Path $projectRoot 'firmware/esp8266/src') (Join-Path $projectRoot 'tests/busy_line_test.cpp') -o $busyBinary
if ($LASTEXITCODE -ne 0) { throw 'Busy line compilation failed' }
& $busyBinary
if ($LASTEXITCODE -ne 0) { throw 'Busy line tests failed' }
$toneBinary = Join-Path $outputDirectory 'tone_player_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -DF_CPU=16000000UL -I (Join-Path $projectRoot 'libraries/BoardCore/src') -I (Join-Path $projectRoot 'tests/tone_stubs') -I (Join-Path $projectRoot 'firmware/avr') (Join-Path $projectRoot 'tests/tone_player_test.cpp') (Join-Path $projectRoot 'firmware/avr/TonePlayer.cpp') (Join-Path $projectRoot 'firmware/avr/BootMelody.cpp') -o $toneBinary
if ($LASTEXITCODE -ne 0) { throw 'Tone player compilation failed' }
& $toneBinary
if ($LASTEXITCODE -ne 0) { throw 'Tone player tests failed' }
$identityBinary = Join-Path $outputDirectory 'device_identity_test.exe'
foreach ($identityFlags in @('-DPAKAR_TABLE_ENABLED=1','-DPAKAR_TABLE_ENABLED=0','-DPAKAR_IDENTITY_CUSTOM=1')) {
    $applicationBinary = Join-Path $outputDirectory 'application_identity_test.exe'
    & g++ -std=c++17 -Wall -Wextra -Werror $identityFlags -I (Join-Path $projectRoot 'tests/access_stubs') -I (Join-Path $projectRoot 'libraries/BoardCore/src') -I (Join-Path $projectRoot 'firmware/esp8266/src') (Join-Path $projectRoot 'tests/application_identity_test.cpp') -o $applicationBinary
    if ($LASTEXITCODE -ne 0) { throw 'Application identity profile compilation failed' }
    & $applicationBinary
    if ($LASTEXITCODE -ne 0) { throw 'Application identity profile tests failed' }
}
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'tests/access_stubs') -I (Join-Path $projectRoot 'libraries/BoardCore/src') -I (Join-Path $projectRoot 'firmware/esp8266/src') (Join-Path $projectRoot 'tests/device_identity_test.cpp') (Join-Path $projectRoot 'firmware/esp8266/src/DeviceIdentity.cpp') -o $identityBinary
if ($LASTEXITCODE -ne 0) { throw 'Identity test compilation failed' }
& $identityBinary
if ($LASTEXITCODE -ne 0) { throw 'Identity tests failed' }
$accessBinary = Join-Path $outputDirectory 'access_policy_test.exe'
$contextBinary = Join-Path $outputDirectory 'device_context_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'libraries/BoardCore/src') (Join-Path $projectRoot 'tests/device_context_test.cpp') -o $contextBinary
if ($LASTEXITCODE -ne 0) { throw 'Device context compilation failed' }
& $contextBinary
if ($LASTEXITCODE -ne 0) { throw 'Device context tests failed' }
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
& g++ -std=c++17 -pthread -Wall -Wextra -Werror -I (Join-Path $projectRoot 'tests/access_stubs') -I (Join-Path $projectRoot 'firmware/esp8266/src') (Join-Path $projectRoot 'tests/async_http_test.cpp') -o $asyncBinary
if ($LASTEXITCODE -ne 0) { throw 'Async HTTP test compilation failed' }
& $asyncBinary
if ($LASTEXITCODE -ne 0) { throw 'Async HTTP tests failed' }
$rangeBinary = Join-Path $outputDirectory 'firmware_range_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'tests/access_stubs') -I (Join-Path $projectRoot 'firmware/esp8266/src') (Join-Path $projectRoot 'tests/firmware_range_test.cpp') -o $rangeBinary
if ($LASTEXITCODE -ne 0) { throw 'Firmware range test compilation failed' }
& $rangeBinary
if ($LASTEXITCODE -ne 0) { throw 'Firmware range tests failed' }
$firmwareReadBinary=Join-Path $outputDirectory 'firmware_read_response_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $projectRoot 'tests/firmware_transfer_stubs') (Join-Path $projectRoot 'tests/firmware_read_response_test.cpp') -o $firmwareReadBinary
if($LASTEXITCODE){throw 'Firmware reader response compilation failed'}
& $firmwareReadBinary
if($LASTEXITCODE){throw 'Firmware reader response fixtures failed'}
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
$pcBinary = Join-Path $outputDirectory 'pc_contract_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror `
    -I (Join-Path $projectRoot 'tests/pc_stubs') `
    -I (Join-Path $projectRoot 'libraries/BoardCore/src') `
    -I (Join-Path $projectRoot 'firmware/avr') `
    (Join-Path $projectRoot 'tests/pc_contract_test.cpp') `
    (Join-Path $projectRoot 'firmware/avr/PcControllerRuntime.cpp') `
    (Join-Path $projectRoot 'firmware/avr/BoardSettings.cpp') `
    (Join-Path $projectRoot 'firmware/avr/SevenSegments.cpp') `
    (Join-Path $projectRoot 'libraries/BoardCore/src/pccontroller/MacroRing.cpp') `
    (Join-Path $projectRoot 'libraries/BoardCore/src/pccontroller/Keys.cpp') `
    (Join-Path $projectRoot 'libraries/BoardCore/src/pccontroller/ControllerEvents.cpp') `
    (Join-Path $projectRoot 'libraries/BoardCore/src/pccontroller/AutomationStore.cpp') `
    -o $pcBinary
if ($LASTEXITCODE -ne 0) { throw 'PCController contract compilation failed' }
& $pcBinary
if ($LASTEXITCODE -ne 0) { throw 'PCController contract tests failed' }
$espPanelBinary = Join-Path $outputDirectory 'esp_front_panel_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror `
    -I (Join-Path $projectRoot 'tests/pc_stubs') `
    -I (Join-Path $projectRoot 'libraries/BoardCore/src') `
    -I (Join-Path $projectRoot 'firmware/avr') `
    -I (Join-Path $projectRoot 'firmware/esp8266/src') `
    (Join-Path $projectRoot 'tests/esp_front_panel_test.cpp') `
    (Join-Path $projectRoot 'firmware/avr/PcControllerRuntime.cpp') `
    (Join-Path $projectRoot 'firmware/avr/BoardSettings.cpp') `
    (Join-Path $projectRoot 'firmware/avr/SevenSegments.cpp') `
    (Join-Path $projectRoot 'libraries/BoardCore/src/pccontroller/MacroRing.cpp') `
    (Join-Path $projectRoot 'libraries/BoardCore/src/pccontroller/Keys.cpp') `
    (Join-Path $projectRoot 'libraries/BoardCore/src/pccontroller/ControllerEvents.cpp') `
    (Join-Path $projectRoot 'libraries/BoardCore/src/pccontroller/AutomationStore.cpp') `
    -o $espPanelBinary
if ($LASTEXITCODE -ne 0) { throw 'ESP front-panel contract compilation failed' }
& $espPanelBinary
if ($LASTEXITCODE -ne 0) { throw 'ESP front-panel contract tests failed' }
foreach ($rgbCase in @('s3_status_rgb_test', 's3_status_rgb_config_test', 's3_passive_avr_rx_test')) {
    $rgbBinary = Join-Path $outputDirectory "$rgbCase.exe"
    & g++ -std=c++17 -Wall -Wextra -Werror `
        -I (Join-Path $projectRoot 'tests/access_stubs') `
        -I (Join-Path $projectRoot 'firmware/esp32s3/src') `
        (Join-Path $projectRoot "tests/$rgbCase.cpp") -o $rgbBinary
    if ($LASTEXITCODE -ne 0) { throw "$rgbCase compilation failed" }
    & $rgbBinary
    if ($LASTEXITCODE -ne 0) { throw "$rgbCase assertions failed" }
}
& (Join-Path $projectRoot 'tests/device_network_script_test.ps1')
& (Join-Path $projectRoot 'tests/avr_rx_script_test.ps1')
& (Join-Path $PSScriptRoot 'test-display.ps1')
& (Join-Path $PSScriptRoot 'test-s3-contract.ps1')
& (Join-Path $PSScriptRoot 'test-uart-flow.ps1')
& (Join-Path $PSScriptRoot 'test-web-access.ps1')
& (Join-Path $PSScriptRoot 'test-tasks.ps1')
