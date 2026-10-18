[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ExecutablePath,
    [Parameter(Mandatory)][string]$ExpectedHash,
    [Parameter(Mandatory)][int]$ExpectedProcessId,
    [Parameter(Mandatory)][string]$ReceiptPath,
    [ValidateRange(1,12)][int]$Count=6,
    [ValidateRange(0,2500)][int]$DelayMilliseconds=0,
    [switch]$Run
)
# Talks ONLY to the identity-checked running Rust app. Never opens COM itself.
# IPC acceptance is not a board ACK. Each changed value must return through USB.
$ErrorActionPreference='Stop'
if(-not [IO.Path]::IsPathRooted($ReceiptPath) -or (Test-Path -LiteralPath $ReceiptPath)){throw 'Use a new absolute receipt path'}
$owner=Get-CimInstance Win32_Process -Filter "ProcessId=$ExpectedProcessId"
$session=(Get-Process -Id $PID).SessionId
if(-not $owner -or $owner.SessionId -ne $session -or $session -eq 0 -or $owner.ExecutablePath -ine $ExecutablePath -or
   (Get-FileHash -LiteralPath $ExecutablePath).Hash -ine $ExpectedHash){throw 'Expected interactive owner/hash changed'}
function Rpc($Command) {
    $pipe=[IO.Pipes.NamedPipeClientStream]::new('.',"PakarController.Control.124b14fa4bf837fc.$session",[IO.Pipes.PipeDirection]::InOut,[IO.Pipes.PipeOptions]::Asynchronous)
    try {
        $pipe.Connect(1500);$pipe.ReadMode=[IO.Pipes.PipeTransmissionMode]::Message
        $request=[Text.Encoding]::UTF8.GetBytes(($Command|ConvertTo-Json -Compress))
        $pipe.Write($request,0,$request.Length)
        $reply=[IO.MemoryStream]::new()
        try {
            $buffer=New-Object byte[] 65536
            do {
                $pending=$pipe.BeginRead($buffer,0,$buffer.Length,$null,$null)
                try{if(-not $pending.AsyncWaitHandle.WaitOne(1500)){throw 'IPC response deadline'};$n=$pipe.EndRead($pending)}finally{$pending.AsyncWaitHandle.Dispose()}
                if($n -eq 0){throw 'IPC closed before reply'};$reply.Write($buffer,0,$n)
            }while(-not $pipe.IsMessageComplete)
            $value=[Text.Encoding]::UTF8.GetString($reply.ToArray())|ConvertFrom-Json
        }finally{$reply.Dispose()}
        if($value.process_id -ne $ExpectedProcessId){throw 'Different IPC owner'}
        return $value
    }finally{$pipe.Dispose()}
}
function Status {Rpc @{command='status'}}
function Peer($s) {
    if($s.connection.method -ne 'USB' -or -not $s.connection.online -or -not $s.serial_live -or $s.websocket_live -or
       $s.serial_port -ne $script:port -or $s.device_status.board_id -ne $script:board -or $s.device_status.boot_id -ne $script:boot){throw 'USB/peer/session authority changed; no replay'}
}
$before=Status
$proof=[ordered]@{executable=$ExecutablePath;sha256=$ExpectedHash;process_id=$ExpectedProcessId;session=$session;before=$before;requests=@();after=$null;error=$null;restored=$false}
try {
    if($Run){
        foreach($key in @('preferences_open','preferences_dirty','mapping_open','workspaces_open')){if($before.$key -ne $false){throw 'Open or unknown draft'}}
        if($before.cell_management.open -ne $false -or $before.device_settings.open -ne $false -or $before.device_settings.write_pending -ne $false -or $before.device_settings.maintenance_pending -ne $false){throw 'Open draft or maintenance'}
        $script:port=$before.serial_port;$script:board=$before.device_status.board_id;$script:boot=$before.device_status.boot_id
        Peer $before
        if($before.device_status.mode -ne 'cell' -or $before.device_status.effect -ne 'none' -or $before.mapping.count -ne 80 -or $before.usb_commands_pending -ne 0){throw 'Requires quiet cell mode, no animation, complete mapping and empty queue'}
        $original=$before.device_status.cells[0]
        if($original -notin @('white','red','off')){throw 'Unknown original state'}
        $states=@();for($i=0;$i -lt $Count;$i++){$states+=if($i%2 -eq 0){if($original -eq 'red'){'white'}else{'red'}}else{$original}}
        if($states[-1] -ne $original){$states+=$original}
        foreach($state in $states){
            if($DelayMilliseconds){Start-Sleep -Milliseconds $DelayMilliseconds}
            Peer (Status)
            $clock=[Diagnostics.Stopwatch]::StartNew()
            $ack=Rpc @{command='led_cells';ids='1';state=$state;effect='immediate'}
            $acceptedMs=$clock.Elapsed.TotalMilliseconds
            if($ack.accepted -ne $true){throw 'App rejected command'}
            do {
                Start-Sleep -Milliseconds 15;$s=Status;Peer $s
                if($clock.Elapsed.TotalSeconds -gt 20){throw 'No board-confirmed change within 20s; no mutation replayed'}
            }while($s.device_status.cells[0] -ne $state -or $s.usb_commands_pending -ne 0)
            $proof.requests+=@{state=$state;ipc_accept_ms=$acceptedMs;confirmed_ms=$clock.Elapsed.TotalMilliseconds;message=$s.message;usb_command=$s.usb_command;observed=$s.device_status.cells[0]}
        }
        $proof.restored=$true
        $null=Rpc @{command='find_cell';query=$before.cell_search}
    }
    $proof.after=Status
}catch{$proof.error=$_.Exception.Message;throw}
finally {
    # Preserve incomplete evidence too; never claim restoration on a lost peer.
    $json=$proof|ConvertTo-Json -Depth 30
    $stream=[IO.File]::Open($ReceiptPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
    try{$bytes=[Text.Encoding]::UTF8.GetBytes($json);$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
}
