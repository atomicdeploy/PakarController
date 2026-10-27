[CmdletBinding(SupportsShouldProcess)]
param(
  [Parameter(Mandatory)][string]$ExecutablePath,
  [Parameter(Mandatory)][string]$CandidatePath,
  [Parameter(Mandatory)][string]$RollbackPath,
  [Parameter(Mandatory)][string]$SettingsPath,
  [Parameter(Mandatory)][string]$SettingsBackupPath,
  [Parameter(Mandatory)][string]$ReceiptPath,
  [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedOldHash,
  [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedNewHash,
  [Parameter(Mandatory)][ValidateRange(1,2147483647)][int]$ExpectedProcessId,
  [Parameter(Mandatory)][string]$LaunchTask,
  [switch]$InitializeClientManagement,
  [string]$ClientManagementBase='https://office.digitalogic.ir/pakar',
  [string]$EnrollmentTokenPath,
  [ValidateRange(5,45)][int]$DeadlineSeconds=20
)
# Adapt actual Replace-AsusRecoveryDesktop and Run-DesktopCandidate: same-session
# owner/draft checks, graceful IPC, atomic NTFS replacement and preserved profile.
# No kill, settings rewrite, USB fallback, board writes or automatic rollback.
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
. (Join-Path $PSScriptRoot 'package-probe-windows.ps1')
if($InitializeClientManagement){
  $taskManagementUri=$null
  if(-not [Uri]::TryCreate($ClientManagementBase,[UriKind]::Absolute,[ref]$taskManagementUri) -or $taskManagementUri.Scheme -ne 'https' -or $taskManagementUri.UserInfo -or $taskManagementUri.Query -or $taskManagementUri.Fragment){throw 'Use an explicit HTTPS management base without credentials'}
  if(-not [IO.Path]::IsPathRooted($EnrollmentTokenPath) -or -not(Test-Path -LiteralPath $EnrollmentTokenPath -PathType Leaf)){throw 'An explicit existing private one-use enrollment token file is required'}
}
$OutputEncoding=[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
foreach($value in @($ExecutablePath,$CandidatePath,$RollbackPath,$SettingsPath,$SettingsBackupPath,$ReceiptPath)){
  if(-not [IO.Path]::IsPathRooted($value)){throw 'All paths must be explicit and absolute'}
}
$dir=[IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($ExecutablePath))
foreach($value in @($CandidatePath,$RollbackPath,$SettingsBackupPath,$ReceiptPath)){
  if([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($value)) -ine $dir){throw 'Candidate and recoverable outputs must be siblings of the executable'}
}
if(@($ExecutablePath,$CandidatePath,$RollbackPath,$SettingsPath,$SettingsBackupPath,$ReceiptPath | Sort-Object -Unique).Count -ne 6){throw 'Paths must be distinct'}
foreach($path in @($RollbackPath,$SettingsBackupPath,$ReceiptPath)){if(Test-Path -LiteralPath $path){throw 'Preserve existing rollback, backup and receipt; never overwrite'}}
function Hash([string]$Path){return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function AssertHash([string]$Path,[string]$Expected){if((Hash $Path) -ine $Expected){throw "File changed: $Path"}}
$session=(Get-Process -Id $PID).SessionId
if($session -eq 0){throw 'Use the existing interactive user session, never SSH session zero'}
function OwnerStatus {
  $owner=Get-CimInstance Win32_Process -Filter "ProcessId=$ExpectedProcessId"
  if(-not $owner -or $owner.ExecutablePath -ine $ExecutablePath -or $owner.SessionId -ne $session){throw 'Owner PID/path/session changed'}
  AssertHash $ExecutablePath $ExpectedOldHash
  # The validated candidate can read larger snapshots than an older installed
  # CLI. Still require the exact running owner/session and all draft guards.
  AssertHash $CandidatePath $ExpectedNewHash
  $status=Invoke-PackageProbe $CandidatePath '--app-status' $dir | ConvertFrom-Json
  if($status.process_id -ne $ExpectedProcessId -or $status.ready -ne $true -or $status.single_instance -ne $true){throw 'Expected ready single-instance IPC owner required'}
  foreach($key in @('preferences_open','preferences_dirty','mapping_open','workspaces_open')){if($status.$key -ne $false){throw "Open or unknown draft: $key"}}
  if($status.device_settings.open -ne $false -or $status.device_settings.write_pending -ne $false -or
     $status.device_settings.maintenance_pending -ne $false -or $status.cell_management.open -ne $false -or
     $status.screenshot.pending -ne $false -or $null -ne $status.connection.releasing){throw 'Editor, pending operation or capture is open/unknown'}
  return $status
}
$task=Get-ScheduledTask -TaskName $LaunchTask
if([string]$task.Principal.LogonType -ne 'Interactive' -or [string]$task.Principal.RunLevel -ne 'Limited' -or
   @($task.Actions).Count -ne 1 -or $task.Actions[0].Execute -ine $ExecutablePath -or
   $task.Actions[0].Arguments -or $task.Actions[0].WorkingDirectory -ine $dir){throw 'Existing interactive production task changed'}
AssertHash $CandidatePath $ExpectedNewHash
if((Get-Item -LiteralPath $CandidatePath).VersionInfo.ProductName -ne 'PakarController'){throw 'Candidate product identity mismatch'}
$proof=[ordered]@{old_process=$ExpectedProcessId;session=$session;old_sha256=$ExpectedOldHash;new_sha256=$ExpectedNewHash;
  settings_sha256=Hash $SettingsPath;before=OwnerStatus;quit_attempted=$false;replaced=$false;new_process=$null;error=$null}
if(-not $PSCmdlet.ShouldProcess($ExecutablePath,'Gracefully replace this verified owner, preserving settings and rollback')){return}
try {
  [IO.File]::Copy($SettingsPath,$SettingsBackupPath,$false)
  AssertHash $SettingsBackupPath $proof.settings_sha256
  $null=OwnerStatus;AssertHash $CandidatePath $ExpectedNewHash
  $proof.quit_attempted=$true
  $reply=Invoke-PackageProbe $ExecutablePath '--quit' $dir | ConvertFrom-Json
  if($reply.accepted -ne $true -or $reply.process_id -ne $ExpectedProcessId){throw 'Graceful quit unconfirmed; no retry or forced shutdown'}
  $until=[DateTime]::UtcNow.AddSeconds($DeadlineSeconds)
  do{Start-Sleep -Milliseconds 150;$owner=Get-CimInstance Win32_Process -Filter "ProcessId=$ExpectedProcessId"}while($owner -and [DateTime]::UtcNow -lt $until)
  if($owner -or @(Get-CimInstance Win32_Process -Filter "Name LIKE 'Pakar%'").Count){throw 'An owner remains; no replacement attempted'}
  AssertHash $ExecutablePath $ExpectedOldHash;AssertHash $CandidatePath $ExpectedNewHash
  [IO.File]::Replace($CandidatePath,$ExecutablePath,$RollbackPath,$false)
  $proof.replaced=$true
  AssertHash $ExecutablePath $ExpectedNewHash;AssertHash $RollbackPath $ExpectedOldHash
  if($InitializeClientManagement){
    $taskSavedProfile=[Environment]::GetEnvironmentVariable('PAKAR_CONFIG_FILE','Process')
    try{
      $env:PAKAR_CONFIG_FILE=$SettingsPath
      & $ExecutablePath --client-management-config $ClientManagementBase --telemetry on --remote-commands on --remote-cleanup on
      if($LASTEXITCODE){throw 'Client management configuration failed; no enrollment was attempted'}
      & $ExecutablePath --client-update-init
      if($LASTEXITCODE){throw 'Managed installation initialization failed; no enrollment was attempted'}
      $taskEnrollment=[IO.File]::ReadAllText($EnrollmentTokenPath).Trim()
      if($taskEnrollment -notmatch '^[A-Za-z0-9_-]{43}$'){throw 'Invalid one-use token; enrollment not attempted'}
      $taskEnrollment | & $ExecutablePath --client-enroll
      $taskEnrollment=$null
      if($LASTEXITCODE){throw 'Enrollment outcome uncertain; not replayed; request a new token if needed'}
      $proof.client_management_initialized=$true
    }finally{[Environment]::SetEnvironmentVariable('PAKAR_CONFIG_FILE',$taskSavedProfile,'Process')}
  }
  Start-ScheduledTask -TaskName $LaunchTask
  $until=[DateTime]::UtcNow.AddSeconds($DeadlineSeconds)
  do{Start-Sleep -Milliseconds 200;$owners=@(Get-CimInstance Win32_Process -Filter "Name LIKE 'Pakar%'" | Where-Object {$_.ExecutablePath -ieq $ExecutablePath -and $_.SessionId -eq $session})}while($owners.Count -ne 1 -and [DateTime]::UtcNow -lt $until)
  if($owners.Count -ne 1){throw 'New production owner not observed; preserve evidence, no automatic retry'}
  $proof.new_process=$owners[0].ProcessId
  $until=[DateTime]::UtcNow.AddSeconds($DeadlineSeconds)
  $status=$null
  do {
    Start-Sleep -Milliseconds 200
    try{$status=Invoke-PackageProbe $ExecutablePath '--app-status' $dir | ConvertFrom-Json}catch{$proof.startup_observation=$_.Exception.Message}
  }while(($null -eq $status -or $status.ready -ne $true) -and [DateTime]::UtcNow -lt $until)
  if($status.process_id -ne $proof.new_process -or $status.ready -ne $true){throw 'New ready IPC identity not confirmed'}
  $proof.after=$status
}catch{$proof.error=$_.Exception.Message;throw}
finally {
  $stream=[IO.File]::Open($ReceiptPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
  try{$bytes=[Text.Encoding]::UTF8.GetBytes(($proof|ConvertTo-Json -Depth 24));$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
}
$proof | Select-Object old_process,new_process,replaced,error | ConvertTo-Json -Compress
