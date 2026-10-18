[CmdletBinding()]
param([ValidateRange(1,30)][int]$Seconds=10)
$ErrorActionPreference='Stop'
if($PSVersionTable.PSEdition -ne 'Desktop') {throw 'Run this read-only WinRT probe with Windows PowerShell (powershell.exe), not PowerShell Core.'}
# Microsoft BluetoothLEAdvertisementWatcher observes only our service. No GATT
# connection, pairing, adapter setting or application/SDK installation occurs.
# PowerShell cannot subscribe to WinRT events, and ServiceUuids is an opaque RCW.
# Use the installed .NET compiler plus Windows' existing WinMetadata projection;
# Add-Type -ReferencedAssemblies itself cannot load these .winmd files.
if(-not ('PakarBluetoothAdvertisementProbe' -as [type])){
  Add-Type -AssemblyName System.Runtime.WindowsRuntime
  Add-Type -AssemblyName System.Runtime
  Add-Type -AssemblyName System.Runtime.InteropServices.WindowsRuntime
  $taskParameters=New-Object System.CodeDom.Compiler.CompilerParameters
  $taskParameters.GenerateInMemory=$true
  $taskReferences=@('System.dll','System.Core.dll',
    (Join-Path $env:windir 'System32/WinMetadata/Windows.Devices.winmd'),
    (Join-Path $env:windir 'System32/WinMetadata/Windows.Foundation.winmd'))+
    @([AppDomain]::CurrentDomain.GetAssemblies() | Where-Object {$_.GetName().Name -like 'System.Runtime*'} | ForEach-Object Location)
  foreach($taskReference in $taskReferences){[void]$taskParameters.ReferencedAssemblies.Add($taskReference)}
  $taskProvider=New-Object Microsoft.CSharp.CSharpCodeProvider
  try {
    $taskCompiled=$taskProvider.CompileAssemblyFromSource($taskParameters,@'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Foundation;
public static class PakarBluetoothAdvertisementProbe {
    public sealed class Candidate {
        public string Address, Name, Service;
        public short Rssi;
        public bool VerifiedBoard = false;
        public string Meaning = "Advertisement only; INFO/status identity and GATT RPC unverified";
    }
    public sealed class Result { public string Status; public int Seconds; public Candidate[] Candidates; }
    public static Result Run(int seconds) {
        if(seconds < 1 || seconds > 30) throw new ArgumentOutOfRangeException("seconds");
        var service = new Guid("a4b68d10-5c3f-4c5a-9f12-50616b617243");
        var watcher = new BluetoothLEAdvertisementWatcher();
        watcher.AdvertisementFilter.Advertisement.ServiceUuids.Add(service);
        var rows = new Dictionary<ulong,Candidate>();
        TypedEventHandler<BluetoothLEAdvertisementWatcher,BluetoothLEAdvertisementReceivedEventArgs> handler = (sender,args) => {
            if(!args.Advertisement.ServiceUuids.Contains(service)) return;
            var row = new Candidate { Address=args.BluetoothAddress.ToString("X12"), Name=args.Advertisement.LocalName,
                Rssi=args.RawSignalStrengthInDBm, Service=service.ToString() };
            lock(rows) rows[args.BluetoothAddress]=row;
        };
        watcher.Received += handler;
        try {
            watcher.Start(); var clock=Stopwatch.StartNew();
            while(clock.Elapsed.TotalSeconds < seconds) {
                if(watcher.Status == BluetoothLEAdvertisementWatcherStatus.Aborted)
                    throw new InvalidOperationException("Windows aborted Bluetooth discovery; adapter/policy/session access is not verified.");
                Thread.Sleep(100);
            }
            lock(rows) { var candidates=new Candidate[rows.Count];rows.Values.CopyTo(candidates,0);
                return new Result { Status=watcher.Status.ToString(), Seconds=seconds, Candidates=candidates }; }
        } finally { watcher.Stop(); watcher.Received -= handler; }
    }
}
'@)
    if($taskCompiled.Errors.HasErrors){throw ('Installed WinRT projection compilation failed: '+($taskCompiled.Errors | ForEach-Object ToString | Out-String))}
    [void]$taskCompiled.CompiledAssembly
  } finally {$taskProvider.Dispose();$taskParameters.TempFiles.Delete()}
}
[PakarBluetoothAdvertisementProbe]::Run($Seconds) | ConvertTo-Json -Depth 4
