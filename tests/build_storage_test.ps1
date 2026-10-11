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
$taskManifest = Get-Content -LiteralPath (Join-Path $taskRoot 'desktop/Cargo.toml') -Raw
Assert-Storage ($taskManifest -match '(?s)\[profile.dev\]\s+debug = 0\s+incremental = true') 'Low-symbol dev profile'
Assert-Storage ($taskManifest -match '(?s)\[profile.test\]\s+debug = 0\s+incremental = true') 'Low-symbol test profile'
$taskShell = Get-Content -LiteralPath (Join-Path $taskRoot 'build.sh') -Raw
Assert-Storage (-not ($taskShell -match 'arduino-cli compile --clean')) 'Shell must not force clean'
Assert-Storage (($taskShell -split '--build-path').Count -eq 3) 'Shell stable cache for both boards'
Write-Host 'Build storage dispatch, parsing, locked canonical cache, budgets, selective firmware and explicit clean tests passed; no compiler was run.'
