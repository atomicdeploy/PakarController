$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern peer-binding -Gate | Out-Null
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskOutputDirectory = Join-Path $taskRoot 'build/native'
New-Item -ItemType Directory -Force -Path $taskOutputDirectory | Out-Null
$taskOutput = Join-Path $taskOutputDirectory 'web_access_fence_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -pthread `
    -I (Join-Path $taskRoot 'tests/access_stubs') `
    -I (Join-Path $taskRoot 'libraries/BoardCore/src') `
    -I (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src') `
    -I (Join-Path $taskRoot 'firmware/esp8266/src') `
    (Join-Path $taskRoot 'tests/web_access_fence_test.cpp') `
    (Join-Path $taskRoot 'firmware/esp8266/src/AccessConfig.cpp') -o $taskOutput
if ($LASTEXITCODE -ne 0) { throw 'WebAccess epoch middleware test compilation failed' }
& $taskOutput
if ($LASTEXITCODE -ne 0) { throw 'WebAccess epoch middleware tests failed' }
$taskPersistence = Join-Path $taskOutputDirectory 'access_config_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror `
    -I (Join-Path $taskRoot 'tests/access_stubs') `
    -I (Join-Path $taskRoot 'libraries/BoardCore/src') `
    -I (Join-Path $taskRoot 'firmware/esp8266/src') `
    (Join-Path $taskRoot 'tests/access_config_test.cpp') `
    (Join-Path $taskRoot 'firmware/esp8266/src/AccessConfig.cpp') -o $taskPersistence
if ($LASTEXITCODE -ne 0) { throw 'Access persistence test compilation failed' }
& $taskPersistence
if ($LASTEXITCODE -ne 0) { throw 'Access persistence tests failed' }
