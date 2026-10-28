$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern lan-link-discovery -Gate | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern build-storage -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
Push-Location $taskRoot
try {
  & node tools/generate-board-icon-asset.mjs --check
  if($LASTEXITCODE){throw 'Board icon asset stale'}
  & node tests/upnp_description_test.mjs
  if($LASTEXITCODE){throw 'UPnP description/icon fixture failed'}
  foreach($taskRole in @('table','keys','app-icon')){
    [xml]$taskXml=Get-Content -Raw -LiteralPath "build/native/upnp-description-$taskRole.xml"
    if($taskXml.root.device.iconList.icon.url -ne '/board-icon.png'){throw 'Missing public UPnP icon'}
    if($taskXml.root.device.friendlyName -ne 'Table & Keys <One> "name" ''ok'''){throw 'Invalid XML escaping'}
  }
  & tests/router_upnp_test.ps1
} finally {Pop-Location}
