[CmdletBinding()]
param(
    [string]$Image=(Join-Path $PSScriptRoot '../build/esp32s3-buffered/esp32s3.ino.bin'),
    [string]$Origin='http://ledboard.local',
    [ValidateSet('auto','local','remote')][string]$Mode='auto',
    [string]$Manifest,
    [string]$ManifestSha,
    [ValidateRange(1,4294967295)][long]$ResumeJobId,
    [ValidateRange(1,4294967295)][long]$AckFailedJobId,
    [string]$AckFailedImage,
    [string]$Backup,
    [switch]$ResumeBackup,
    [ValidateRange(10,600)][int]$TimeoutSeconds=180,
    [switch]$Run
)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern remote-ota -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern api-contract -Gate | Out-Null
if($PSBoundParameters.ContainsKey('Backup') -or $ResumeBackup){
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern pu-client-download -Gate | Out-Null
    & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern pu-backup-update -Gate | Out-Null
}
$taskArguments=@((Join-Path $PSScriptRoot 'update-firmware.mjs'),'--image',$Image,'--origin',$Origin,'--mode',$Mode,'--timeout',"$TimeoutSeconds")
if($Manifest){$taskArguments+=@('--manifest',$Manifest)}
if($ManifestSha){$taskArguments+=@('--manifest-sha',$ManifestSha)}
if($PSBoundParameters.ContainsKey('ResumeJobId')){$taskArguments+=@('--resume-job',"$ResumeJobId")}
if($PSBoundParameters.ContainsKey('AckFailedJobId')){$taskArguments+=@('--ack-failed-job',"$AckFailedJobId")}
if($PSBoundParameters.ContainsKey('AckFailedImage')){$taskArguments+=@('--ack-failed-image',$AckFailedImage)}
if($PSBoundParameters.ContainsKey('Backup')){$taskArguments+=@('--backup',$Backup)}
if($ResumeBackup){$taskArguments+='--resume-backup'}
if($Run){$taskArguments+='--run'}
# Windows PowerShell treats redirected native stderr as ErrorRecords. The
# updater deliberately writes progress there; Stop can kill an in-flight upload.
# Keep its stdout JSON/stderr diagnostics separate and trust the real exit code.
$taskErrorPreference=$ErrorActionPreference
# Resolve one PATH-preferred executable, not an array joined into a bogus path
# when both a system Node and the bundled Codex runtime are available.
$taskNode=@(Get-Command node -CommandType Application -ErrorAction Stop)[0].Source
$taskNativeExit=$null
try {
    $ErrorActionPreference='Continue'
    & $taskNode @taskArguments
    $taskNativeExit=$LASTEXITCODE
} finally { $ErrorActionPreference=$taskErrorPreference }
if($null -eq $taskNativeExit -or $taskNativeExit -ne 0){throw 'Update not verified; inspect the reported stage. No automatic firmware write retry was made.'}
