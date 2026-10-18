[CmdletBinding()]
param(
    [string]$Image=(Join-Path $PSScriptRoot '../build/esp32s3-buffered/esp32s3.ino.bin'),
    [string]$Origin='http://ledboard.local',
    [ValidateSet('auto','local','remote')][string]$Mode='auto',
    [string]$Manifest,
    [string]$ManifestSha,
    [ValidateRange(10,600)][int]$TimeoutSeconds=180,
    [switch]$Run
)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern remote-ota -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern api-contract -Gate | Out-Null
$taskArguments=@((Join-Path $PSScriptRoot 'update-firmware.mjs'),'--image',$Image,'--origin',$Origin,'--mode',$Mode,'--timeout',"$TimeoutSeconds")
if($Manifest){$taskArguments+=@('--manifest',$Manifest)}
if($ManifestSha){$taskArguments+=@('--manifest-sha',$ManifestSha)}
if($Run){$taskArguments+='--run'}
# Windows PowerShell treats redirected native stderr as ErrorRecords. The
# updater deliberately writes progress there; Stop can kill an in-flight upload.
# Keep its stdout JSON/stderr diagnostics separate and trust the real exit code.
$taskErrorPreference=$ErrorActionPreference
$taskNode=(Get-Command node -CommandType Application -ErrorAction Stop).Source
$taskNativeExit=$null
try {
    $ErrorActionPreference='Continue'
    & $taskNode @taskArguments
    $taskNativeExit=$LASTEXITCODE
} finally { $ErrorActionPreference=$taskErrorPreference }
if($null -eq $taskNativeExit -or $taskNativeExit -ne 0){throw 'Update not verified; inspect the reported stage. No automatic firmware write retry was made.'}
