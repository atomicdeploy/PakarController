[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$LocalAddress,
  [Parameter(Mandatory)][string]$Gateway,
  [ValidateRange(500,5000)][int]$WindowMs=1500,
  [switch]$ScanSubnet,
  [Parameter(Mandatory)][string]$OutputPath,
  [switch]$NoProbe
)
# Scoped read-only LAN/IGD verification. SSDP framing follows PU850 SSDPDevice
# and our actual discovery tool. IGD actions follow the UPnP WANIPConnection
# service specification. No router login, port forwarding, firewall change,
# reboot, broad port scan or board command is made by this diagnostic.
$ErrorActionPreference='Stop'
function Read-DiscoveryHeaders([string]$Text) {
  if($Text.Length -gt 4096 -or $Text -notmatch '^HTTP/1\.1 200(?: [^\r\n]*)?\r\n'){throw 'Not a bounded SSDP response'}
  $headers=@{}
  foreach($line in ($Text -split "\r\n" | Select-Object -Skip 1)) {
    if(-not $line){break}
    $at=$line.IndexOf(':');if($at -lt 1){throw 'Invalid SSDP header'}
    $key=$line.Substring(0,$at).Trim().ToLowerInvariant()
    if($headers.ContainsKey($key)){throw 'Duplicate SSDP header'}
    $headers[$key]=$line.Substring($at+1).Trim()
  }
  return $headers
}
function Read-SafeXml([string]$Text) {
  $settings=[Xml.XmlReaderSettings]::new();$settings.DtdProcessing=[Xml.DtdProcessing]::Prohibit
  $settings.XmlResolver=$null;$settings.MaxCharactersInDocument=131072
  $reader=[Xml.XmlReader]::Create([IO.StringReader]::new($Text),$settings)
  try{$xml=[Xml.XmlDocument]::new();$xml.XmlResolver=$null;$xml.Load($reader);return ,$xml}finally{$reader.Dispose()}
}
function Gateway-Url([string]$Value,[Uri]$Base) {
  $url=[Uri]::new($Base,$Value)
  if($url.Scheme -ne 'http' -or $url.Host -ne $Gateway -or $url.UserInfo -or $url.Fragment -or $url.Query){throw 'Untrusted IGD URL: only the inventoried gateway HTTP origin is allowed'}
  return $url
}
function Invoke-GatewayHttp([Uri]$Url,[string]$Action,[string]$Type,[hashtable]$Arguments=@{}) {
  $Url=Gateway-Url $Url.AbsoluteUri $Url
  $request=[Net.Http.HttpRequestMessage]::new($(if($Action){[Net.Http.HttpMethod]::Post}else{[Net.Http.HttpMethod]::Get}),$Url)
  $response=$null;$deadline=[Threading.CancellationTokenSource]::new(4000)
  try{
    if($Action) {
      if($Action -notin @('GetExternalIPAddress','GetStatusInfo','GetNATRSIPStatus','GetGenericPortMappingEntry')){throw 'Only read-only IGD actions are allowed'}
      if($Type -notmatch '^urn:schemas-upnp-org:service:WAN(IP|PPP)Connection:[12]$'){throw 'Unsupported IGD service type'}
      $fields=($Arguments.Keys|Sort-Object|ForEach-Object{'<'+$_+'>'+[Security.SecurityElement]::Escape([string]$Arguments[$_])+'</'+$_+'>'})-join ''
      $body='<?xml version="1.0"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><s:Body><u:'+ $Action +' xmlns:u="'+$Type+'">'+$fields+'</u:'+$Action+'></s:Body></s:Envelope>'
      $request.Content=[Net.Http.StringContent]::new($body,[Text.Encoding]::UTF8,'text/xml')
      $null=$request.Headers.TryAddWithoutValidation('SOAPAction','"'+$Type+'#'+$Action+'"')
    }
    $response=$script:Client.SendAsync($request,[Net.Http.HttpCompletionOption]::ResponseHeadersRead,$deadline.Token).GetAwaiter().GetResult()
    $stream=$response.Content.ReadAsStreamAsync().GetAwaiter().GetResult();$buffer=[byte[]]::new(4096);$memory=[IO.MemoryStream]::new()
    try{
      do{$count=$stream.ReadAsync($buffer,0,$buffer.Length,$deadline.Token).GetAwaiter().GetResult();if($memory.Length+$count -gt 131072){throw 'Oversized IGD response'};if($count){$memory.Write($buffer,0,$count)}}while($count)
      return @{status=[int]$response.StatusCode;body=[Text.Encoding]::UTF8.GetString($memory.ToArray())}
    }finally{$stream.Dispose();$memory.Dispose()}
  }finally{if($response){$response.Dispose()};$request.Dispose();$deadline.Dispose()}
}
function Invoke-DiscoveryProbe([string]$Destination) {
  $udp=[Net.Sockets.UdpClient]::new([Net.IPEndPoint]::new($script:LocalIp,0));$udp.Client.ReceiveTimeout=150
  $packets=[Collections.Generic.List[object]]::new()
  try{
    $udp.Client.SetSocketOption([Net.Sockets.SocketOptionLevel]::IP,[Net.Sockets.SocketOptionName]::MulticastInterface,$script:LocalIp.GetAddressBytes())
    $udp.Ttl=2
    foreach($st in @('ssdp:all','urn:schemas-upnp-org:device:InternetGatewayDevice:1','urn:schemas-upnp-org:device:InternetGatewayDevice:2')){
      $text='M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: "ssdp:discover"\r\nMX: 1\r\nST: '+$st+'\r\n\r\n'
      # PowerShell uses backtick escapes; replace the literal wire escapes.
      $text=$text.Replace('\r\n',"`r`n").Replace('\"','"')
      $bytes=[Text.Encoding]::ASCII.GetBytes($text);$null=$udp.Send($bytes,$bytes.Length,$Destination,1900)
    }
    $clock=[Diagnostics.Stopwatch]::StartNew()
    while($clock.ElapsedMilliseconds -lt $WindowMs -and $packets.Count -lt 64){
      $sender=[Net.IPEndPoint]::new([Net.IPAddress]::Any,0)
      try{$bytes=$udp.Receive([ref]$sender)}catch [Net.Sockets.SocketException]{
        if($_.Exception.SocketErrorCode -eq 'TimedOut'){continue}
        if($_.Exception.SocketErrorCode -eq 'ConnectionReset'){
          $script:ProbeWarnings+=@{destination=$Destination;message='UDP port unreachable/reset observed; no request replay'};break
        }
        throw
      }
      if($bytes.Length -gt 4096){continue}
      $parts=$sender.Address.ToString().Split('.');if(($parts[0..2]-join '.') -ne ($LocalAddress.Split('.')[0..2]-join '.')){continue}
      try{$headers=Read-DiscoveryHeaders ([Text.Encoding]::ASCII.GetString($bytes));$packets.Add(@{source=$sender.Address.ToString();headers=$headers;wire_base64=[Convert]::ToBase64String($bytes)})}catch{}
    }
  }finally{$udp.Dispose()}
  return @($packets.ToArray())
}
if($NoProbe){return}
if(-not [IO.Path]::IsPathRooted($OutputPath) -or (Test-Path -LiteralPath $OutputPath)){throw 'Use an absolute nonexisting receipt path'}
$script:LocalIp=[Net.IPAddress]::Parse($LocalAddress);$gatewayIp=[Net.IPAddress]::Parse($Gateway)
$interface=Get-NetIPAddress -AddressFamily IPv4|Where-Object IPAddress -eq $LocalAddress|Select-Object -First 1
if(-not $interface -or $interface.PrefixLength -ne 24 -or ($LocalAddress.Split('.')[0..2]-join '.') -ne ($Gateway.Split('.')[0..2]-join '.')){throw 'This bounded probe requires an actual directly connected /24 and its gateway'}
$actual=Get-NetRoute -InterfaceIndex $interface.InterfaceIndex -DestinationPrefix '0.0.0.0/0'|Where-Object NextHop -eq $Gateway
if(-not $actual){throw 'Gateway does not belong to the selected LAN interface'}
Add-Type -AssemblyName System.Net.Http
$handler=[Net.Http.HttpClientHandler]::new();$handler.UseProxy=$false;$handler.AllowAutoRedirect=$false
$script:Client=[Net.Http.HttpClient]::new($handler);$script:Client.Timeout=[TimeSpan]::FromSeconds(4)
$script:ProbeWarnings=@()
$result=[ordered]@{local=$LocalAddress;gateway=$Gateway;mutations=$false;subnet_scan=@();ssdp=@();igd=@();neighbors=@();warnings=@();errors=@()}
try{
  if($ScanSubnet){
    $prefix=$LocalAddress.Split('.')[0..2]-join '.'
    for($start=1;$start -le 254;$start+=32){
      $batch=@()
      for($i=$start;$i -le [Math]::Min($start+31,254);$i++){if("$prefix.$i" -eq $LocalAddress){continue};$ping=[Net.NetworkInformation.Ping]::new();$batch+=@{ping=$ping;task=$ping.SendPingAsync("$prefix.$i",250);address="$prefix.$i"}}
      foreach($item in $batch){try{$reply=$item.task.GetAwaiter().GetResult();if($reply.Status -eq 'Success'){$result.subnet_scan+=@{address=$item.address;rtt_ms=$reply.RoundtripTime}}}finally{$item.ping.Dispose()}}
    }
  }
  $result.ssdp=@(Invoke-DiscoveryProbe '239.255.255.250')
  $result.ssdp+=@(Invoke-DiscoveryProbe $Gateway)
  $result.warnings=$script:ProbeWarnings
  $locations=@($result.ssdp|Where-Object source -eq $Gateway|ForEach-Object{$_.headers['location']}|Where-Object{$_}|Select-Object -Unique -First 4)
  foreach($location in $locations){
    try{
      $url=Gateway-Url $location ([Uri]("http://$Gateway/"));$reply=Invoke-GatewayHttp $url
      if($reply.status -ne 200){throw "IGD description HTTP$($reply.status)"}
      $xml=Read-SafeXml $reply.body
      $base=$url;$declaredBase=$xml.SelectSingleNode("/*[local-name()='root']/*[local-name()='URLBase']")
      if($declaredBase -and $declaredBase.InnerText){$base=Gateway-Url $declaredBase.InnerText $url}
      $device=$xml.SelectSingleNode("//*[local-name()='device']")
      $entry=[ordered]@{description=$url.AbsoluteUri;model=$device.SelectSingleNode("*[local-name()='modelName']").InnerText;friendly_name=$device.SelectSingleNode("*[local-name()='friendlyName']").InnerText;services=@()}
      foreach($service in $xml.SelectNodes("//*[local-name()='service']")){
        $type=$service.SelectSingleNode("*[local-name()='serviceType']").InnerText
        if($type -notmatch '^urn:schemas-upnp-org:service:WAN(IP|PPP)Connection:[12]$'){continue}
        $control=Gateway-Url $service.SelectSingleNode("*[local-name()='controlURL']").InnerText $base
        $observed=[ordered]@{type=$type;control=$control.AbsoluteUri;actions=@{};mappings=@()}
        foreach($action in @('GetExternalIPAddress','GetStatusInfo','GetNATRSIPStatus')){
          $actionReply=Invoke-GatewayHttp $control $action $type
          $observed.actions[$action]=@{status=$actionReply.status;xml=$actionReply.body}
        }
        for($index=0;$index -lt 16;$index++){
          $mapping=Invoke-GatewayHttp $control 'GetGenericPortMappingEntry' $type @{NewPortMappingIndex=$index}
          if($mapping.status -ne 200){$observed.mapping_end=@{index=$index;status=$mapping.status;xml=$mapping.body};break}
          $observed.mappings+=@{index=$index;xml=$mapping.body}
        }
        $entry.services+=$observed
      }
      $result.igd+=$entry
    }catch{$result.errors+=$_.Exception.Message}
  }
  $result.neighbors=@(Get-NetNeighbor -InterfaceIndex $interface.InterfaceIndex -AddressFamily IPv4|Where-Object{ $_.IPAddress -like (($LocalAddress.Split('.')[0..2]-join '.')+'.*') -and $_.IPAddress -notlike '*.255'}|Select-Object IPAddress,LinkLayerAddress,@{n='state';e={$_.State.ToString()}})
}finally{$script:Client.Dispose();$handler.Dispose();$result|ConvertTo-Json -Depth 16|Set-Content -LiteralPath $OutputPath -Encoding UTF8}
[ordered]@{receipt=$OutputPath;hosts=$result.subnet_scan.Count;ssdp_packets=$result.ssdp.Count;gateway_igd=$result.igd.Count;errors=$result.errors;mutations=$false}|ConvertTo-Json -Depth 4
