$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../tools/test-device-discovery.ps1') -LocalAddress '192.0.2.1' -NoProbe
function Assert($Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Reject([scriptblock]$Action){$rejected=$false;try{& $Action|Out-Null}catch{$rejected=$true};Assert $rejected 'Malformed packet was accepted'}
$query=New-PakarDnsQuery @(@{name='ledboard.local';type=1})
Assert ((Read-PakarU16 $query 4) -eq 1) 'Wrong DNS question count'
$at=12;Assert ((Read-PakarDnsName $query ([ref]$at)) -eq 'ledboard.local') 'DNS name encoding differs'
Assert ((Read-PakarU16 $query ($at+2)) -eq 32769) 'QU/class must request one unicast IN answer'
$packet=[Collections.Generic.List[byte]]::new();$packet.AddRange($query);$packet[2]=132;$packet[7]=1
$packet.AddRange([byte[]](192,12,0,1,0,1,0,0,0,120,0,4,192,0,2,7))
$answer=@(ConvertFrom-PakarDns $packet.ToArray())
Assert ($answer.Count -eq 1 -and $answer[0].name -eq 'ledboard.local' -and $answer[0].value -eq '192.0.2.7') 'Compressed A answer decoding differs'
$loop=[byte[]](192,0);Reject {$p=0;Read-PakarDnsName $loop ([ref]$p)}
Reject {ConvertFrom-PakarDns ([byte[]](0,0,132))}
Reject {ConvertFrom-PakarMndp ([byte[]](0,0,0,1,0,5,0,8,65))}
$mndp=ConvertFrom-PakarMndp ([byte[]](0,0,18,52,0,5,0,8,108,101,100,98,111,97,114,100,0,17,0,4,192,0,2,7,0,10,0,4,1,2,0,0))
Assert ($mndp['sequence'] -eq 0x1234 -and $mndp['5'] -eq 'ledboard' -and $mndp['17'] -eq '192.0.2.7' -and $mndp['10'] -eq 513) 'MNDP big-endian TLV/little-endian uptime differs'
$helper=Get-Content -LiteralPath (Join-Path $PSScriptRoot '../tools/test-device-discovery.ps1') -Raw
foreach($method in @('ConnectAsync','SendAsync')) {
  # Task<void>.GetResult can emit a VoidTaskResult object in Windows PowerShell;
  # both must be explicitly discarded, not become extra HTTP result-array items.
  Assert ($helper -match ('\$null\s*=\s*\$ws\.'+$method+'\(')) "$method must discard its non-data task result"
}
'Discovery helper parser fixtures passed; no sockets, DNS lookup, HTTP, WebSocket, UART, settings or audio operation executed.'
