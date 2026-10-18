[CmdletBinding()]
param(
  [string]$DeviceHost='ledboard.local',
  [Parameter(Mandatory)][string]$LocalAddress,
  [ValidateRange(250,10000)][int]$WindowMs=2500,
  [ValidateSet(0,5353)][int]$DnsSourcePort=0,
  [string]$OutputPath,
  [switch]$IncludeHttp,
  [switch]$NoProbe
)
# Read-only, single-device LAN evidence. Never enables interfaces/firewall,
# changes settings, opens UART, requests a melody or sends an actuator command.
# SSDP request/deadline structure adapts desktop/src/platform/discovery.rs and
# PCController internal/discovery/discovery.go discoverSSDP/parseSSDPResponse.
# DNS compression bounds adapt protocols.go skipDNSName; MNDP TLV layout comes
# from PU850 LocalLib/MNDP.{h,cpp}, shared MndpService.h and mndp_test.cpp.
# These are wire diagnostics, not a replacement discovery engine or physical proof.
$ErrorActionPreference='Stop'

function Read-PakarU16([byte[]]$Packet,[int]$At) {
  if($At -lt 0 -or $At+2 -gt $Packet.Length){throw 'Truncated wire integer'}
  return ([int]$Packet[$At]*256)+$Packet[$At+1]
}
function Read-PakarDnsName([byte[]]$Packet,[ref]$Position) {
  $at=[int]$Position.Value;$next=-1;$labels=[Collections.Generic.List[string]]::new()
  for($step=0;$step -lt 64;$step++) {
    if($at -lt 0 -or $at -ge $Packet.Length){throw 'DNS name outside datagram'}
    $length=[int]$Packet[$at++]
    if(($length -band 192) -eq 192) {
      if($at -ge $Packet.Length){throw 'Truncated DNS compression pointer'}
      if($next -lt 0){$next=$at+1}
      $at=(($length -band 63)*256)+$Packet[$at];continue
    }
    if($length -eq 0){$Position.Value=$(if($next -ge 0){$next}else{$at});return ($labels -join '.')}
    if($length -gt 63 -or $at+$length -gt $Packet.Length){throw 'Invalid DNS label'}
    $labels.Add([Text.Encoding]::UTF8.GetString($Packet,$at,$length));$at+=$length
  }
  throw 'DNS compression loop/label limit'
}
function ConvertFrom-PakarDns([byte[]]$Packet) {
  if($Packet.Length -lt 12 -or -not ($Packet[2] -band 128)){throw 'Not a DNS response'}
  $at=12;$questions=Read-PakarU16 $Packet 4
  if($questions -gt 64){throw 'Too many DNS questions'}
  for($i=0;$i -lt $questions;$i++){$null=Read-PakarDnsName $Packet ([ref]$at);$at+=4;if($at -gt $Packet.Length){throw 'Truncated question'}}
  $records=(Read-PakarU16 $Packet 6)+(Read-PakarU16 $Packet 8)+(Read-PakarU16 $Packet 10)
  if($records -gt 128){throw 'Too many DNS records'}
  for($i=0;$i -lt $records;$i++) {
    $name=Read-PakarDnsName $Packet ([ref]$at);$type=Read-PakarU16 $Packet $at;$class=Read-PakarU16 $Packet ($at+2)
    $length=Read-PakarU16 $Packet ($at+8);$at+=10;$end=$at+$length
    if($end -gt $Packet.Length){throw 'Truncated DNS data'}
    $value=$null
    switch($type) {
      1 {if($length -ne 4){throw 'Invalid A record'};$value=([Net.IPAddress]::new([byte[]]$Packet[$at..($end-1)])).ToString()}
      12 {$cursor=$at;$value=Read-PakarDnsName $Packet ([ref]$cursor);if($cursor -gt $end){throw 'PTR exceeds record'}}
      33 {if($length -lt 7){throw 'Invalid SRV record'};$port=Read-PakarU16 $Packet ($at+4);$cursor=$at+6;$target=Read-PakarDnsName $Packet ([ref]$cursor);if($cursor -gt $end){throw 'SRV exceeds record'};$value=@{port=$port;target=$target}}
      16 {$txt=[Collections.Generic.List[string]]::new();$cursor=$at;while($cursor -lt $end){$n=[int]$Packet[$cursor++];if($cursor+$n -gt $end){throw 'Invalid TXT length'};$txt.Add([Text.Encoding]::UTF8.GetString($Packet,$cursor,$n));$cursor+=$n};$value=@($txt.ToArray())}
    }
    [PSCustomObject]@{name=$name;type=$type;class=($class -band 32767);value=$value};$at=$end
  }
}
function New-PakarDnsQuery([object[]]$Questions) {
  $bytes=[Collections.Generic.List[byte]]::new();$bytes.AddRange([byte[]](0,0,0,0,0,$Questions.Count,0,0,0,0,0,0))
  foreach($q in $Questions){foreach($label in $q.name.Split('.')){$raw=[Text.Encoding]::UTF8.GetBytes($label);if($raw.Length -lt 1 -or $raw.Length -gt 63){throw 'Invalid query label'};$bytes.Add([byte]$raw.Length);$bytes.AddRange($raw)};$bytes.AddRange([byte[]](0,0,$q.type,128,1))}
  return ,$bytes.ToArray() # QU asks for a unicast reply; multicast records are retained too.
}
function ConvertFrom-PakarMndp([byte[]]$Packet) {
  if($Packet.Length -lt 4 -or $Packet[0] -ne 0 -or $Packet[1] -ne 0){throw 'Invalid MNDP header'}
  $fields=[ordered]@{sequence=(Read-PakarU16 $Packet 2)};$at=4
  while($at -lt $Packet.Length) {
    $type=Read-PakarU16 $Packet $at;$n=Read-PakarU16 $Packet ($at+2);$at+=4
    if($at+$n -gt $Packet.Length){throw 'Truncated MNDP TLV'}
    $value=if($n){[Text.Encoding]::UTF8.GetString($Packet,$at,$n)}else{''}
    if($type -eq 1 -and $n -eq 6){$value=($Packet[$at..($at+5)]|ForEach-Object{$_.ToString('x2')}) -join ':'}
    if($type -eq 17 -and $n -eq 4){$value=([Net.IPAddress]::new([byte[]]$Packet[$at..($at+3)])).ToString()}
    if($type -eq 10 -and $n -eq 4){$value=[BitConverter]::ToUInt32($Packet,$at)}
    $fields[[string]$type]=$value;$at+=$n
  }
  return $fields
}
function Invoke-PakarUdpProbe([string]$Name,[byte[]]$Request,[string]$Target,[int]$Port,[bool]$Mdns=$false) {
  $udp=[Net.Sockets.UdpClient]::new([Net.Sockets.AddressFamily]::InterNetwork)
  $packets=[Collections.Generic.List[object]]::new();$errorText=$null
  try {
    $udp.ExclusiveAddressUse=$false
    $udp.Client.SetSocketOption([Net.Sockets.SocketOptionLevel]::Socket,[Net.Sockets.SocketOptionName]::ReuseAddress,$true)
    # RFC6762 legacy unicast replies avoid competing with Windows DNS/Bonjour
    # for a shared5353 unicast socket.5353 remains an explicit multicast probe.
    $bind=if($Mdns -and $DnsSourcePort -eq 5353){[Net.IPEndPoint]::new([Net.IPAddress]::Any,5353)}else{[Net.IPEndPoint]::new($script:ProbeLocal,0)}
    $udp.Client.Bind($bind);$udp.Client.ReceiveTimeout=180
    $udp.Client.SetSocketOption([Net.Sockets.SocketOptionLevel]::IP,[Net.Sockets.SocketOptionName]::MulticastInterface,$script:ProbeLocal.GetAddressBytes())
    $udp.Client.SetSocketOption([Net.Sockets.SocketOptionLevel]::IP,[Net.Sockets.SocketOptionName]::MulticastTimeToLive,$(if($Mdns){255}else{2}))
    if($Mdns){$udp.JoinMulticastGroup([Net.IPAddress]::Parse('224.0.0.251'),$script:ProbeLocal)}
    $destination=[Net.IPEndPoint]::new([Net.IPAddress]::Parse($Target),$Port)
    $null=$udp.Send($Request,$Request.Length,$destination);$timer=[Diagnostics.Stopwatch]::StartNew()
    while($timer.ElapsedMilliseconds -lt $WindowMs -and $packets.Count -lt 64) {
      $remote=[Net.IPEndPoint]::new([Net.IPAddress]::Any,0)
      try{$data=$udp.Receive([ref]$remote)}catch [Net.Sockets.SocketException]{if($_.Exception.SocketErrorCode -eq 'TimedOut'){continue};throw}
      if($remote.Address.ToString() -ne $script:ProbeDevice.ToString()){continue} # Never catalogue unrelated LAN peers.
      $packet=[ordered]@{source=$remote.ToString();bytes=$data.Length;wire_base64=[Convert]::ToBase64String($data)}
      try {
        if($Mdns){$packet.records=@(ConvertFrom-PakarDns $data)}
        elseif($Port -eq 5678){$packet.fields=ConvertFrom-PakarMndp $data}
        else{$packet.text=[Text.Encoding]::UTF8.GetString($data);$packet.valid_response=$packet.text.StartsWith('HTTP/1.1 200')}
      }catch{$packet.parse_error=$_.Exception.Message}
      $packets.Add($packet)
    }
  }catch{$errorText=$_.Exception.Message}finally{$udp.Dispose()}
  return [PSCustomObject]@{probe=$Name;request_base64=[Convert]::ToBase64String($Request);destination="${Target}:$Port";local=$LocalAddress;error=$errorText;packets=@($packets.ToArray())}
}
function Invoke-PakarReadOnlyHttp {
  Add-Type -AssemblyName System.Net.Http
  $handler=[Net.Http.HttpClientHandler]::new();$handler.UseProxy=$false
  $client=[Net.Http.HttpClient]::new($handler);$client.Timeout=[TimeSpan]::FromSeconds(6)
  $result=[ordered]@{};$auth=$null
  try {
    if($env:PAKAR_USER -and $env:PAKAR_PASSWORD){$auth='Basic '+[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($env:PAKAR_USER+':'+$env:PAKAR_PASSWORD));$client.DefaultRequestHeaders.TryAddWithoutValidation('Authorization',$auth)|Out-Null}
    foreach($path in @('/api/status','/api/network','/api/time','/description.xml')) {
      try{$response=$client.GetAsync("http://$DeviceHost$path").GetAwaiter().GetResult();$body=$response.Content.ReadAsStringAsync().GetAwaiter().GetResult();$result[$path]=@{status=[int]$response.StatusCode;body=$body};$response.Dispose()}
      catch{$result[$path]=@{error=$_.Exception.Message}}
    }
    $ws=[Net.WebSockets.ClientWebSocket]::new();$cancel=[Threading.CancellationTokenSource]::new(6000)
    try {
      if($auth){$ws.Options.SetRequestHeader('Authorization',$auth)}
      $ws.Options.Proxy=$null;$null=$ws.ConnectAsync([Uri]"ws://$DeviceHost/ws",$cancel.Token).GetAwaiter().GetResult()
      $initial=Read-PakarWebSocketMessage $ws $cancel.Token
      $request=[Text.Encoding]::UTF8.GetBytes('{"type":"telemetry"}')
      $null=$ws.SendAsync([ArraySegment[byte]]::new($request),[Net.WebSockets.WebSocketMessageType]::Text,$true,$cancel.Token).GetAwaiter().GetResult()
      $reply=Read-PakarWebSocketMessage $ws $cancel.Token
      $result.websocket=@{state=$ws.State.ToString();initial=$initial;readonly_request='{"type":"telemetry"}';reply=$reply}
    }catch{$result.websocket=@{error=$_.Exception.Message}}finally{$ws.Abort();$ws.Dispose();$cancel.Dispose()}
  }finally{$client.Dispose();$handler.Dispose();$auth=$null}
  return $result
}
function Read-PakarWebSocketMessage($Socket,[Threading.CancellationToken]$Token) {
  $buffer=[byte[]]::new(16384);$offset=0
  do {$segment=[ArraySegment[byte]]::new($buffer,$offset,$buffer.Length-$offset);$received=$Socket.ReceiveAsync($segment,$Token).GetAwaiter().GetResult();$offset+=$received.Count;if($offset -eq $buffer.Length -and -not $received.EndOfMessage){throw 'WebSocket snapshot exceeds bounded capture'}}while(-not $received.EndOfMessage)
  if($received.MessageType -ne [Net.WebSockets.WebSocketMessageType]::Text){throw 'Expected read-only JSON WebSocket text reply'}
  return [PSCustomObject]@{message_type=$received.MessageType.ToString();body=[Text.Encoding]::UTF8.GetString($buffer,0,$offset)}
}
if($NoProbe){return} # Parser fixtures can dot-source without any DNS/socket/API operation.
if($OutputPath -and (Test-Path -LiteralPath $OutputPath)){throw 'Evidence path already exists; choose a new meaningful non-timestamp name'}
$script:ProbeLocal=[Net.IPAddress]::Parse($LocalAddress)
if($script:ProbeLocal.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork){throw 'Select the actual LAN IPv4 interface'}
$script:ProbeDevice=[Net.Dns]::GetHostAddresses($DeviceHost)|Where-Object AddressFamily -eq InterNetwork|Select-Object -First 1
if(-not $script:ProbeDevice){throw 'Board hostname does not resolve; no broad IP scan/fallback performed'}
$questions=@(@{name='_http._tcp.local';type=12},@{name='_pakarcontroller._tcp.local';type=12},@{name=$DeviceHost;type=1})
$dns=Invoke-PakarUdpProbe 'dns-sd-ptr-a' (New-PakarDnsQuery $questions) '224.0.0.251' 5353 $true
$instances=@($dns.packets|ForEach-Object{$_.records}|Where-Object {$_.type -eq 12 -and $_.value -like '*.local'}|ForEach-Object value|Select-Object -Unique -First 8)
$follow=$null
if($instances.Count){$next=@();foreach($instance in $instances){$next+=@{name=$instance;type=33};$next+=@{name=$instance;type=16}};$follow=Invoke-PakarUdpProbe 'dns-sd-srv-txt' (New-PakarDnsQuery $next) '224.0.0.251' 5353 $true}
$search=[Text.Encoding]::ASCII.GetBytes("M-SEARCH * HTTP/1.1`r`nHOST: 239.255.255.250:1900`r`nMAN: `"ssdp:discover`"`r`nMX: 1`r`nST: ssdp:all`r`n`r`n")
$result=[ordered]@{device_host=$DeviceHost;device_address=$script:ProbeDevice.ToString();local_address=$LocalAddress;meaning='Actual bounded wire observations on this interface, not routed-VPN multicast or physical peripheral acceptance';dns_sd=$dns;dns_sd_followup=$follow;ssdp_multicast=(Invoke-PakarUdpProbe 'ssdp-multicast' $search '239.255.255.250' 1900);ssdp_unicast=(Invoke-PakarUdpProbe 'ssdp-unicast' $search $script:ProbeDevice.ToString() 1900);mndp=(Invoke-PakarUdpProbe 'mndp-query' ([byte[]](0,0,0,0)) $script:ProbeDevice.ToString() 5678)}
if($IncludeHttp){$result.http=Invoke-PakarReadOnlyHttp}
$json=$result|ConvertTo-Json -Depth 16
if($OutputPath){$json|Set-Content -LiteralPath $OutputPath -Encoding UTF8}
$json
