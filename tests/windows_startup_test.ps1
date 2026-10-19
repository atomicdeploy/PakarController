[CmdletBinding()]
param([string]$Executable)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $taskRoot 'tools/package-probe-windows.ps1')
function Reject([byte[]]$Bytes,[string]$Reason) {
    $taskFixture=Join-Path $taskRoot 'build/native/windows-startup-fixture.exe'
    New-Item -ItemType Directory -Force -Path (Split-Path $taskFixture -Parent) | Out-Null
    [IO.File]::WriteAllBytes($taskFixture,$Bytes)
    try {Assert-PakarWindowsGuiExecutable $taskFixture} catch {
        if($_.Exception.Message -notlike $Reason){throw "Unexpected package rejection: $_"}
        return
    }
    throw "Missing package rejection: $Reason"
}
Reject ([byte[]]::new(64)) 'Invalid Windows executable*'
$taskBytes=[byte[]]::new(256)
[BitConverter]::GetBytes([uint16]0x5a4d).CopyTo($taskBytes,0)
[BitConverter]::GetBytes([uint32]64).CopyTo($taskBytes,0x3c)
[BitConverter]::GetBytes([uint32]0x4550).CopyTo($taskBytes,64)
[BitConverter]::GetBytes([uint16]70).CopyTo($taskBytes,84)
[BitConverter]::GetBytes([uint16]0x20b).CopyTo($taskBytes,88)
[BitConverter]::GetBytes([uint16]3).CopyTo($taskBytes,156)
Reject $taskBytes '*must use the Windows GUI subsystem*'
[BitConverter]::GetBytes([uint16]2).CopyTo($taskBytes,156)
Reject $taskBytes 'Unexpected native application identity*'
[BitConverter]::GetBytes([uint32]255).CopyTo($taskBytes,0x3c)
Reject $taskBytes 'Invalid PE header range*'
if($Executable) {
    $Executable=[IO.Path]::GetFullPath($Executable)
    Assert-PakarWindowsGuiExecutable $Executable
    foreach($taskOption in @('--help','--version')) {
        $taskOutput=Invoke-PackageProbe $Executable $taskOption (Split-Path $Executable -Parent)
        if($taskOutput -notmatch '^PakarController '){throw "Missing redirected CLI identity: $taskOption"}
    }
    $taskChild=[Diagnostics.Process]::new()
    try {
        $taskChild.StartInfo.FileName=$Executable
        $taskChild.StartInfo.Arguments='--apply-client-update'
        $taskChild.StartInfo.UseShellExecute=$false
        $taskChild.StartInfo.CreateNoWindow=$true
        $taskChild.StartInfo.RedirectStandardOutput=$true
        $taskChild.StartInfo.RedirectStandardError=$true
        [void]$taskChild.Start()
        $taskError=$taskChild.StandardError.ReadToEndAsync()
        if(-not $taskChild.WaitForExit(10000) -or $taskChild.ExitCode -ne 2 -or
            $taskError.GetAwaiter().GetResult() -notmatch 'Invalid managed app update arguments') {
            throw 'Detached helper must return its actual error through redirected stderr, without opening GUI'
        }
    }finally{$taskChild.Dispose()}
}
Write-Host 'Windows startup: malformed/console/wrong-brand rejection passed; supplied executable also verifies real PE/resources and redirected CLI/helper output. No board contacted.'
