param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern remote-ota -Gate | Out-Null
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'build/native/s3_flash_guard_test.exe'
New-Item -ItemType Directory -Force (Split-Path -Parent $out) | Out-Null
& g++ -std=c++17 -Wall -Wextra -Werror -pthread -I (Join-Path $root 'tests/s3_flash_guard_stubs') `
 (Join-Path $root 'tests/s3_flash_guard_test.cpp') (Join-Path $root 'tests/s3_flash_guard_runtime.cpp') `
 (Join-Path $root 'firmware/esp32s3/src/LedFlashGuard.cpp') `
 '-Wl,--wrap=esp_partition_write,--wrap=esp_partition_write_raw,--wrap=esp_partition_erase_range,--wrap=esp_partition_read,--wrap=esp_partition_read_raw,--wrap=esp_image_verify' -o $out
if($LASTEXITCODE){throw 'S3 flash-boundary wrapper fixture compilation failed'}
& $out
if($LASTEXITCODE){throw 'S3 flash-boundary wrapper fixture failed'}
