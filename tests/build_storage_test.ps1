$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
function Assert-Storage($condition, $message) { if (-not $condition) { throw $message } }
foreach ($taskFile in @('tools/build-desktop.ps1', 'tools/build-firmware.ps1', 'tools/stage-desktop.ps1')) {
    $taskTokens=$null; $taskErrors=$null
    [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $taskRoot $taskFile), [ref]$taskTokens, [ref]$taskErrors)
    Assert-Storage ($taskErrors.Count -eq 0) "Invalid PowerShell: $taskFile"
}
$taskCheck = & (Join-Path $taskRoot 'tools/build-desktop.ps1') -Plan
$taskTest = & (Join-Path $taskRoot 'tools/build-desktop.ps1') -Mode Test -Plan
$taskRelease = & (Join-Path $taskRoot 'tools/build-desktop.ps1') -Mode Release -Plan
Assert-Storage ($taskCheck.Arguments[0] -eq 'check') 'Default must not link an executable'
Assert-Storage ($taskTest.Arguments[0] -eq 'test') 'Test dispatch'
Assert-Storage ($taskRelease.Arguments -contains '--release') 'Release remains explicit'
Assert-Storage ($taskCheck.Cache -eq $taskTest.Cache -and $taskTest.Cache -eq $taskRelease.Cache) 'One canonical target'
Assert-Storage ($taskTest.Arguments -contains '--locked') 'Locked dependencies'
Assert-Storage (($taskTest.Arguments -join ' ') -match '--bin pakarcontroller --release -- --test-threads=1 --nocapture$') 'Actual donor serial test contract with immediate failure evidence'
Assert-Storage ($taskCheck.Arguments -contains '--release' -and $taskTest.Arguments -contains '--release') 'Routine checks/tests reuse production dependency artifacts, not obsolete debug output'
$taskDebug = & (Join-Path $taskRoot 'tools/build-desktop.ps1') -Mode Test -DebugArtifacts -Plan
Assert-Storage ($taskDebug.DebugArtifacts -and -not ($taskDebug.Arguments -contains '--release')) 'Debug artifacts require essential-diagnostics opt-in'
Assert-Storage (-not ($taskCheck.Arguments -contains '--test-threads=1') -and -not ($taskRelease.Arguments -contains '--test-threads=1')) 'Runtime test flags cannot change compile/release profiles'
Assert-Storage ($taskTest.Arguments -contains '--jobs' -and $taskTest.Arguments -contains '2') 'Compile jobs remain independently bounded'
Assert-Storage ($taskTest.TestConfigurationFile -eq $(if ($env:PAKAR_CONFIG_FILE) { $env:PAKAR_CONFIG_FILE } else { Join-Path $taskRoot 'build/native/native-tests.json' })) 'Stable isolated test config or explicit fixture override'
Assert-Storage ($null -eq $taskCheck.TestConfigurationFile -and $null -eq $taskRelease.TestConfigurationFile) 'Test configuration never leaks into check/release'
Assert-Storage ($taskCheck.MinimumFreeGiB -eq 8 -and $taskCheck.MaximumCacheGiB -eq 8) 'Storage budgets'
# Both entrypoints construct the real CLI arguments; Plan never compiles/writes.
$taskFirmware = @( & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Plan )
Assert-Storage ($taskFirmware.Count -eq 2) 'Both boards by default'
foreach ($taskBoard in $taskFirmware) {
    Assert-Storage ($taskBoard.Arguments -contains '--build-path') 'Stable Arduino build path'
    Assert-Storage (-not ($taskBoard.Arguments -contains '--clean')) 'No forced clean by default'
}
$taskAvrPlan=$taskFirmware | Where-Object Board -eq 'avr'
Assert-Storage (($taskAvrPlan.Arguments -join ' ') -match 'LTO=Os_flto') 'AVR retains selected bootloader-safe MiniCore LTO profile'
$taskCppFlags=@($taskAvrPlan.Arguments | Where-Object { $_ -like 'compiler.cpp.extra_flags=*' })
Assert-Storage ($taskCppFlags.Count -eq 1 -and $taskCppFlags[0] -match '(?:=|\s)-flto(?:\s|$)') 'Custom AVR macros must not erase the selected MiniCore C++ LTO option'
$taskEsp = @( & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp8266 -Plan )
$taskPolling = @( & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target avr -LinkMode polling -Clean -Plan )
Assert-Storage ($taskEsp.Count -eq 1 -and $taskEsp[0].Board -eq 'esp8266') 'Selective ESP build'
Assert-Storage ($taskPolling.Count -eq 1 -and $taskPolling[0].Arguments -contains '--clean') 'Explicit clean only'
Assert-Storage (($taskPolling[0].Arguments -join ' ') -match 'PAKAR_USE_BUSY_WIRE=0') 'Polling flags preserved'
$taskBuffered = @( & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target avr -LinkMode buffered -Plan )
Assert-Storage ($taskBuffered.Count -eq 1 -and ($taskBuffered[0].Arguments -join ' ') -match 'PAKAR_USE_BUSY_WIRE=0 -DPAKAR_UART_BUFFERED=1') 'Paired AVR buffered flags'
Assert-Storage (($taskBuffered[0].Arguments -join ' ') -match 'cache[\\/]avr-buffered') 'Stable buffered AVR cache'
foreach($taskCase in @(@('first-wins','auto',0),@('table-selection','auto',1),@('first-wins','enabled',1),@('table-selection','disabled',0))) {
    $taskGate = & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target avr -LinkMode buffered -GameMode $taskCase[0] -AvrAutomations $taskCase[1] -Plan
    Assert-Storage (($taskGate.Arguments -join ' ') -match "-DPAKAR_PC_AUTOMATIONS=$($taskCase[2])(?:\s|$)") "AVR automation profile $taskCase"
}
$taskRadio = & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target avr -LinkMode buffered -GameMode first-wins -AvrRadio -Plan
Assert-Storage (($taskRadio.Arguments -join ' ') -match '-DPAKAR_PC_RF=1') 'Explicit raw RF adapter'
Assert-Storage (($taskBuffered[0].Arguments -join ' ') -notmatch '-DPAKAR_PC_RF=1') 'RF remains opt-in until physical acceptance'
$taskStaticS3 = @( & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp32s3 -Plan )
$taskNoBleS3 = @( & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp32s3 -S3Bluetooth disabled -Plan )
Assert-Storage ($taskNoBleS3[0].LedBackendProfile.Content -match 'PAKAR_S3_BLE_ENABLED 0') 'Explicit Bluetooth exclusion reaches the application profile'
Assert-Storage ($taskStaticS3[0].LedBackendProfile.Content -match 'PAKAR_S3_BLE_ENABLED 1') 'Runtime BLE choice remains available in the default profile'
Assert-Storage (($taskNoBleS3[0].Arguments -join ' ') -eq ($taskStaticS3[0].Arguments -join ' ')) 'BLE policy must not invalidate unrelated SDK objects'
Assert-Storage (($taskNoBleS3[0].Arguments -join ' ') -match 'CDCOnBoot=cdc,USBMode=hwcdc') 'Bluetooth exclusion must not remove board-selected USB recovery'
Assert-Storage (($taskNoBleS3[0].Arguments -join ' ') -match 'PAKAR_S3_HOST_TRANSPORT=0') 'Default command path remains the USB-to-UART connector'
$taskFourBoxS3 = @( & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp32s3 -GameMode first-wins -InitialKeyMask 15 -DeviceName keycontroller4 -Plan )
Assert-Storage ($taskFourBoxS3[0].LedBackendProfile.Content -match '(?m)^#define PAKAR_GAME_MODE 2$') 'Four-box profile must remain first-wins'
Assert-Storage ($taskFourBoxS3[0].LedBackendProfile.Content -match '(?m)^#define PAKAR_INITIAL_KEY_MASK 15$') 'Four-box profile must boot with exactly boxes one through four enabled'
Assert-Storage ($taskFourBoxS3[0].LedBackendProfile.Content -match '(?m)^#define PAKAR_INITIAL_NAME keycontroller4$') 'Four-box profile must keep its unique hostname'
Assert-Storage (($taskFourBoxS3[0].Arguments -join ' ') -notmatch 'PAKAR_INITIAL_NAME=') 'Changing an S3 hostname must not invalidate every SDK/library object'
$taskProductionS3 = @( & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp32s3 -S3FlashMB 16 -S3Psram opi -Plan )
$taskProductionCache=$taskProductionS3[0].Arguments[[Array]::IndexOf($taskProductionS3[0].Arguments,'--build-path')+1]
$taskRecoveryCache=$taskStaticS3[0].Arguments[[Array]::IndexOf($taskStaticS3[0].Arguments,'--build-path')+1]
Assert-Storage ($taskProductionCache -ne $taskRecoveryCache) 'Different hardware profiles cannot evict one another, including an adopted matching warm cache'
Assert-Storage ($taskProductionCache -match 'cache[\\/]esp32s3-buffered(?:-f16m-psram-opi-host-uart0-wifi)?$') 'Production hardware profile owns its keyed or exact-matching adopted cache'
try { & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp8266 -S3Bluetooth disabled -Plan; throw 'Must reject S3 Bluetooth flag on ESP8266' }
catch { Assert-Storage ($_.Exception.Message -like 'Bluetooth build selection requires ESP32-S3*') 'Bluetooth exclusion is S3-specific' }
$taskDmaS3 = @( & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp32s3 -S3LedGpio -S3LedRmtDma -Plan )
Assert-Storage ($taskStaticS3[0].LedBackendProfile.Content -match 'PAKAR_S3_LED_GPIO_ENABLED 0' -and $taskStaticS3[0].LedBackendProfile.Content -match 'PAKAR_S3_LED_RMT_ENABLED 0') 'No explicit driver flags means no strip GPIO control'
Assert-Storage ($taskDmaS3[0].LedBackendProfile.Content -match 'PAKAR_S3_LED_GPIO_ENABLED 1' -and $taskDmaS3[0].LedBackendProfile.Content -match 'PAKAR_S3_LED_RMT_ENABLED 1' -and $taskDmaS3[0].LedBackendProfile.Content -match 'PAKAR_S3_LED_DEFAULT_BACKEND 1') 'Both drivers, default GPIO, one image'
Assert-Storage (($taskDmaS3[0].Arguments -join ' ') -eq ($taskStaticS3[0].Arguments -join ' ')) 'Backend changes must not invalidate global SDK/library compiler flags'
Assert-Storage ($taskDmaS3[0].LedBackendProfile.Path -eq $taskStaticS3[0].LedBackendProfile.Path) 'One stable LED profile, not another cache'
Assert-Storage (($taskDmaS3[0].Arguments -join ' ') -notmatch '--clean') 'Backend selection does not force clean or a duplicate cache'
try { & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp8266 -S3LedRmtDma -Plan; throw 'Must reject DMA ESP8266' }
catch { Assert-Storage ($_.Exception.Message -like 'LED driver build flags require ESP32-S3*') 'DMA is S3-specific' }
try { & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp32s3 -S3LedGpio -S3LedDefault rmt-dma -Plan; throw 'Must reject unavailable default' }
catch { Assert-Storage ($_.Exception.Message -like 'Default LED driver must be explicitly included*') 'Default cannot bypass inclusion flags' }
try { & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp8266 -LinkMode buffered -Plan; throw 'Must reject buffered ESP8266' }
catch { Assert-Storage ($_.Exception.Message -like 'Buffered mode currently targets*') 'Do not accidentally enable buffered ESP8266' }
$taskManifest = Get-Content -LiteralPath (Join-Path $taskRoot 'desktop/Cargo.toml') -Raw
Assert-Storage ($taskManifest -match '(?s)\[profile.dev\]\s+debug = 0\s+incremental = true') 'Low-symbol dev profile'
Assert-Storage ($taskManifest -match '(?s)\[profile.test\]\s+debug = 0\s+incremental = true') 'Low-symbol test profile'
$taskShell = Get-Content -LiteralPath (Join-Path $taskRoot 'build.sh') -Raw
Assert-Storage (-not ($taskShell -match 'arduino-cli compile --clean')) 'Shell must not force clean'
Assert-Storage ($taskShell -match 'pakar.sh.*build.*\$@') 'Shell delegates to the same reviewed owner instead of a drifting second compiler implementation'
Assert-Storage ($taskShell -notmatch 'arduino-cli compile') 'No duplicate shell-side compiler plan'
Write-Host 'Build storage dispatch, parsing, locked canonical cache, budgets, selective firmware and explicit clean tests passed; no compiler was run.'
