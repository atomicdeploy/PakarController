[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$ExecutablePath,
  [Parameter(Mandatory)][int]$ExpectedProcessId,
  [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedHash,
  [Parameter(Mandatory)][string]$OriginalScenePath,
  [Parameter(Mandatory)][string]$UpdateReceiptPath,
  [Parameter(Mandatory)][string]$ReceiptPath
)
# One-time, explicit migration from old firmware without scene persistence.
# Adapt the reviewed Restore-UsbAckScene's owner/boot/readback guards. Ordinary
# reconnect never runs this tool. It never retries a mutation or changes a newer
# scene. All LED commands go through the actual running app's selected owner.
$ErrorActionPreference='Stop'
$OutputEncoding=[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
foreach($path in @($ExecutablePath,$OriginalScenePath,$UpdateReceiptPath,$ReceiptPath)){
  if(-not [IO.Path]::IsPathRooted($path)){throw 'Explicit absolute paths required'}
}
if(Test-Path -LiteralPath $ReceiptPath){throw 'Preserve existing receipt'}
$update=Get-Content -LiteralPath $UpdateReceiptPath -Encoding UTF8 -Raw|ConvertFrom-Json
$original=Get-Content -LiteralPath $OriginalScenePath -Encoding UTF8 -Raw|ConvertFrom-Json
if($update.error -or $update.actual_tcp -ne $true -or $update.result.ok -ne $true -or
   $original.board_id -cne $update.result.board_id -or $original.boot_id -cne $update.result.boot_before -or
   $original.mode -ne 'cell' -or $original.cells.Count -ne 80 -or
   $original.effect -notin @('none','rainbow','fire') -or
   @($original.cells|Where-Object {$_ -notin @('white','red','off')}).Count){throw 'Verified update and supported original scene required'}
$session=(Get-Process -Id $PID).SessionId
if($session -eq 0){throw 'Use the interactive owner session'}
function Status {
  $owner=Get-CimInstance Win32_Process -Filter "ProcessId=$ExpectedProcessId"
  if(-not $owner -or $owner.ExecutablePath -ine $ExecutablePath -or $owner.SessionId -ne $session -or
     (Get-FileHash -LiteralPath $ExecutablePath).Hash.ToLowerInvariant() -ne $ExpectedHash.ToLowerInvariant()){throw 'Owner changed'}
  $s=& $ExecutablePath --app-status|ConvertFrom-Json
  if($LASTEXITCODE -or $s.process_id -ne $ExpectedProcessId -or $s.connection.online -ne $true -or
     $s.device_status.board_id -cne $original.board_id -or
     $s.device_status.boot_id -cne $update.result.boot_after){throw 'Live post-update app/board/boot unavailable'}
  return $s
}
function Accepted($reply) {
  if($LASTEXITCODE -or $reply.process_id -ne $ExpectedProcessId -or $reply.accepted -ne $true){throw 'Mutation admission unknown; not replayed'}
}
$proof=[ordered]@{before=$null;after=$null;restored=$false;error=$null}
try {
  $proof.before=Status
  foreach($key in @('preferences_open','preferences_dirty','mapping_open','workspaces_open')){
    if($proof.before.$key -ne $false){throw "Open/unknown draft: $key"}
  }
  if($proof.before.device_settings.open -ne $false -or $proof.before.cell_management.open -ne $false -or
     $proof.before.device_status.mode -ne 'cell' -or $proof.before.device_status.effect -ne 'none' -or
     @($proof.before.device_status.cells|Where-Object {$_ -ne 'white'}).Count){throw 'Scene changed since first boot; preserve newer user work'}
  foreach($state in @('white','red','off')) {
    $ids=@();for($i=0;$i -lt 80;++$i){if($original.cells[$i] -ceq $state){$ids+=($i+1)}}
    if(-not $ids.Count){continue}
    $s=Status
    if(@($ids|Where-Object {$s.device_status.cells[$_-1] -cne $state}).Count -eq 0){continue}
    $reply=& $ExecutablePath --led-cells ($ids -join ',') $state immediate|ConvertFrom-Json;Accepted $reply
    $until=[DateTime]::UtcNow.AddSeconds(10)
    do{Start-Sleep -Milliseconds 150;$s=Status;$different=@($ids|Where-Object {$s.device_status.cells[$_-1] -cne $state}).Count}while($different -and [DateTime]::UtcNow -lt $until)
    if($different){throw 'Cell readback unconfirmed; no mutation replay'}
  }
  if($original.effect -ne 'none'){
    $reply=& $ExecutablePath --led-effect $original.effect|ConvertFrom-Json;Accepted $reply
  }
  Start-Sleep -Seconds 3
  $proof.after=Status
  if(($proof.after.device_status.cells-join ',') -cne ($original.cells-join ',') -or
     $proof.after.device_status.effect -cne $original.effect){throw 'Final scene readback differs'}
  $proof.restored=$true
}catch{$proof.error=$_.Exception.Message;throw}
finally {
  $stream=[IO.File]::Open($ReceiptPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
  try{$bytes=[Text.Encoding]::UTF8.GetBytes(($proof|ConvertTo-Json -Depth 24));$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
}
$proof|Select-Object restored,error|ConvertTo-Json -Compress
