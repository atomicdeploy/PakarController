[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$ExecutablePath,
  [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedHash,
  [Parameter(Mandatory)][int]$ExpectedProcessId,
  [Parameter(Mandatory)][string]$ReceiptPath,
  [string]$ResumeReceiptPath
)
# Adapt verify-desktop-workspace's same-session owner/draft/lifecycle guards.
# No selection edit, board command, direct transport, reset or force shutdown.
$ErrorActionPreference='Stop'
$OutputEncoding=[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
if(-not [IO.Path]::IsPathRooted($ExecutablePath) -or -not [IO.Path]::IsPathRooted($ReceiptPath) -or
   (Test-Path -LiteralPath $ReceiptPath)){throw 'Explicit executable and unused receipt required'}
$session=(Get-Process -Id $PID).SessionId
if($session -eq 0){throw 'Run in the existing interactive session'}
function Status([switch]$AllowRelease) {
  $owner=Get-CimInstance Win32_Process -Filter "ProcessId=$ExpectedProcessId"
  if(-not $owner -or $owner.SessionId -ne $session -or $owner.ExecutablePath -ine $ExecutablePath -or
     (Get-FileHash -LiteralPath $ExecutablePath).Hash.ToLowerInvariant() -ne $ExpectedHash.ToLowerInvariant()){throw 'Owner changed'}
  $raw=& $ExecutablePath --app-status
  if($LASTEXITCODE){throw 'Owner IPC failed'}
  $s=$raw|ConvertFrom-Json
  if($s.process_id -ne $ExpectedProcessId -or -not $s.ready){throw 'Owner IPC changed'}
  foreach($key in @('preferences_open','preferences_dirty','mapping_open','workspaces_open')){
    if($s.$key -ne $false){throw "Open/unknown draft: $key"}
  }
  if($s.device_settings.open -ne $false -or $s.device_settings.write_pending -ne $false -or
     $s.device_settings.maintenance_pending -ne $false -or $s.cell_management.open -ne $false -or
     $s.screenshot.pending -ne $false -or (-not $AllowRelease -and $s.connection.releasing)){throw 'Pending editor or operation'}
  return $s
}
function Accepted($reply){if($LASTEXITCODE -or $reply.process_id -ne $ExpectedProcessId -or $reply.accepted -ne $true){throw 'Lifecycle action unconfirmed; not replayed'}}
function Retained($s,$before){
  if($s.workspace.retained_layout_sha256 -cne $before.workspace.retained_layout_sha256 -or
     (@($s.workspace.hidden_panels)-join '|') -cne (@($before.workspace.hidden_panels)-join '|') -or
     (@($s.selected_cells)-join ',') -cne (@($before.selected_cells)-join ',')){throw 'Workspace or selection changed'}
}
$proof=[ordered]@{before=$null;disconnected=$null;reconnected=$null;error=$null}
try{
  if($ResumeReceiptPath){
    if(-not [IO.Path]::IsPathRooted($ResumeReceiptPath)){throw 'Explicit prior receipt required'}
    $prior=Get-Content -LiteralPath $ResumeReceiptPath -Raw|ConvertFrom-Json
    if($prior.before.process_id -ne $ExpectedProcessId -or -not $prior.error){throw 'Prior failed owner receipt required'}
    $proof.before=$prior.before
    $current=Status
    if($current.connection.enabled -or $current.connection.online){throw 'Resume requires the same explicitly disconnected owner'}
    Retained $current $proof.before
  }else{$proof.before=Status}
  if($proof.before.workspace.retained_layout_sha256 -notmatch '^[a-f0-9]{64}$' -or
     -not $proof.before.connection.online -or $proof.before.connection.method -ne 'Wi-Fi'){throw 'Existing live Wi-Fi owner with layout diagnostics required'}
  $endpoint=$proof.before.configured_endpoint
  if(-not $ResumeReceiptPath){$reply=& $ExecutablePath --disconnect|ConvertFrom-Json;Accepted $reply}
  $releaseDeadline=[DateTime]::UtcNow.AddSeconds(20)
  do{Start-Sleep -Milliseconds 300;$released=Status -AllowRelease}while($released.connection.releasing -and [DateTime]::UtcNow -lt $releaseDeadline)
  if($released.connection.releasing){throw 'Transport release did not complete; no new connection attempted'}
  $proof.disconnected=Status;Retained $proof.disconnected $proof.before
  if($proof.disconnected.connection.enabled -or $proof.disconnected.connection.online){throw 'Disconnect not observed'}
  $reply=& $ExecutablePath --wifi --endpoint $endpoint|ConvertFrom-Json;Accepted $reply
  $until=[DateTime]::UtcNow.AddSeconds(60)
  do{
    Start-Sleep -Milliseconds 300
    $s=Status;Retained $s $proof.before
    $ready=$s.connection.online -and $s.device_status.board_id -ceq $proof.before.device_status.board_id -and
      (@($s.panels|Sort-Object)-join '|') -ceq (@($proof.before.panels|Sort-Object)-join '|')
  }while(-not $ready -and [DateTime]::UtcNow -lt $until)
  $proof.reconnected=$s
  if(-not $ready){throw 'Peer/panels not recovered within deadline; no fallback transport or replay'}
}catch{$proof.error=$_.Exception.Message;throw}
finally{
  $stream=[IO.File]::Open($ReceiptPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
  try{$bytes=[Text.Encoding]::UTF8.GetBytes(($proof|ConvertTo-Json -Depth 24));$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
}
$proof|Select-Object error,@{n='process_id';e={$_.reconnected.process_id}},@{n='layout_sha256';e={$_.reconnected.workspace.retained_layout_sha256}}|ConvertTo-Json -Compress
