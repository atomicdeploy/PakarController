[CmdletBinding()]
param([Parameter(Mandatory)][ValidatePattern('^(COM[1-9][0-9]*|/dev/[A-Za-z0-9._/-]+)$')][string]$Port,
  [string]$Execute,[switch]$Tui,[switch]$Plan,[ValidateRange(100,15000)][int]$DeadlineMs=8000)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'serial-wire.ps1')
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern network-cloud | Out-Null
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern api-contract -Gate | Out-Null
if($Plan){[pscustomobject]@{Port=$Port;Baud=115200;Transport='COBS shell channel';RawEscape=$false;AutomaticCommandRetry=$false};return}
if(-not $Execute -and -not $Tui -and ([Console]::IsInputRedirected -or [Console]::IsOutputRedirected)){throw 'Interactive shell requires a terminal; use -Execute for a bounded command'}
if($Tui -and ([Console]::IsInputRedirected -or [Console]::IsOutputRedirected)){throw 'TUI requires a terminal'}
$taskSerial=[IO.Ports.SerialPort]::new($Port,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
$taskSerial.DtrEnable=$false;$taskSerial.RtsEnable=$false;$taskSerial.ReadTimeout=100;$taskSerial.WriteTimeout=$DeadlineMs
$script:taskFrameSequence=10;$taskInputSequence=0;$taskAck=65535;$taskToken=[byte[]]::new(4);$taskOpened=$false
$taskConsoleModeChanged=$false;$taskPriorControlC=$false
function Request([byte[]]$Payload,[byte]$Response=0xb9){
  $script:taskFrameSequence=($script:taskFrameSequence+1) -band 255
  return ,(Send-PakarNetworkRequest $taskSerial 0x60 $script:taskFrameSequence $Payload $Response $DeadlineMs)
}
function InputBytes([byte[]]$Bytes){
  for($taskAt=0;$taskAt -lt $Bytes.Length;$taskAt+=41){
    if($script:taskInputSequence -ge 65535){throw 'Input sequence exhausted; reopen shell before continuing'}
    $taskCount=[Math]::Min(41,$Bytes.Length-$taskAt);$taskPacket=[byte[]]::new(7+$taskCount);$taskPacket[0]=1
    [Array]::Copy($taskToken,0,$taskPacket,1,4);[Array]::Copy([BitConverter]::GetBytes([uint16]$script:taskInputSequence),0,$taskPacket,5,2)
    [Array]::Copy($Bytes,$taskAt,$taskPacket,7,$taskCount)
    Request $taskPacket 0x80 | Out-Null # Exactly once; uncertain input is NEVER replayed.
    $script:taskInputSequence++
    [Array]::Clear($taskPacket,0,$taskPacket.Length)
  }
}
function Poll {
  $taskPacket=[byte[]]::new(7);$taskPacket[0]=3;[Array]::Copy($taskToken,0,$taskPacket,1,4)
  [Array]::Copy([BitConverter]::GetBytes([uint16]$script:taskAck),0,$taskPacket,5,2)
  [byte[]]$taskReply=Request $taskPacket
  if($taskReply[4] -lt 10 -or $taskReply[5] -ne 3 -or [BitConverter]::ToUInt32($taskReply,6) -ne [BitConverter]::ToUInt32($taskToken,0)){throw 'Shell output owner mismatch'}
  $taskCount=$taskReply[4]-10;$taskSequence=[BitConverter]::ToUInt16($taskReply,13)
  if($taskCount){
    if($taskSequence -ne (($script:taskAck+1) -band 65535)){throw 'Shell output gap; reopen without replaying commands'}
    [Console]::Write([Text.Encoding]::UTF8.GetString($taskReply,15,$taskCount));$script:taskAck=$taskSequence
  }
  if($taskReply[12] -band 2){throw 'Shell output bound/allocation failed; command outcome may be uncertain'}
  return [pscustomobject]@{Bytes=$taskCount;Closed=($taskReply[12] -band 1) -ne 0}
}
try {
  if(-not $Execute){$taskPriorControlC=[Console]::TreatControlCAsInput;[Console]::TreatControlCAsInput=$true;$taskConsoleModeChanged=$true}
  $taskSerial.Open()
  [byte[]]$taskHello=Send-PakarNetworkRequest $taskSerial 0x01 1 @() 0x81 $DeadlineMs
  Assert-PakarNetworkEspHello $taskHello
  if($taskHello[4] -lt 25){throw 'Peer has not advertised a boot-bound shell handshake; update through the approved firmware path first'}
  $taskNonce=[byte[]]::new(4);$taskRng=[Security.Cryptography.RandomNumberGenerator]::Create();try{$taskRng.GetBytes($taskNonce)}finally{$taskRng.Dispose()}
  $taskOpen=[byte[]]::new(21);[Array]::Copy($taskHello,14,$taskOpen,1,16);[Array]::Copy($taskNonce,0,$taskOpen,17,4)
  [byte[]]$taskAdmission=Request $taskOpen
  if($taskAdmission[4] -ne 13 -or $taskAdmission[5] -ne 0 -or [BitConverter]::ToUInt32($taskAdmission,14) -ne [BitConverter]::ToUInt32($taskNonce,0)){throw 'Shell admission mismatch; no terminal input sent'}
  [Array]::Copy($taskAdmission,6,$taskToken,0,4);$taskOpened=$true
  $script:taskInputSequence=0;$script:taskAck=65535
  do{$taskResult=Poll}while($taskResult.Bytes)
  if($Execute -or $Tui){InputBytes ([Text.Encoding]::UTF8.GetBytes($(if($Tui){"tui`r"}else{$Execute+"`r"})))}
  $taskQuiet=0
  while($true){
    $taskResult=Poll
    if($taskResult.Closed -and -not $taskResult.Bytes){break}
    if($Execute){if($taskResult.Bytes){$taskQuiet=0}else{$taskQuiet++;if($taskQuiet -ge 3){break}}}
    elseif([Console]::KeyAvailable){
      $taskKey=[Console]::ReadKey($true)
      $taskBytes=switch($taskKey.Key){
        UpArrow{([string][char]27)+'[A'} DownArrow{([string][char]27)+'[B'} RightArrow{([string][char]27)+'[C'} LeftArrow{([string][char]27)+'[D'}
        Home{([string][char]27)+'[H'} End{([string][char]27)+'[F'} Delete{([string][char]27)+'[3~'} Backspace{[string][char]127} Enter{"`r"} Escape{[string][char]27}
        default{[string]$taskKey.KeyChar}
      }
      if($taskBytes){InputBytes ([Text.Encoding]::UTF8.GetBytes($taskBytes))}
    }
    if(-not $taskResult.Bytes){Start-Sleep -Milliseconds 20}
  }
}finally{
  if($taskOpened -and $taskSerial.IsOpen){try{Request ([byte[]](@(2)+$taskToken)) 0x80 | Out-Null}catch{Write-Warning 'Shell close was not acknowledged; no command was replayed'}}
  $taskSerial.Dispose();[Array]::Clear($taskToken,0,$taskToken.Length)
  if($taskConsoleModeChanged){[Console]::TreatControlCAsInput=$taskPriorControlC}
  if(-not [Console]::IsOutputRedirected){[Console]::Write(([string][char]27)+'[0m')}
}
