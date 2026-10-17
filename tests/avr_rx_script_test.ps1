$ErrorActionPreference='Stop'
$taskSource=Join-Path $PSScriptRoot '../tools/capture-avr-rx.ps1'
$taskTokens=$null;$taskErrors=$null
$taskAst=[Management.Automation.Language.Parser]::ParseFile($taskSource,[ref]$taskTokens,[ref]$taskErrors)
if($taskErrors.Count){throw 'Passive RX capture script syntax invalid'}
. (Join-Path $PSScriptRoot '../tools/serial-wire.ps1')
# Pure function extraction never binds entrypoint parameters or opens a port.
foreach($taskFunction in $taskAst.FindAll({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)){
    . ([ScriptBlock]::Create($taskFunction.Extent.Text))
}
function Assert([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Must-Fail([scriptblock]$Action,[string]$Reason){
    $failed=$false;try{& $Action}catch{$failed=$true}
    Assert $failed $Reason
}
class AvrRxFixturePort {
    [Collections.Generic.Queue[int]]$Input=[Collections.Generic.Queue[int]]::new()
    [Collections.Generic.List[byte[]]]$Writes=[Collections.Generic.List[byte[]]]::new()
    [int]ReadByte(){if(-not $this.Input.Count){throw [TimeoutException]::new()};return $this.Input.Dequeue()}
    [void]Write([byte[]]$Bytes,[int]$Offset,[int]$Length){
        [byte[]]$copy=[byte[]]::new($Length);[Array]::Copy($Bytes,$Offset,$copy,0,$Length)
        $this.Writes.Add($copy)
    }
}
function Wire-Reply([byte]$Op,[byte]$Seq,[byte[]]$Payload){
    [byte[]]$raw=@(0xa5,0,$Op,$Seq,$Payload.Length)+$Payload
    $raw+=Get-PakarNetworkCrc8 $raw
    return ,(ConvertTo-PakarNetworkCobs $raw)
}
function Queue-Reply($Port,[byte]$Op,[byte]$Seq,[byte[]]$Payload){
    foreach($value in (Wire-Reply $Op $Seq $Payload)){$Port.Input.Enqueue($value)}
}
function Snapshot-Payload([byte]$Action,[bool]$Active,[int]$Stored=0,[byte]$BaudCode=0){
    [byte[]]$p=[byte[]]::new(27)
    $p[0]=$Action;$p[2]=[byte]$Active;$p[3]=$BaudCode;$p[4]=1
    $p[6]=if($Action -eq 2){1}else{0};$p[7]=$Stored;$p[8]=$Stored
    $p[24]=1;$p[25]=1;$p[26]=0
    return ,$p
}
function Make-Fixture([int]$Stored=5,[byte]$BaudCode=0){
    $port=[AvrRxFixturePort]::new()
    Queue-Reply $port 0x81 1 ([byte[]]@(0,2,0,0x20,80,9,0,2,7))
    [byte[]]$status=[byte[]]::new(42);$status[41]=0
    Queue-Reply $port 0x90 2 $status
    Queue-Reply $port 0xb4 3 (Snapshot-Payload 0 $false 0 255)
    Queue-Reply $port 0xb4 4 (Snapshot-Payload 1 $true 0 $BaudCode)
    Queue-Reply $port 0xb4 5 (Snapshot-Payload 0 $true $Stored $BaudCode)
    Queue-Reply $port 0xb4 6 (Snapshot-Payload 2 $false $Stored $BaudCode)
    return $port
}
function Decode-Writes($Port){
    foreach($wire in $Port.Writes){
        [byte[]]$raw=ConvertFrom-PakarNetworkCobs $wire[0..($wire.Length-2)]
        [pscustomobject]@{Opcode=[int]$raw[2];Action=$(if($raw[4]){[int]$raw[5]}else{-1})}
    }
}
$port=Make-Fixture
Queue-Reply $port 0xb4 7 ([byte[]]@(3,0,0,5,1,2,0,255,7))
$capture=Invoke-PakarAvrRxCapture $port 38400 100 100 {param($ms)}
Assert ($capture.Hex -eq '010200FF07' -and $capture.Base64 -eq 'AQIA/wc=') 'Raw capture bytes changed'
Assert ($capture.Capture.Stored -eq 5 -and -not $capture.Capture.Active -and -not $capture.NormalAvrLinkResumed) 'Stop/capture state was manufactured'
$writes=@(Decode-Writes $port)
Assert (@($writes | Where-Object {$_.Opcode -eq 0x5b -and $_.Action -eq 1}).Count -eq 1) 'Capture start repeated'
Assert (@($writes | Where-Object {$_.Opcode -eq 0x5b -and $_.Action -eq 2}).Count -eq 1) 'Capture stop repeated'
Assert (@($writes | Where-Object {$_.Opcode -notin @(1,2,0x5b)}).Count -eq 0) 'An AVR control/tone/frame opcode was sent'
$port=Make-Fixture 0
$capture=Invoke-PakarAvrRxCapture $port 38400 100 100 {param($ms)}
Assert ($capture.Hex -eq '' -and $capture.Base64 -eq '' -and $capture.Capture.Received -eq 0 -and $capture.Meaning -match 'does not prove') 'Zero-byte window claimed wiring evidence'
Assert ($port.Writes.Count -eq 6) 'Zero capture requested invalid/extra chunks'
$port=Make-Fixture 0 1
$capture=Invoke-PakarAvrRxCapture $port 115200 100 100 {param($ms)}
Assert ($capture.Baud -eq 115200 -and $capture.Capture.Baud -eq 115200) 'Explicit alternate baud was not preserved'
$port=Make-Fixture 128
for($offset=0;$offset -lt 128;$offset+=32){
    [byte[]]$p=@(3,0,[byte]$offset,32)+[byte[]]($offset..($offset+31))
    Queue-Reply $port 0xb4 ([byte](7+$offset/32)) $p
}
$capture=Invoke-PakarAvrRxCapture $port 38400 100 100 {param($ms)}
Assert ($capture.Capture.Stored -eq 128 -and [Convert]::FromBase64String($capture.Base64).Length -eq 128) 'Maximum bounded capture failed'
$goldenHex='000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F404142434445464748494A4B4C4D4E4F505152535455565758595A5B5C5D5E5F606162636465666768696A6B6C6D6E6F707172737475767778797A7B7C7D7E7F'
Assert ($capture.Hex -ceq $goldenHex) '128-byte upper-case hex golden vector failed'
Assert ($port.Writes.Count -eq 10) '128-byte capture did not use exactly four bounded chunks'
# Active normal UART must fail before any capture opcode.
$port=[AvrRxFixturePort]::new()
Queue-Reply $port 0x81 1 ([byte[]]@(0,2,0,0x20,80,9,0,2,7))
[byte[]]$status=[byte[]]::new(42);$status[41]=1;Queue-Reply $port 0x90 2 $status
Must-Fail {Invoke-PakarAvrRxCapture $port 38400 100 10 {param($ms)}} 'Ordinary AVR link was not required paused'
Assert ($port.Writes.Count -eq 2) 'Capture mutation occurred before pause ACK'
# Unknown/old status is not a paused acknowledgement either.
$status=[byte[]]::new(47);$status[2]=0x90;$status[4]=41
Must-Fail {Assert-PakarAvrRxPaused $status} 'Old firmware without pause field was accepted'
# Uncertain START triggers ONE cleanup STOP, never another START or STOP retry.
$port=[AvrRxFixturePort]::new()
Queue-Reply $port 0x81 1 ([byte[]]@(0,2,0,0x20,80,9,0,2,7))
[byte[]]$status=[byte[]]::new(42);Queue-Reply $port 0x90 2 $status
Queue-Reply $port 0xb4 3 (Snapshot-Payload 0 $false 0 255)
Must-Fail {Invoke-PakarAvrRxCapture $port 38400 100 10 {param($ms)} 3>$null} 'Missing start acknowledgement accepted'
$writes=@(Decode-Writes $port)
Assert (@($writes | Where-Object {$_.Opcode -eq 0x5b -and $_.Action -eq 1}).Count -eq 1) 'Uncertain start retried'
Assert (@($writes | Where-Object {$_.Opcode -eq 0x5b -and $_.Action -eq 2}).Count -eq 1) 'Cleanup stop missing or retried'
# Losing an explicit stop ACK must NOT cause the finally block to repeat it.
$port=[AvrRxFixturePort]::new()
Queue-Reply $port 0x81 1 ([byte[]]@(0,2,0,0x20,80,9,0,2,7))
[byte[]]$status=[byte[]]::new(42);Queue-Reply $port 0x90 2 $status
Queue-Reply $port 0xb4 3 (Snapshot-Payload 0 $false 0 255)
Queue-Reply $port 0xb4 4 (Snapshot-Payload 1 $true)
Queue-Reply $port 0xb4 5 (Snapshot-Payload 0 $true)
Must-Fail {Invoke-PakarAvrRxCapture $port 38400 100 10 {param($ms)}} 'Lost stop acknowledgement accepted'
$writes=@(Decode-Writes $port)
Assert (@($writes | Where-Object {$_.Opcode -eq 0x5b -and $_.Action -eq 2}).Count -eq 1) 'Uncertain explicit stop was retried'
foreach($oe in @(1,255)){
    $port=[AvrRxFixturePort]::new()
    Queue-Reply $port 0x81 1 ([byte[]]@(0,2,0,0x20,80,9,0,2,7))
    [byte[]]$status=[byte[]]::new(42);Queue-Reply $port 0x90 2 $status
    [byte[]]$unsafe=Snapshot-Payload 0 $false 0 255;$unsafe[26]=$oe
    Queue-Reply $port 0xb4 3 $unsafe
    Must-Fail {Invoke-PakarAvrRxCapture $port 38400 100 10 {param($ms)}} 'Unsafe/unknown RESET output ownership allowed capture'
    Assert ($port.Writes.Count -eq 3) 'Capture started before RESET ownership acknowledgement'
}
# Snapshot length, action, error, counter and pad bounds stay strict.
[byte[]]$snapshot=@(0xa5,0,0xb4,1,27)+(Snapshot-Payload 0 $false 0 255)+@(0)
$null=Read-PakarAvrRxSnapshot $snapshot 0
$snapshot[5]=1;Must-Fail {Read-PakarAvrRxSnapshot $snapshot 0} 'Mismatched response action accepted'
$snapshot[5]=0;$snapshot[6]=6;Must-Fail {Read-PakarAvrRxSnapshot $snapshot 0} 'ESP error treated as success'
$snapshot[6]=0;$snapshot[12]=129;Must-Fail {Read-PakarAvrRxSnapshot $snapshot 0} 'Oversized capture accepted'
$snapshot[12]=1;Must-Fail {Read-PakarAvrRxSnapshot $snapshot 0} 'Inconsistent byte count accepted'
[byte[]]$chunk=@(0xa5,0,0xb4,1,6,3,0,0,2,12,13,0)
Must-Fail {Read-PakarAvrRxChunk $chunk 1 2} 'Wrong chunk offset accepted'
Must-Fail {Read-PakarAvrRxChunk $chunk 0 3} 'Short chunk accepted as full readback'
$source=Get-Content -LiteralPath $taskSource -Raw
Assert ($source.Contains('. (Join-Path $PSScriptRoot ''serial-wire.ps1'')')) 'Shared actual wire module not imported'
Assert ($source -notmatch 'function (Get-PakarNetworkCrc8|ConvertTo-PakarNetworkCobs|ConvertFrom-PakarNetworkCobs|Receive-PakarNetworkReply|Send-PakarNetworkRequest)') 'Wire parser duplicated into capture helper'
Assert ($source -match 'DtrEnable=\$false;\$serial.RtsEnable=\$false') 'Port-open reset-line safety missing'
Assert ($source -notmatch 'Write-Host|PBX|AT\+|avrdude|esptool') 'Unexpected device mutation/helper output path'
$runtimeSources=@($source,(Get-Content (Join-Path $PSScriptRoot '../tools/serial-wire.ps1') -Raw),
    (Get-Content (Join-Path $PSScriptRoot '../tools/set-device-network.ps1') -Raw)) -join "`n"
Assert ($runtimeSources -notmatch '\[Convert\]::(?:ToHexString|FromHexString)|\[Array\]::Fill') 'Runtime helpers depend on modern .NET APIs missing from Windows PowerShell5/.NET Framework'
Write-Output 'Passive AVR RX host contracts pass: paused/identity/pad gates, strict snapshots/chunks, exact data, bounded128 bytes, no mutation replay and zero-byte uncertainty; no COM port or board was used.'
