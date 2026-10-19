[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Output,[string]$Origin='http://ledboard.local',
    [ValidateRange(10,600)][int]$TimeoutSeconds=180,[switch]$Resume,[switch]$Run)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern pu-client-download -Gate -Summary | Out-Null
$taskArguments=@((Join-Path $PSScriptRoot 'download-firmware.mjs'),'--output',$Output,'--origin',$Origin,'--timeout',"$TimeoutSeconds")
if($Resume){$taskArguments+='--resume'}
if($Run){$taskArguments+='--run'}
& node @taskArguments
if($LASTEXITCODE){throw 'Firmware backup not verified; partial file retained, no device write or retry.'}
