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
& node @taskArguments
if($LASTEXITCODE){throw 'Update not verified; inspect the reported stage. No automatic firmware write retry was made.'}
