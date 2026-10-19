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
$nb=New-PakarNbnsQuery 'ledboard'
Assert ($nb.Length -eq 50 -and (Read-PakarU16 $nb 46) -eq 32) 'NB name query framing differs'
Reject {New-PakarNbnsQuery 'sixteencharacters'}
$nbReply=[byte[]]::new(62);[Array]::Copy($nb,$nbReply,50);$nbReply[2]=133;$nbReply[3]=0;$nbReply[5]=0;$nbReply[7]=1
$nbReply[54]=0;$nbReply[55]=6;$nbReply[58]=192;$nbReply[59]=0;$nbReply[60]=2;$nbReply[61]=7
Assert ((ConvertFrom-PakarNbns $nbReply 19280 32).address -eq '192.0.2.7') 'Actual NB reply parser differs'
Reject {ConvertFrom-PakarNbns $nbReply 1 32}
Reject {ConvertFrom-PakarNbns $nbReply 19280 33}
$node=New-PakarNbnsQuery 'ledboard' $true;$status=[byte[]]::new(121);[Array]::Copy($node,$status,50)
$status[2]=133;$status[3]=0;$status[5]=0;$status[7]=1;$status[55]=65;$status[56]=1
[Array]::Copy([Text.Encoding]::ASCII.GetBytes('LEDBOARD       '),0,$status,57,15);$status[73]=4
[Array]::Copy([byte[]](2,3,4,5,6,7),0,$status,75,6)
$parsed=ConvertFrom-PakarNbns $status 19281 33
Assert ($parsed.names.Count -eq 1 -and $parsed.names[0].name -eq 'LEDBOARD' -and $parsed.names[0].suffix -eq 0 -and $parsed.mac -eq '02:03:04:05:06:07') 'Complete NBSTAT names/statistics differ'
$status[56]=2;Reject {ConvertFrom-PakarNbns $status 19281 33}
'Discovery helper parser fixtures passed; no sockets, DNS lookup, HTTP, WebSocket, UART, settings or audio operation executed.'
