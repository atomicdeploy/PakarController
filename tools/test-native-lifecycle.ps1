# Live product-CLI acceptance, not desktop input or a synthetic GUI fixture.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateRange(1,2147483647)][int]$ExpectedProcessId,
    [Parameter(Mandatory=$true)][string]$CapturePath,
    [Parameter(Mandatory=$true)][string]$HiddenCapturePath
)
$ErrorActionPreference='Stop'
$taskReuse=& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern capture -Gate
$taskNative=& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern native-lifecycle
$taskOwner=Get-CimInstance Win32_Process -Filter "ProcessId=$ExpectedProcessId"
if(-not $taskOwner.ExecutablePath){throw 'Expected app owner is unavailable.'}
$taskExe=$taskOwner.ExecutablePath
$taskExeHash=(Get-FileHash -LiteralPath $taskExe).Hash
if(-not [IO.Path]::IsPathRooted($CapturePath) -or -not [IO.Path]::IsPathRooted($HiddenCapturePath)){throw 'Explicit absolute capture paths required.'}
$taskCapture=[IO.Path]::GetFullPath($CapturePath)
$taskHidden=[IO.Path]::GetFullPath($HiddenCapturePath)
if($taskCapture -eq $taskHidden){throw 'Visible and hidden capture paths must differ.'}
foreach($taskPath in @($taskCapture,$taskHidden)){
    if(-not [IO.Path]::IsPathRooted($taskPath) -or [IO.Path]::GetExtension($taskPath) -ne '.png' -or (Test-Path -LiteralPath $taskPath)){throw 'Capture must be an unused absolute PNG path.'}
}
function Read-OwnerStatus {
    $taskValue=& $taskExe --app-status | ConvertFrom-Json
    if($LASTEXITCODE -ne 0 -or $taskValue.process_id -ne $ExpectedProcessId){throw 'IPC owner missing or changed.'}
    if((Get-FileHash -LiteralPath $taskExe).Hash -ne $taskExeHash){throw 'Owner executable changed during acceptance.'}
    return $taskValue
}
function Wait-OwnerState([scriptblock]$Predicate,[int]$Seconds=6) {
    $taskDeadline=[DateTime]::UtcNow.AddSeconds($Seconds)
    do {
        $taskValue=Read-OwnerStatus
        if(& $Predicate $taskValue){return $taskValue}
        Start-Sleep -Milliseconds 100
    } while([DateTime]::UtcNow -lt $taskDeadline)
    throw 'Expected application state did not arrive before deadline.'
}
$taskInitial=Read-OwnerStatus
if(-not $taskInitial.ready -or -not $taskInitial.single_instance -or $taskInitial.preferences_open -or $taskInitial.preferences_dirty -or $taskInitial.device_settings.open -or $taskInitial.minimized -or $taskInitial.screenshot.pending){throw 'Live smoke requires a ready visible single-instance owner with no open draft or pending capture.'}
try {
    & $taskExe --minimize | Out-Null
    if($LASTEXITCODE){throw 'Minimize command rejected.'}
    $taskMinimized=Wait-OwnerState {param($s) $s.minimized -eq $true}
    $taskOldId=$taskMinimized.screenshot.request_id
    & $taskExe --screenshot $taskHidden | Out-Null
    if($LASTEXITCODE){throw 'Hidden capture request rejected.'}
    $taskPending=Wait-OwnerState {param($s) $s.screenshot.pending -and $s.screenshot.request_id -ne $taskOldId -and $s.screenshot.path -eq $taskHidden}
    $taskTimeout=Wait-OwnerState {param($s) -not $s.screenshot.pending -and $s.screenshot.request_id -eq $taskPending.screenshot.request_id} 12
    if(-not $taskTimeout.minimized -or $taskTimeout.preferences_open -or $taskTimeout.preferences_dirty -or (Test-Path -LiteralPath $taskHidden) -or $taskTimeout.screenshot.result -notlike '*timed out*'){throw 'Hidden capture revealed/mutated the app or claimed pixels it did not render.'}
    # Default single-instance launch is the actual Show command, not SendInput.
    & $taskExe | Out-Null
    if($LASTEXITCODE){throw 'Show command rejected.'}
    $taskShown=Wait-OwnerState {param($s) $s.minimized -eq $false}
    $taskPng=& (Join-Path $PSScriptRoot 'capture-window-windows.ps1') -ExpectedProcessId $ExpectedProcessId -OutputPath $taskCapture -Backend Internal
    $taskFinal=Read-OwnerStatus
    if($taskFinal.preferences_open -or $taskFinal.preferences_dirty -or -not (Test-Path -LiteralPath $taskCapture)){throw 'Visible capture missing or changed a draft.'}
    [pscustomobject]@{ProcessId=$ExpectedProcessId;ExecutableSHA256=$taskExeHash;MinimizeObserved=$true;HiddenRequestApplied=$true;HiddenDeadlineWithoutPaint=$true;HiddenDidNotReveal=$true;ShowAppliedWithoutPaint=$true;Capture=$taskPng}
} finally {
    # Never leave the user's app minimized after a failed diagnostic; only send
    # Show to the same surviving owner, and never launch a replacement here.
    $taskSurvivor=Get-CimInstance Win32_Process -Filter "ProcessId=$ExpectedProcessId"
    if($taskSurvivor -and $taskSurvivor.ExecutablePath -eq $taskExe -and (Get-FileHash -LiteralPath $taskExe).Hash -eq $taskExeHash){& $taskExe | Out-Null}
}
