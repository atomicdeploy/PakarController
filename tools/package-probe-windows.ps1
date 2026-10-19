function Assert-PakarWindowsGuiExecutable([string]$Executable) {
    # Check the artifact, not merely the Rust attribute or build logs. Both
    # PE32 and PE32+ put Subsystem at offset 68 in the optional header.
    $stream = [IO.File]::OpenRead($Executable)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($stream.Length -lt 128 -or $reader.ReadUInt16() -ne 0x5a4d) { throw 'Invalid Windows executable.' }
        $stream.Position = 0x3c
        $pe = $reader.ReadUInt32()
        if ($pe -lt 64 -or $pe -gt $stream.Length - 94) { throw 'Invalid PE header range.' }
        $stream.Position = $pe
        if ($reader.ReadUInt32() -ne 0x4550) { throw 'Invalid PE signature.' }
        $stream.Position = $pe + 20
        $optionalSize = $reader.ReadUInt16()
        if ($optionalSize -lt 70 -or $pe + 24 + $optionalSize -gt $stream.Length) { throw 'Invalid PE optional header.' }
        $stream.Position = $pe + 24
        if ($reader.ReadUInt16() -notin @(0x10b, 0x20b)) { throw 'Unsupported PE optional header.' }
        $stream.Position = $pe + 24 + 68
        if ($reader.ReadUInt16() -ne 2) { throw 'PakarController must use the Windows GUI subsystem, not a console.' }
    } finally { $reader.Dispose(); $stream.Dispose() }
    $version = (Get-Item -LiteralPath $Executable).VersionInfo
    foreach ($property in @('FileDescription', 'ProductName', 'CompanyName', 'InternalName')) {
        if ($version.$property -cne 'PakarController') { throw "Unexpected native application identity: $property" }
    }
}

function Invoke-PackageProbe([string]$Executable, [string]$Arguments, [string]$WorkingDirectory) {
    # Retain the handle: Start-Process can lose a short-lived GUI exe's exit code.
    $probe = [System.Diagnostics.Process]::new()
    try {
        $probe.StartInfo.FileName = $Executable
        $probe.StartInfo.Arguments = $Arguments
        $probe.StartInfo.WorkingDirectory = $WorkingDirectory
        $probe.StartInfo.UseShellExecute = $false
        $probe.StartInfo.CreateNoWindow = $true
        $probe.StartInfo.RedirectStandardOutput = $true
        [void]$probe.Start()
        $probeOutput = $probe.StandardOutput.ReadToEndAsync()
        if (-not $probe.WaitForExit(30000)) { throw "Package probe timed out: $Arguments" }
        if ($probe.ExitCode -ne 0) { throw "Package probe failed ($($probe.ExitCode)): $Arguments" }
        return $probeOutput.GetAwaiter().GetResult()
    } finally { $probe.Dispose() }
}
