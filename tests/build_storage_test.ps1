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
$taskEsp = @( & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp8266 -Plan )
$taskPolling = @( & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target avr -LinkMode polling -Clean -Plan )
Assert-Storage ($taskEsp.Count -eq 1 -and $taskEsp[0].Board -eq 'esp8266') 'Selective ESP build'
Assert-Storage ($taskPolling.Count -eq 1 -and $taskPolling[0].Arguments -contains '--clean') 'Explicit clean only'
Assert-Storage (($taskPolling[0].Arguments -join ' ') -match 'PAKAR_USE_BUSY_WIRE=0') 'Polling flags preserved'
$taskBuffered = @( & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target avr -LinkMode buffered -Plan )
Assert-Storage ($taskBuffered.Count -eq 1 -and ($taskBuffered[0].Arguments -join ' ') -match 'PAKAR_USE_BUSY_WIRE=0 -DPAKAR_UART_BUFFERED=1') 'Paired AVR buffered flags'
Assert-Storage (($taskBuffered[0].Arguments -join ' ') -match 'cache[\\/]avr-buffered') 'Stable buffered AVR cache'
$taskStaticS3 = @( & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp32s3 -Plan )
$taskNoBleS3 = @( & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp32s3 -S3Bluetooth disabled -Plan )
Assert-Storage ($taskNoBleS3[0].LedBackendProfile.Content -match 'PAKAR_S3_BLE_ENABLED 0') 'Explicit Bluetooth exclusion reaches the application profile'
Assert-Storage ($taskStaticS3[0].LedBackendProfile.Content -match 'PAKAR_S3_BLE_ENABLED 1') 'Runtime BLE choice remains available in the default profile'
Assert-Storage (($taskNoBleS3[0].Arguments -join ' ') -eq ($taskStaticS3[0].Arguments -join ' ')) 'BLE policy must not invalidate unrelated SDK objects'
Assert-Storage (($taskNoBleS3[0].Arguments -join ' ') -match 'ARDUINO_USB_CDC_ON_BOOT=1 -DARDUINO_USB_MODE=1') 'Bluetooth exclusion must not remove USB recovery'
try { & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp8266 -S3Bluetooth disabled -Plan; throw 'Must reject S3 Bluetooth flag on ESP8266' }
catch { Assert-Storage ($_.Exception.Message -like 'Bluetooth build selection requires ESP32-S3*') 'Bluetooth exclusion is S3-specific' }
$taskDmaS3 = @( & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp32s3 -S3LedBackend rmt-dma -Plan )
Assert-Storage ($taskStaticS3[0].LedBackendProfile.Content -match 'PAKAR_S3_LED_USE_DMA 0') 'Non-DMA remains the safe default'
Assert-Storage ($taskDmaS3[0].LedBackendProfile.Content -match 'PAKAR_S3_LED_USE_DMA 1') 'Explicit RMT-DMA build reaches the application-only profile'
Assert-Storage (($taskDmaS3[0].Arguments -join ' ') -eq ($taskStaticS3[0].Arguments -join ' ')) 'Backend changes must not invalidate global SDK/library compiler flags'
Assert-Storage ($taskDmaS3[0].LedBackendProfile.Path -eq $taskStaticS3[0].LedBackendProfile.Path) 'One stable LED profile, not another cache'
Assert-Storage (($taskDmaS3[0].Arguments -join ' ') -notmatch '--clean') 'Backend selection does not force clean or a duplicate cache'
try { & (Join-Path $taskRoot 'tools/build-firmware.ps1') -Target esp8266 -S3LedBackend rmt-dma -Plan; throw 'Must reject DMA ESP8266' }
catch { Assert-Storage ($_.Exception.Message -like 'The RMT-DMA LED backend requires ESP32-S3*') 'DMA is S3-specific' }
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
