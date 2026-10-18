$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$plan=& (Join-Path $root 'tools/build-mobile.ps1') -Mode PixelProbe -Plan
if($plan.Executable -notmatch 'dart(?:\.bat)?$' -or $plan.Commands.Count -ne 1){throw 'Probe must use one standalone Dart command'}
$argsText=($plan.Commands[0] -join ' ')
if($argsText -notmatch '^compile exe ' -or $argsText -notmatch 'tool/pixel_probe.dart' -or $argsText -notmatch 'build/pixel-probe' -or $argsText -match 'pub get|flutter build|test '){throw 'Probe must reuse existing mobile packages/cache and never dispatch device commands during build'}
if($plan.Signing -notmatch 'not an Android package'){throw 'Probe packaging must not imply phone installation/signing'}
Write-Output 'Mobile pixel probe: canonical compile-only plan, packages/cache reused, no device action.'
