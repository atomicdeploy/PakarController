# Shared tooling identity. One process session, actual installed release, no
# machine inventory/network probe merely for preparing HTTP/SSDP headers.
function Get-PakarToolHeaders {
  if(-not $script:PakarToolPeerSession){$script:PakarToolPeerSession=[Guid]::NewGuid().ToString()}
  $release=Get-Content -LiteralPath (Join-Path $PSScriptRoot '../interfaces/client-release.json') -Raw|ConvertFrom-Json
  if($release.product -ne 'PakarController' -or $release.version -notmatch '^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$' -or $release.build -lt 1){throw 'Invalid installed tool release identity'}
  return [ordered]@{'User-Agent'=('PakarController/'+$release.version+' (tool; PowerShell)');'X-Pakar-Client'='pakarcontroller-tool';'X-Pakar-Version'=[string]$release.version;'X-Pakar-Client-Id'=$script:PakarToolPeerSession;'X-Pakar-Build'=[string]$release.build}
}
function Get-PakarToolSsdpAgent {
  $headers=Get-PakarToolHeaders
  return 'PowerShell/'+$PSVersionTable.PSVersion.Major+' UPnP/1.1 PakarController/'+$headers['X-Pakar-Version']
}
function New-PakarToolUsbHello {
  $headers=Get-PakarToolHeaders;$version=$headers['X-Pakar-Version']
  if($version -notmatch '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$'){throw 'USB numeric build identity requires a stable major.minor.patch release'}
  $parts=@([int]$Matches[1],[int]$Matches[2],[int]$Matches[3])
  foreach($part in $parts){if($part -gt 255){throw 'USB version component exceeds its wire representation'}}
  return ,([byte[]]@(3,$parts[0],$parts[1],$parts[2])) # Controller role, not AVR or API selector.
}
function Assert-PakarToolUsbHello([byte[]]$Reply) {
  Assert-PakarNetworkEspHello $Reply
  if($Reply[4] -lt 9 -or $Reply.Length -lt 15){throw 'ESP build identity is unavailable; no settings were changed'}
  Assert-PakarToolPeerCompatible @{version=([string]$Reply[11]+'.'+$Reply[12]+'.'+$Reply[13])}
}
function ConvertTo-PakarToolSemver([string]$Value) {
  if($Value.Length -gt 63 -or $Value -cnotmatch '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$'){return $null}
  $numbers=@($Matches[1],$Matches[2],$Matches[3]);$pre=@();if($Matches[4]){$pre=@($Matches[4].Split('.'))}
  foreach($n in $numbers){$parsed=0L;if(-not [long]::TryParse($n,[ref]$parsed) -or $parsed -gt [uint32]::MaxValue){return $null}}
  foreach($part in $pre){if($part -match '^\d+$' -and $part.Length -gt 1 -and $part.StartsWith('0')){return $null}}
  return @{numbers=$numbers;pre=$pre}
}
function Compare-PakarToolVersion([string]$Left,[string]$Right) {
  $a=ConvertTo-PakarToolSemver $Left;$b=ConvertTo-PakarToolSemver $Right;if(-not $a -or -not $b){return $null}
  for($i=0;$i -lt 3;$i++){if($a.numbers[$i] -ne $b.numbers[$i]){if([long]$a.numbers[$i] -lt [long]$b.numbers[$i]){return -1};return 1}}
  if(-not $a.pre.Count -or -not $b.pre.Count){if($a.pre.Count -eq $b.pre.Count){return 0};if($a.pre.Count){return -1};return 1}
  for($i=0;$i -lt [Math]::Max($a.pre.Count,$b.pre.Count);$i++){
    if($i -ge $a.pre.Count){return -1};if($i -ge $b.pre.Count){return 1}
    $x=$a.pre[$i];$y=$b.pre[$i];if($x -ceq $y){continue};$nx=$x -match '^\d+$';$ny=$y -match '^\d+$'
    if($nx -and $ny -and $x.Length -ne $y.Length){if($x.Length -lt $y.Length){return -1};return 1}
    if($nx -ne $ny){if($nx){return -1};return 1}
    if([string]::CompareOrdinal($x,$y) -lt 0){return -1};return 1
  }
  return 0
}
function Assert-PakarToolPeerCompatible($Peer) {
  $policy=Get-Content -LiteralPath (Join-Path $PSScriptRoot '../interfaces/peer-policy.json') -Raw|ConvertFrom-Json
  if(-not $policy.enforce -or [Environment]::GetEnvironmentVariable('PAKAR_ENFORCE_PEER_VERSIONS') -in @('0','false')){return}
  $release=Get-PakarToolHeaders
  $board=Compare-PakarToolVersion ([string]$Peer.version) ([string]$policy.minimum_board)
  $maximum=if($policy.maximum_board){Compare-PakarToolVersion ([string]$Peer.version) ([string]$policy.maximum_board)}else{-1}
  if($null -eq $board -or $board -lt 0 -or $null -eq $maximum -or $maximum -ge 0){throw 'Unsupported board version; update the board before connecting'}
  if($Peer.compatibility -and $Peer.compatibility.enforced -ne $false -and $null -ne $Peer.compatibility.minimum_app){
    $app=Compare-PakarToolVersion $release['X-Pakar-Version'] ([string]$Peer.compatibility.minimum_app)
    $maximum=if($Peer.compatibility.maximum_app){Compare-PakarToolVersion $release['X-Pakar-Version'] ([string]$Peer.compatibility.maximum_app)}else{-1}
    if($null -eq $app -or $app -lt 0 -or $null -eq $maximum -or $maximum -ge 0){throw 'Unsupported application version; update the tool before connecting'}
  }
}
