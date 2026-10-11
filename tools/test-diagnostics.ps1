[CmdletBinding()]
param([switch]$Plan)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern error-management -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
# This module is std-only. Reuse one tiny native test output instead of linking
# the full GUI again for a cause-formatting refinement. No debug symbols/cache.
$taskOutput=Join-Path $taskRoot 'build/native/diagnostics-test.exe'
$taskArguments=@('--test','--edition=2024','-C','debuginfo=0',
  (Join-Path $taskRoot 'desktop/src/diagnostics.rs'),'-o',$taskOutput)
if($Plan){[pscustomobject]@{Executable='rustc';Arguments=$taskArguments;Scope='Pure incident/error-chain contracts only, not native GUI or physical reconnect'};return}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
& rustc @taskArguments
if($LASTEXITCODE){throw 'Diagnostic contract compilation failed'}
& $taskOutput --test-threads=1
if($LASTEXITCODE){throw 'Diagnostic contract assertions failed'}
