[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_.:-]+$')][string]$Serial,
    [string]$AdbPath=(Join-Path $env:LOCALAPPDATA 'Android/sdk/platform-tools/adb.exe'),
    [switch]$Wake,
    [switch]$DismissSwipeLock,
    [switch]$Launch,
    [string]$Screenshot
)
$ErrorActionPreference='Stop'
# Reuse the actual imported Pealayer process-handle/stdout/exit/timeout helper.
# No install, settings change, credential entry or global ADB-server restart.
. (Join-Path $PSScriptRoot 'package-probe-windows.ps1')
$taskAdb=(Resolve-Path -LiteralPath $AdbPath).Path
function Invoke-DeviceAdb([string]$Arguments) {
    Invoke-PackageProbe $taskAdb "-s $Serial $Arguments" $PSScriptRoot
}
function Get-DeviceLock {
    $taskPolicy=Invoke-DeviceAdb 'shell dumpsys window policy'
    $taskSecure=[regex]::Matches($taskPolicy,'(?m)^\s*secure=(true|false)\s*$')
    $taskShowing=[regex]::Matches($taskPolicy,'(?m)^\s*mIsShowing=(true|false)\s*$')
    if($taskSecure.Count -ne 1 -or $taskShowing.Count -ne 1) {
        throw 'Cannot establish authoritative keyguard state; unlock the device manually.'
    }
    [pscustomobject]@{Secure=$taskSecure[0].Groups[1].Value -eq 'true';
        Showing=$taskShowing[0].Groups[1].Value -eq 'true'}
}
$taskDevices=Invoke-PackageProbe $taskAdb 'devices -l' $PSScriptRoot
if($taskDevices -notmatch ('(?m)^'+[regex]::Escape($Serial)+'\s+device(?:\s|$)')) {
    throw 'The explicit ADB target is not connected and authorized; no input sent.'
}
$taskSizeText=Invoke-DeviceAdb 'shell wm size'
$taskSizes=[regex]::Matches($taskSizeText,'(?m)^(Physical|Override) size:\s*(\d+)x(\d+)\s*$')
if(-not $taskSizes.Count){throw 'Cannot read screen dimensions; no input sent.'}
$taskSize=@($taskSizes | Where-Object {$_.Groups[1].Value -eq 'Override'})
if(-not $taskSize.Count){$taskSize=@($taskSizes | Where-Object {$_.Groups[1].Value -eq 'Physical'})}
if($taskSize.Count -ne 1){throw 'Ambiguous screen dimensions; no input sent.'}
$taskWidth=[int]$taskSize[0].Groups[2].Value
$taskHeight=[int]$taskSize[0].Groups[3].Value
if($taskWidth -lt 320 -or $taskHeight -lt 320 -or $taskWidth -gt 8192 -or $taskHeight -gt 8192) {
    throw 'Unsupported screen dimensions; no input sent.'
}
if($Wake -or $DismissSwipeLock) {
    Invoke-DeviceAdb 'shell input keyevent KEYCODE_WAKEUP' | Out-Null
    Start-Sleep -Milliseconds 250
}
$taskLock=Get-DeviceLock
$taskSwiped=$false
if($DismissSwipeLock -and $taskLock.Showing) {
    if($taskLock.Secure){throw 'PIN/password/pattern-protected lock: manual unlock required; no swipe sent.'}
    $taskX=[int]($taskWidth*0.5)
    $taskFrom=[int]($taskHeight*0.84)
    $taskTo=[int]($taskHeight*0.38)
    Invoke-DeviceAdb "shell input swipe $taskX $taskFrom $taskX $taskTo 450" | Out-Null
    $taskSwiped=$true
    $taskDeadline=[DateTime]::UtcNow.AddSeconds(5)
    do {
        Start-Sleep -Milliseconds 250
        $taskLock=Get-DeviceLock
    } while($taskLock.Showing -and [DateTime]::UtcNow -lt $taskDeadline)
    if($taskLock.Showing){throw 'One authorized swipe did not dismiss the lock; unlock manually. No retry loop.'}
}
if(($Launch -or $Screenshot) -and $taskLock.Showing){throw 'Unlock the device or explicitly request -DismissSwipeLock for an unsecured swipe lock.'}
if($Launch) {
    $taskLaunch=Invoke-DeviceAdb 'shell am start -W -n ir.digitalogic.pakar.pakarcontroller/.MainActivity'
    if($taskLaunch -notmatch '(?m)^Status:\s*ok\s*$'){throw "App launch did not report success: $taskLaunch"}
}
if($Screenshot) {
    if(-not [IO.Path]::IsPathRooted($Screenshot) -or [IO.Path]::GetExtension($Screenshot) -ne '.png') {
        throw 'Screenshot must be an absolute PNG path.'
    }
    if(Test-Path -LiteralPath $Screenshot){throw 'Screenshot exists; evidence will not be overwritten.'}
    $taskCapture=[Diagnostics.Process]::new()
    try {
        $taskCapture.StartInfo.FileName=$taskAdb
        $taskCapture.StartInfo.Arguments="-s $Serial exec-out screencap -p"
        $taskCapture.StartInfo.UseShellExecute=$false
        $taskCapture.StartInfo.CreateNoWindow=$true
        $taskCapture.StartInfo.RedirectStandardOutput=$true
        $taskCapture.StartInfo.RedirectStandardError=$true
        $taskStream=[IO.File]::Open($Screenshot,[IO.FileMode]::CreateNew)
        try {
            [void]$taskCapture.Start()
            $taskError=$taskCapture.StandardError.ReadToEndAsync()
            $taskCopy=$taskCapture.StandardOutput.BaseStream.CopyToAsync($taskStream)
            if(-not $taskCapture.WaitForExit(15000)) {
                $taskCapture.Kill() # This script's local screencap client only, never the app/device/server.
                $taskCapture.WaitForExit()
                throw 'ADB capture timed out; partial evidence is retained, not silently overwritten.'
            }
            [void]$taskCopy.GetAwaiter().GetResult()
            if($taskCapture.ExitCode -ne 0){throw $taskError.GetAwaiter().GetResult()}
        } finally {$taskStream.Dispose()}
    } finally {$taskCapture.Dispose()}
}
$taskFocus=Invoke-DeviceAdb 'shell dumpsys window'
[pscustomobject]@{Serial=$Serial;Width=$taskWidth;Height=$taskHeight;SecureLock=$taskLock.Secure;
    Locked=$taskLock.Showing;Swiped=$taskSwiped;Launched=[bool]$Launch;Screenshot=$Screenshot;
    Focus=(@($taskFocus -split "`n" | Where-Object {$_ -match 'mCurrentFocus='}) -join '').Trim()}
