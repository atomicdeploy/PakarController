# Read-only Win32 capture adapted from Pealayer's capture-visible-window-windows.ps1
# and update-screenshots-windows.ps1. No activation, resize, mouse or keyboard input.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateRange(1,2147483647)][int]$ExpectedProcessId,
    [Parameter(Mandatory=$true)][string]$OutputPath,
    [ValidateSet('Auto','Win32','Internal')][string]$Backend='Auto'
)
$ErrorActionPreference='Stop'
$taskReuse = & (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern capture -Gate
if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {throw 'Windows capture requires Windows.'}
$taskOutput=[IO.Path]::GetFullPath($OutputPath)
if ([IO.Path]::GetExtension($taskOutput) -ne '.png') {throw 'Output must be a PNG.'}
if (Test-Path -LiteralPath $taskOutput) {throw 'Output exists; capture will not overwrite it.'}
$taskProcess=Get-CimInstance Win32_Process -Filter "ProcessId=$ExpectedProcessId"
if (-not $taskProcess -or -not $taskProcess.ExecutablePath) {throw 'Expected process does not exist or its identity is unavailable.'}
$taskExecutable=[IO.Path]::GetFullPath($taskProcess.ExecutablePath)
if ($Backend -ne 'Win32') {
    try {
        if ((Get-Item -LiteralPath $taskExecutable).VersionInfo.ProductName -ne 'PakarController') {throw 'This executable does not expose the Pakar capture contract.'}
        $taskStatus=(& $taskExecutable --app-status | ConvertFrom-Json)
        if ($LASTEXITCODE -ne 0 -or $taskStatus.process_id -ne $ExpectedProcessId) {throw 'IPC target is not the expected process.'}
        if ($taskStatus.screenshot.backend -ne 'egui framebuffer') {throw 'Owner does not advertise internal screenshot capability.'}
        if ($taskStatus.screenshot.pending) {throw 'Owner already has a screenshot pending.'}
        $taskBeforeRequestId=$taskStatus.screenshot.request_id
        $taskBeforeResult=$taskStatus.screenshot.result
        $taskObservedPending=$false
        & $taskExecutable --screenshot $taskOutput | Out-Null
        if ($LASTEXITCODE -ne 0) {throw 'Internal screenshot request was rejected.'}
        $taskDeadline=[DateTime]::UtcNow.AddSeconds(12)
        while ([DateTime]::UtcNow -lt $taskDeadline) {
            $taskStatus=(& $taskExecutable --app-status | ConvertFrom-Json)
            if ($taskStatus.process_id -ne $ExpectedProcessId) {throw 'IPC owner changed during capture.'}
            # Acceptance only queues work; the first snapshot can still be idle.
            # New owners correlate by request ID/path; older owners require an
            # observed pending edge or a changed completion result plus the file.
            $taskCorrelated=if ($null -ne $taskBeforeRequestId) {
                $taskStatus.screenshot.request_id -ne $taskBeforeRequestId -and $taskStatus.screenshot.path -eq $taskOutput
            } else {
                $taskObservedPending -or ($taskStatus.screenshot.result -ne $taskBeforeResult -and (Test-Path -LiteralPath $taskOutput))
            }
            if ($taskStatus.screenshot.pending) {$taskObservedPending=$true}
            if ($taskCorrelated -and $taskStatus.screenshot.pending -eq $false) {
                if (Test-Path -LiteralPath $taskOutput) {
                    [pscustomobject]@{ProcessId=$ExpectedProcessId;Method='egui framebuffer';Path=$taskOutput;Result=$taskStatus.screenshot.result}
                    return
                }
                throw $taskStatus.screenshot.result
            }
            Start-Sleep -Milliseconds 100
        }
        throw 'Internal framebuffer capture timed out.'
    } catch {
        if ($Backend -eq 'Internal' -or (Test-Path -LiteralPath $taskOutput)) {throw}
        Write-Warning ("Internal capture unavailable; trying read-only Win32: "+$_.Exception.Message)
    }
}
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class PakarReadOnlyCapture {
    public delegate bool EnumProc(IntPtr hwnd,IntPtr state);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X,Y; }
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback,IntPtr state);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd,out Rect rectangle);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(Point point);
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr value);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr value);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd,IntPtr dc,uint flags);
    [DllImport("user32.dll")] public static extern IntPtr GetWindowDC(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr hwnd,IntPtr dc);
    [DllImport("gdi32.dll")] public static extern bool BitBlt(IntPtr dest,int x,int y,int w,int h,IntPtr src,int sx,int sy,uint operation);
    public static IntPtr Find(uint expected) {
        IntPtr target=IntPtr.Zero; long area=0;
        EnumWindows((hwnd,state)=>{
            uint pid; Rect r; GetWindowThreadProcessId(hwnd,out pid);
            if(pid==expected && IsWindowVisible(hwnd) && !IsIconic(hwnd) && GetWindowRect(hwnd,out r)) {
                long next=(long)(r.Right-r.Left)*(r.Bottom-r.Top);
                if(r.Right-r.Left>=320 && r.Bottom-r.Top>=240 && next>area) {target=hwnd;area=next;}
            }
            return true;
        },IntPtr.Zero); return target;
    }
    public static bool Owns(IntPtr hwnd,uint expected) {uint pid;GetWindowThreadProcessId(hwnd,out pid);return pid==expected;}
    public static bool ScreenSafe(IntPtr hwnd,Rect r,uint expected) {
        if(GetForegroundWindow()!=hwnd) return false;
        int[] xs={r.Left+16,(r.Left+r.Right)/2,r.Right-16};
        int[] ys={r.Top+16,(r.Top+r.Bottom)/2,r.Bottom-16};
        foreach(int x in xs) foreach(int y in ys)
            if(!Owns(WindowFromPoint(new Point{X=x,Y=y}),expected)) return false;
        return true;
    }
}
'@
[void][PakarReadOnlyCapture]::SetProcessDpiAwarenessContext([IntPtr]::new(-4))
$taskOldDpi=[PakarReadOnlyCapture]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
function Test-Blank([Drawing.Bitmap]$Bitmap) {
    $taskMin=255;$taskMax=0
    for($taskY=0;$taskY -lt $Bitmap.Height;$taskY+=[Math]::Max(1,[Math]::Floor($Bitmap.Height/20))) {
        for($taskX=0;$taskX -lt $Bitmap.Width;$taskX+=[Math]::Max(1,[Math]::Floor($Bitmap.Width/32))) {
            $taskPixel=$Bitmap.GetPixel($taskX,$taskY)
            $taskMin=[Math]::Min($taskMin,[Math]::Min($taskPixel.R,[Math]::Min($taskPixel.G,$taskPixel.B)))
            $taskMax=[Math]::Max($taskMax,[Math]::Max($taskPixel.R,[Math]::Max($taskPixel.G,$taskPixel.B)))
        }
    }
    return $taskMax -lt 12 -or ($taskMax-$taskMin) -lt 3
}
try {
    $taskHandle=[PakarReadOnlyCapture]::Find([uint32]$ExpectedProcessId)
    if ($taskHandle -eq [IntPtr]::Zero) {throw 'No visible non-minimized window for expected PID.'}
    $taskRect=New-Object PakarReadOnlyCapture+Rect
    if (-not [PakarReadOnlyCapture]::GetWindowRect($taskHandle,[ref]$taskRect)) {throw 'Cannot read window bounds.'}
    $taskWidth=$taskRect.Right-$taskRect.Left;$taskHeight=$taskRect.Bottom-$taskRect.Top
    if($taskWidth -le 0 -or $taskHeight -le 0 -or $taskWidth -gt 8192 -or $taskHeight -gt 8192) {throw 'Invalid capture dimensions.'}
    $taskBitmap=New-Object Drawing.Bitmap $taskWidth,$taskHeight
    $taskGraphics=[Drawing.Graphics]::FromImage($taskBitmap)
    try {
        $taskDc=$taskGraphics.GetHdc()
        try {
            $taskPrinted=[PakarReadOnlyCapture]::PrintWindow($taskHandle,$taskDc,2)
            $taskMethod='user32.PrintWindow(PW_RENDERFULLCONTENT)'
        } finally {$taskGraphics.ReleaseHdc($taskDc)}
            if (-not $taskPrinted -or (Test-Blank $taskBitmap)) {
                $taskWindowDc=[PakarReadOnlyCapture]::GetWindowDC($taskHandle)
                if($taskWindowDc -ne [IntPtr]::Zero) {
                    $taskDc=$taskGraphics.GetHdc()
                    try {
                        [void][PakarReadOnlyCapture]::BitBlt($taskDc,0,0,$taskWidth,$taskHeight,$taskWindowDc,0,0,0x40CC0020)
                        $taskMethod='gdi32.BitBlt(window-dc)'
                    } finally {$taskGraphics.ReleaseHdc($taskDc);[void][PakarReadOnlyCapture]::ReleaseDC($taskHandle,$taskWindowDc)}
                }
            }
        if(Test-Blank $taskBitmap) {
            if(-not [PakarReadOnlyCapture]::ScreenSafe($taskHandle,$taskRect,[uint32]$ExpectedProcessId)) {
                throw 'GPU window rejected read-only capture and foreground screen ownership is not safe; use internal framebuffer.'
            }
            $taskGraphics.CopyFromScreen($taskRect.Left,$taskRect.Top,0,0,$taskBitmap.Size)
            $taskMethod='System.Drawing.Graphics.CopyFromScreen(verified-target)'
        }
        if(Test-Blank $taskBitmap) {throw 'Blank capture rejected.'}
        if(-not [PakarReadOnlyCapture]::Owns($taskHandle,[uint32]$ExpectedProcessId)) {throw 'Window owner changed; capture rejected.'}
        $taskStream=[IO.File]::Open($taskOutput,[IO.FileMode]::CreateNew)
        try {$taskBitmap.Save($taskStream,[Drawing.Imaging.ImageFormat]::Png)} finally {$taskStream.Dispose()}
        [pscustomobject]@{ProcessId=$ExpectedProcessId;Width=$taskWidth;Height=$taskHeight;Method=$taskMethod;Path=$taskOutput}
    } finally {$taskGraphics.Dispose();$taskBitmap.Dispose()}
} finally {
    if($taskOldDpi -ne [IntPtr]::Zero) {[void][PakarReadOnlyCapture]::SetThreadDpiAwarenessContext($taskOldDpi)}
}
