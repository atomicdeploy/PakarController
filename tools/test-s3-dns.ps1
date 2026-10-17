param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern remote-ota -Gate | Out-Null
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'build/native/s3_dns_lock_test.exe'
New-Item -ItemType Directory -Force (Split-Path -Parent $out) | Out-Null
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $root 'tests/s3_dns_stubs') `
 (Join-Path $root 'tests/s3_dns_lock_test.cpp') (Join-Path $root 'tests/s3_dns_runtime.cpp') `
 (Join-Path $root 'firmware/esp32s3/src/DnsCoreLock.cpp') '-Wl,--wrap=dns_clear_cache' -o $out
if($LASTEXITCODE){throw 'S3 DNS wrapper fixture compilation failed'}
& $out
if($LASTEXITCODE){throw 'S3 DNS wrapper fixture failed'}
$recoveryOut=Join-Path $root 'build/native/s3_dns_recovery_test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $root 'tests/s3_dns_stubs') `
 (Join-Path $root 'tests/s3_dns_recovery_test.cpp') -o $recoveryOut
if($LASTEXITCODE){throw 'S3 DNS recovery fixture compilation failed'}
& $recoveryOut
if($LASTEXITCODE){throw 'S3 DNS recovery fixture failed'}
