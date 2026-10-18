[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$ExecutablePath,
  [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedHash,
  [Parameter(Mandatory)][int]$ExpectedProcessId,
  [Parameter(Mandatory)][string]$ReceiptPath,
  [string]$ProgressPath,
  [ValidateRange(60,1800)][int]$Seconds=600,
  [ValidateRange(1,10)][int]$IntervalSeconds=2,
  [string]$UsbPort='COM22',
  [string]$Endpoint='http://ledboard.local'
)
# Adapt the already-reviewed Run-DesktopCandidate/Inspect-RealtimeDesktop
# identity checks and typed owner IPC. No direct serial/network reader, device
# mutation, settings-file rewrite, audio, process replacement or forced close.
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$OutputEncoding=[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
if(-not [IO.Path]::IsPathRooted($ExecutablePath) -or -not [IO.Path]::IsPathRooted($ReceiptPath) -or
   (Test-Path -LiteralPath $ReceiptPath)){throw 'Absolute owner and unused receipt paths required'}
if($ProgressPath -and (-not [IO.Path]::IsPathRooted($ProgressPath) -or
   (Test-Path -LiteralPath $ProgressPath))){throw 'Progress path must be absolute and unused'}
$session=(Get-Process -Id $PID).SessionId
if($session -eq 0){throw 'Run in the existing interactive user session'}
$samples=[Collections.Generic.List[object]]::new();$last='';$phase='USB';$timer=[Diagnostics.Stopwatch]::StartNew()
$proof=[ordered]@{owner=$ExpectedProcessId;sha256=$ExpectedHash;session=$session;requested_seconds=$Seconds;started_utc=[DateTime]::UtcNow.ToString('o');before=$null;after=$null;samples=$samples;error=$null}
$progress=$null
if($ProgressPath){$stream=[IO.File]::Open($ProgressPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read);$progress=[IO.StreamWriter]::new($stream,[Text.UTF8Encoding]::new($false));$progress.AutoFlush=$true}
function OwnerStatus {
  $owner=Get-CimInstance Win32_Process -Filter "ProcessId=$ExpectedProcessId"
  if(-not $owner -or $owner.ExecutablePath -ine $ExecutablePath -or $owner.SessionId -ne $session -or
     (Get-FileHash -LiteralPath $ExecutablePath).Hash.ToLowerInvariant() -cne $ExpectedHash.ToLowerInvariant()){throw 'Expected owner changed; no further IPC'}
  $raw=& $ExecutablePath --app-status
  if($LASTEXITCODE){throw 'Owner status IPC failed'}
  $s=$raw|ConvertFrom-Json
  if($s.process_id -ne $ExpectedProcessId -or $s.ready -ne $true){throw 'Unexpected IPC owner'}
  return $s
}
function NoDraft($s) {
  foreach($key in @('preferences_open','preferences_dirty','mapping_open','workspaces_open')){
    if($s.$key -ne $false){throw "Open/unknown draft: $key"}
  }
  if($s.device_settings.open -ne $false -or $s.device_settings.write_pending -ne $false -or
     $s.device_settings.maintenance_pending -ne $false -or $s.cell_management.open -ne $false){throw 'Device/cell editor or operation is open'}
}
function SelectRoute([string]$route) {
  $s=OwnerStatus;NoDraft $s
  if($route -eq 'USB'){$raw=& $ExecutablePath --usb --port $UsbPort}
  else{$raw=& $ExecutablePath --wifi --endpoint $Endpoint}
  if($LASTEXITCODE){throw 'Typed route selection failed; no fallback'}
  $reply=$raw|ConvertFrom-Json
  if($reply.accepted -ne $true -or $reply.process_id -ne $ExpectedProcessId){throw 'Route selection not accepted by owner'}
}
try {
  $proof.before=OwnerStatus;SelectRoute 'USB'
  do {
    if($phase -eq 'USB' -and $timer.Elapsed.TotalSeconds -ge ($Seconds/2)){
      SelectRoute 'Wi-Fi';$phase='Wi-Fi'
    }
    $s=OwnerStatus
    $row=[ordered]@{elapsed_s=[math]::Round($timer.Elapsed.TotalSeconds,2);phase=$phase;connection=$s.connection;
      usb=$s.serial_live;websocket=$s.websocket_live;mapping=$s.mapping;title=$s.app_title;
      diagnostics=$s.diagnostics;messages=$s.messages;notifications=$s.notifications;presentation=$s.presentation;usb_snapshot=$s.usb_snapshot;
      panels=$s.panels;capabilities=$s.device_status.capabilities;board=$s.device_status.board_id;boot=$s.device_status.boot_id}
    $samples.Add([pscustomobject]$row)
    $key=@($s.connection.method,$s.connection.online,$s.serial_live,$s.websocket_live,$s.diagnostics.active,$s.diagnostics.repeated_occurrences,$s.messages.revision,$s.app_title,($s.panels -join '|')) -join '|'
    if($key -ne $last){$line=[pscustomobject]@{elapsed_s=$row.elapsed_s;phase=$phase;connection=$s.connection;usb=$s.serial_live;ws=$s.websocket_live;diagnostics=$s.diagnostics;messages=$s.messages;panels=$s.panels;usb_snapshot=$s.usb_snapshot}|ConvertTo-Json -Depth 9 -Compress;Write-Output $line;if($progress){$progress.WriteLine($line)};$last=$key}
    Start-Sleep -Seconds $IntervalSeconds
  }while($timer.Elapsed.TotalSeconds -lt $Seconds)
  $proof.after=OwnerStatus
}catch{$proof.error=$_.Exception.Message;Write-Output ('MONITOR_ERROR '+$proof.error)}
finally {
  $proof.elapsed_seconds=$timer.Elapsed.TotalSeconds
  $proof.ended_utc=[DateTime]::UtcNow.ToString('o')
  if($progress){$progress.Dispose()}
  $file=[IO.File]::Open($ReceiptPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
  try{$bytes=[Text.Encoding]::UTF8.GetBytes(($proof|ConvertTo-Json -Depth 24));$file.Write($bytes,0,$bytes.Length)}finally{$file.Dispose()}
}
if($proof.error){exit 1}
Write-Output ('MONITOR_COMPLETE '+$ReceiptPath)
