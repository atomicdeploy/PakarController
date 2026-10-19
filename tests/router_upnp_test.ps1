$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
# Import parsing/read-only admission only: no network, login or router mutations.
. (Join-Path $taskRoot 'tools/check-router-upnp.ps1') -LocalAddress '192.0.2.2' -Gateway '192.0.2.1' -OutputPath ([IO.Path]::GetFullPath('unused-router-receipt.json')) -NoProbe
function Require($Value,[string]$Message){if(-not $Value){throw $Message}}
function Reject([scriptblock]$Action,[string]$Message){$failed=$false;try{& $Action|Out-Null}catch{$failed=$true};Require $failed $Message}
$headers=Read-DiscoveryHeaders "HTTP/1.1 200 OK`r`nLocation: http://192.0.2.1/igd.xml`r`nST: upnp:rootdevice`r`n`r`n"
Require ($headers.location -eq 'http://192.0.2.1/igd.xml') 'Valid SSDP response not parsed'
foreach($wire in @("HTTP/1.1 2000 no`r`n`r`n","HTTP/1.1 200 OK`r`nST: a`r`nst: b`r`n`r`n","HTTP/1.1 200 OK`r`nbad-header`r`n`r`n",('x'*4097))){Reject {Read-DiscoveryHeaders $wire} 'Malformed or unbounded SSDP accepted'}
$description=[Uri]'http://192.0.2.1:5431/root/description.xml'
Require ((Gateway-Url '../control' $description).AbsoluteUri -eq 'http://192.0.2.1:5431/control') 'Relative control URL resolution failed'
foreach($url in @('http://192.0.2.3/control','https://192.0.2.1/control','http://user@192.0.2.1/control','http://192.0.2.1/control?q=1','http://192.0.2.1/control#x','//example.com/igd')){Reject {Gateway-Url $url $description} 'Untrusted gateway URL accepted'}
$xml=Read-SafeXml '<root xmlns="urn:schemas-upnp-org:device-1-0"><URLBase>http://192.0.2.1:5000/base/</URLBase><device><modelName>Fixture</modelName></device></root>'
$base=Gateway-Url $xml.SelectSingleNode("/*[local-name()='root']/*[local-name()='URLBase']").InnerText $description
Require ((Gateway-Url 'control' $base).AbsoluteUri -eq 'http://192.0.2.1:5000/base/control') 'URLBase service resolution failed'
Reject {Read-SafeXml '<!DOCTYPE root [<!ENTITY x SYSTEM "file:///not-read">]><root>&x;</root>'} 'External entities/DTD accepted'
Reject {Read-SafeXml ('<root>'+('x'*131073)+'</root>')} 'Oversized XML accepted'
foreach($action in @('AddPortMapping','AddAnyPortMapping','DeletePortMapping','ForceTermination','RequestConnection')){Reject {Invoke-GatewayHttp $description $action 'urn:schemas-upnp-org:service:WANIPConnection:1'} 'Router mutation action accepted'}
Reject {Invoke-GatewayHttp $description 'GetStatusInfo' 'invalid-service'} 'Unvalidated SOAP namespace accepted'
Write-Host 'UPnP read-only probe: SSDP bounds/duplicates, gateway-only URLs, URLBase, safe XML and mutation refusal passed (offline fixtures, not router acceptance)'
