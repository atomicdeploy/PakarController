[CmdletBinding(SupportsShouldProcess,ConfirmImpact='High')]
param(
    [Parameter(Mandatory)][string]$Port,
    [Parameter(Mandatory)][string]$ExpectedBoardId
)
$ErrorActionPreference='Stop'
$invoke=Join-Path $PSScriptRoot 'invoke-usb-api.ps1'
function Invoke-InitializationApi([string]$Method,[string]$Path,$Body=@{}) {
    & $invoke -Port $Port -ExpectedBoardId $ExpectedBoardId -RequestJson (@{method=$Method;path=$Path;body=$Body}|ConvertTo-Json -Depth 16 -Compress)
}
$status=Invoke-InitializationApi GET /api/maintenance
if($status.filesystem_mounted){throw 'Filesystem already mounted; no formatting is permitted by this tool'}
if($status.filesystem_initialized_reboot_required){throw 'Initialization already completed; reboot explicitly rather than repeat formatting'}
if(-not $PSCmdlet.ShouldProcess($ExpectedBoardId,'Disable LED output, verify completely erased filesystem partition, initialize it and reboot')){return}
$prepared=Invoke-InitializationApi POST /api/filesystem/prepare @{disable_led_output=$true}
if(-not $prepared.token -or $prepared.confirmation -cne 'INITIALIZE EMPTY FILESYSTEM'){throw "Initialization refused: $($prepared.error). No format was requested."}
$result=Invoke-InitializationApi POST /api/filesystem/initialize @{token=$prepared.token;confirmation=$prepared.confirmation}
if($result.accepted -ne $true -or $result.reboot_required -ne $true){throw "Initialization failed: $($result.error). No retry was attempted."}
$reboot=Invoke-InitializationApi POST /api/filesystem/reboot @{confirmation='REBOOT INITIALIZED FILESYSTEM'}
if($reboot.accepted -ne $true){throw 'Filesystem initialized, but reboot was not accepted; do not repeat initialization'}
[pscustomobject]@{BoardId=$ExpectedBoardId;Initialized=$true;RebootRequested=$true;PostBootMountVerified=$false}
