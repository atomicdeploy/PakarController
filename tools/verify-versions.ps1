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
$taskMobilePeer=[IO.File]::ReadAllText((Join-Path $root 'mobile/lib/peer_identity.dart'))
if([regex]::Match($taskMobilePeer,"peerDefaultVersion = '([^']+)'").Groups[1].Value -ne $version -or
   [regex]::Match($taskMobilePeer,"peerDefaultBuild = '([^']+)'").Groups[1].Value -ne $taskMobileBuild){throw 'Mobile peer header version/build differs from actual package'}
$taskPolicy=Get-Content -LiteralPath (Join-Path $root 'interfaces/peer-policy.json') -Raw | ConvertFrom-Json
$taskCorePolicy=[IO.File]::ReadAllText((Join-Path $root 'libraries/BoardCore/src/PeerCompatibility.h'))
$taskMobilePolicy=[IO.File]::ReadAllText((Join-Path $root 'mobile/lib/peer_policy.dart'))
if([regex]::Match($taskCorePolicy,'#define PAKAR_MIN_APP_VERSION "([^"]+)"').Groups[1].Value -ne $taskPolicy.minimum_app -or
   [regex]::Match($taskCorePolicy,'#define PAKAR_MIN_BOARD_VERSION "([^"]+)"').Groups[1].Value -ne $taskPolicy.minimum_board -or
   [regex]::Match($taskMobilePolicy,"minimumPeerApp = '([^']+)'").Groups[1].Value -ne $taskPolicy.minimum_app -or
   [regex]::Match($taskMobilePolicy,"minimumPeerBoard = '([^']+)'").Groups[1].Value -ne $taskPolicy.minimum_board){throw 'Compiled peer-policy defaults differ from the shared support policy'}
Write-Host "ESP/AVR/native/Android source versions match: $version. This does not verify deployed device versions."
