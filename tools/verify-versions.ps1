$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$firmware = [IO.File]::ReadAllText((Join-Path $root 'libraries/BoardCore/src/BuildInfo.h'))
$cargo = [IO.File]::ReadAllText((Join-Path $root 'desktop/Cargo.toml'))
$lock = [IO.File]::ReadAllText((Join-Path $root 'desktop/Cargo.lock'))
$mobile = [IO.File]::ReadAllText((Join-Path $root 'mobile/pubspec.yaml'))
$version = [regex]::Match($firmware, 'kVersion\[\]\s*=\s*"(\d+\.\d+\.\d+)"').Groups[1].Value
if (-not $version) { throw 'Firmware version is absent' }
$parts = $version.Split('.')
for ($index=0; $index -lt 3; ++$index) {
    $name = @('Major','Minor','Patch')[$index]
    if (-not [regex]::IsMatch($firmware, "kVersion$name\s*=\s*$($parts[$index]);")) { throw "Firmware numeric $name mismatch" }
}
if ([regex]::Match($cargo, '(?m)^version\s*=\s*"([^"]+)"').Groups[1].Value -ne $version) { throw 'Native Cargo version differs from firmware' }
if ([regex]::Match($lock, '(?s)name = "pakarcontroller"\s+version = "([^"]+)"').Groups[1].Value -ne $version) { throw 'Native lockfile version mismatch' }
if ([regex]::Match($mobile, '(?m)^version:\s*(\d+\.\d+\.\d+)').Groups[1].Value -ne $version) { throw 'Mobile version differs from firmware/native' }
$taskClientIdentity=Get-Content -LiteralPath (Join-Path $root 'interfaces/client-release.json') -Raw | ConvertFrom-Json
$taskMobileBuild=[regex]::Match($mobile, '(?m)^version:\s*\d+\.\d+\.\d+\+(\d+)').Groups[1].Value
if($taskClientIdentity.product -ne 'PakarController' -or $taskClientIdentity.version -ne $version -or $taskClientIdentity.build -lt 1 -or [string]$taskClientIdentity.build -ne $taskMobileBuild){throw 'Native/Android client release build identities differ'}
Write-Host "ESP/AVR/native/Android source versions match: $version. This does not verify deployed device versions."
