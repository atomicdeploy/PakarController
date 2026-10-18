[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$ExecutablePath,
  [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedHash,
  [Parameter(Mandatory)][int]$ExpectedProcessId,
  [Parameter(Mandatory)][string]$LaunchTask,
  [Parameter(Mandatory)][string]$ReceiptPath,
  [switch]$ClearTestSelection
)
# Adapt the existing replace/observe-desktop owner, draft and graceful lifecycle
# checks. Only UI selection and selected connection lifecycle change; no LED,
# serial reader, board reset, firmware, credentials or settings-file rewrite.
$ErrorActionPreference='Stop'
$OutputEncoding=[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
if(-not [IO.Path]::IsPathRooted($ExecutablePath) -or -not [IO.Path]::IsPathRooted($ReceiptPath) -or
   (Test-Path -LiteralPath $ReceiptPath)){throw 'Explicit executable and unused receipt required'}
$session=(Get-Process -Id $PID).SessionId
if($session -eq 0){throw 'Use the existing interactive user session'}
$task=Get-ScheduledTask -TaskName $LaunchTask
if([string]$task.Principal.LogonType -ne 'Interactive' -or
   @($task.Actions).Count -ne 1 -or $task.Actions[0].Execute -ine $ExecutablePath -or
   $task.Actions[0].Arguments){throw 'Existing production task identity changed'}
$ownerId=$ExpectedProcessId
function Status {
  $owner=Get-CimInstance Win32_Process -Filter "ProcessId=$ownerId"
  if(-not $owner -or $owner.SessionId -ne $session -or $owner.ExecutablePath -ine $ExecutablePath -or
     (Get-FileHash -LiteralPath $ExecutablePath).Hash.ToLowerInvariant() -ne $ExpectedHash.ToLowerInvariant()){throw 'Owner changed'}
  $raw=& $ExecutablePath --app-status
  if($LASTEXITCODE){throw 'IPC owner unavailable'}
  $s=$raw|ConvertFrom-Json
  if($s.process_id -ne $ownerId -or -not $s.ready){throw 'IPC owner unavailable'}
  return $s
}
function NoDraft($s) {
  foreach($key in @('preferences_open','preferences_dirty','mapping_open','workspaces_open')){
    if($s.$key -ne $false){throw "Open/unknown draft: $key"}
  }
  if($s.device_settings.open -ne $false -or $s.device_settings.write_pending -ne $false -or
     $s.device_settings.maintenance_pending -ne $false -or $s.cell_management.open -ne $false -or
     $s.screenshot.pending -ne $false -or $s.connection.releasing){throw 'Open/pending editor or operation'}
}
function Accepted($reply) {
  if($LASTEXITCODE -or $reply.process_id -ne $ownerId -or $reply.accepted -ne $true){throw 'IPC action unconfirmed; not replayed'}
}
function Selected($s) {
  if((@($s.selected_cells) -join ',') -ne '1,21,80'){throw 'Selection was not retained'}
}
function Online {
  $until=[DateTime]::UtcNow.AddSeconds(25)
  $s=$null
  do {
    Start-Sleep -Milliseconds 250
    try{$s=Status}catch{if($_.Exception.Message -ne 'IPC owner unavailable'){throw}}
  }while(($null -eq $s -or -not $s.connection.online) -and [DateTime]::UtcNow -lt $until)
  if(-not $s.connection.online){throw 'Fresh live peer unavailable; no further lifecycle test'}
  return $s
}
$proof=[ordered]@{before=$null;selected=$null;reconnected=$null;restarted=$null;after=$null;error=$null}
try {
  $proof.before=Status;NoDraft $proof.before
  if($proof.before.connection.method -ne 'Wi-Fi' -or -not $proof.before.connection.online){throw 'Test requires existing selected live Wi-Fi owner'}
  $endpoint=$proof.before.device_status.canonical_url
  if(-not $endpoint){$endpoint='http://ledboard.local'}
  $null=& $ExecutablePath --find-cell '1,21,80'
  if($LASTEXITCODE){throw 'Find-cell IPC failed; verify actual selection, not a text admission'}
  Start-Sleep -Seconds 2
  $proof.selected=Status;Selected $proof.selected;NoDraft $proof.selected
  $reply=& $ExecutablePath --disconnect|ConvertFrom-Json;Accepted $reply
  Start-Sleep -Milliseconds 500
  $offline=Status;Selected $offline
  $reply=& $ExecutablePath --wifi --endpoint $endpoint|ConvertFrom-Json;Accepted $reply
  $proof.reconnected=Online;Selected $proof.reconnected;NoDraft $proof.reconnected
  $reply=& $ExecutablePath --quit|ConvertFrom-Json;Accepted $reply
  $until=[DateTime]::UtcNow.AddSeconds(15)
  do{Start-Sleep -Milliseconds 150;$old=Get-Process -Id $ownerId -ErrorAction SilentlyContinue}while($old -and [DateTime]::UtcNow -lt $until)
  if($old){throw 'Owner did not exit; no kill or replacement'}
  Start-ScheduledTask -TaskName $LaunchTask
  $until=[DateTime]::UtcNow.AddSeconds(15)
  do{Start-Sleep -Milliseconds 200;$owners=@(Get-CimInstance Win32_Process -Filter "Name='pakarcontroller.exe'" | Where-Object {$_.ExecutablePath -ieq $ExecutablePath -and $_.SessionId -eq $session})}while($owners.Count -ne 1 -and [DateTime]::UtcNow -lt $until)
  if($owners.Count -ne 1){throw 'New owner not observed'}
  $ownerId=$owners[0].ProcessId
  $proof.restarted=Online;Selected $proof.restarted;NoDraft $proof.restarted
  # Restore the user's original selection, not a persistent test highlight.
  $query=if($ClearTestSelection){' '}elseif($proof.before.cell_search){$proof.before.cell_search}else{' '}
  $null=& $ExecutablePath --find-cell $query
  if($LASTEXITCODE){throw 'Original Find restoration failed'}
  Start-Sleep -Seconds 2
  $proof.after=Status
  $expectedSelection=if($ClearTestSelection){''}else{@($proof.before.selected_cells)-join ','}
  if((@($proof.after.selected_cells)-join ',') -ne $expectedSelection){throw 'Original selection restoration differs'}
}catch{$proof.error=$_.Exception.Message;throw}
finally {
  $stream=[IO.File]::Open($ReceiptPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
  try{$bytes=[Text.Encoding]::UTF8.GetBytes(($proof|ConvertTo-Json -Depth 24));$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
}
$proof | Select-Object error,@{n='process_id';e={$_.after.process_id}} | ConvertTo-Json -Compress
