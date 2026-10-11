param(
    [Parameter(Mandatory)] [string] $Port,
    [Parameter(Mandatory)] [string] $Name
)
# Physical ESP USB naming; no Wi-Fi, authentication or AVR storage is erased.
$ErrorActionPreference = 'Stop'
$label = $Name.ToLowerInvariant()
if ($label.EndsWith('.local')) { $label = $label.Substring(0, $label.Length - 6) }
if ($label -notmatch '^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$' -or $label -in @('local','localhost')) {
    throw 'Use 1..63 letters/digits/hyphens, no leading/trailing hyphen; optional .local suffix'
}
function Get-Crc8([byte[]] $Bytes) {
    [int] $crc = 0
    foreach ($value in $Bytes) {
        $crc = $crc -bxor $value
        for ($bit = 0; $bit -lt 8; $bit++) {
            $crc = if ($crc -band 128) { (($crc -shl 1) -bxor 7) -band 255 } else { ($crc -shl 1) -band 255 }
        }
    }
    return [byte] $crc
}
function Encode-Cobs([byte[]] $Bytes) {
    $encoded = [Collections.Generic.List[byte]]::new(); $encoded.Add(0)
    $position = 0; $code = 1
    foreach ($value in $Bytes) {
        if ($value -eq 0) { $encoded[$position] = $code; $position = $encoded.Count; $encoded.Add(0); $code = 1 }
        else { $encoded.Add($value); $code++ }
    }
    $encoded[$position] = $code; $encoded.Add(0)
    return ,$encoded.ToArray()
}
function Decode-Cobs([byte[]] $Bytes) {
    $decoded = [Collections.Generic.List[byte]]::new(); $at = 0
    while ($at -lt $Bytes.Length) {
        $code = [int] $Bytes[$at++]; if ($code -eq 0 -or $at + $code - 1 -gt $Bytes.Length) { return $null }
        for ($i = 1; $i -lt $code; $i++) { $decoded.Add($Bytes[$at++]) }
        if ($code -ne 255 -and $at -lt $Bytes.Length) { $decoded.Add(0) }
    }
    return ,$decoded.ToArray()
}
$serial = [IO.Ports.SerialPort]::new($Port, 115200, 'None', 8, 'One')
$serial.DtrEnable = $false; $serial.RtsEnable = $false; $serial.ReadTimeout = 100
$script:nameSequence = 0
function Send-NameChunk([byte[]] $Payload) {
    $script:nameSequence++; $sequence = [byte] $script:nameSequence
    [byte[]] $raw = @(0xa5, 1, 0x55, $sequence, $Payload.Length) + $Payload
    $raw += Get-Crc8 $raw
    [byte[]] $wire = Encode-Cobs $raw
    $serial.Write($wire, 0, $wire.Length)
    $received = [Collections.Generic.List[byte]]::new(); $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.ElapsedMilliseconds -lt 5000) {
        try { $value = $serial.ReadByte() } catch [TimeoutException] { continue }
        if ($value -ne 0) { if ($received.Count -lt 60) { $received.Add([byte] $value) }; continue }
        [byte[]] $reply = Decode-Cobs $received.ToArray(); $received.Clear()
        if (-not $reply -or $reply.Length -lt 6 -or $reply[0] -ne 0xa5 -or $reply[1] -ne 1 -or $reply[3] -ne $sequence) { continue }
        if ($reply.Length -ne $reply[4] + 6 -or (Get-Crc8 $reply[0..($reply.Length - 2)]) -ne $reply[-1]) { continue }
        if ($reply[2] -eq 0x80 -and $reply[4] -ge 1 -and $reply[5] -eq 0x55) { return }
        if ($reply[2] -eq 0x82) { throw 'Device rejected device naming. Check firmware version, hostname rules and ongoing updates.' }
    }
    throw 'No matching USB acknowledgment. No success is assumed; stop other serial clients and check ESP USB/firmware.'
}
try {
    $serial.Open(); Start-Sleep -Milliseconds 250; $serial.DiscardInBuffer()
    Send-NameChunk ([byte[]] @(0))
    [byte[]] $bytes = [Text.Encoding]::ASCII.GetBytes($label)
    for ($offset = 0; $offset -lt $bytes.Length; $offset += 47) {
        $end = [Math]::Min($offset + 46, $bytes.Length - 1)
        Send-NameChunk ([byte[]] (@(1) + $bytes[$offset..$end]))
    }
    Send-NameChunk ([byte[]] @(2))
    Write-Host "Device confirmed the name is saved. Reconnect at http://$label.local/ after Wi-Fi reassociates."
} finally {
    if ($serial.IsOpen) { $serial.Close() }; $serial.Dispose()
}
