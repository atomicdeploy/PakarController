param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern network-cloud | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/tls_memory_test.exe'
New-Item -ItemType Directory -Force (Split-Path -Parent $taskOutput) | Out-Null
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror `
 -I (Join-Path $taskRoot 'tests/tls_memory_stubs') `
 (Join-Path $taskRoot 'tests/tls_memory_test.cpp') `
 (Join-Path $taskRoot 'tests/tls_memory_runtime.cpp') `
 (Join-Path $taskRoot 'firmware/esp32s3/src/TlsMemory.cpp') `
 '-Wl,--wrap=esp_mbedtls_mem_calloc' -o $taskOutput
if($LASTEXITCODE){throw 'TLS memory adapter fixture compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'TLS memory adapter fixture assertions failed'}
Write-Host 'Linked TLS allocator fixture passed: PSRAM record-sized allocations, small/internal and no-PSRAM/failure fallback, overflow and zero-size behavior. Not board TLS proof.'
