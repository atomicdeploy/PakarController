[CmdletBinding()]
param([ValidateRange(1,30)][int]$Seconds=10,[switch]$ReadInfo,[string]$ExpectedBoardId)
$ErrorActionPreference='Stop'
if($PSVersionTable.PSEdition -ne 'Desktop') {throw 'Run this read-only WinRT probe with Windows PowerShell (powershell.exe), not PowerShell Core.'}
if($ReadInfo -and ($ExpectedBoardId -notmatch '^[a-zA-Z0-9][a-zA-Z0-9_.:-]{0,79}$')){throw '-ReadInfo requires an explicit -ExpectedBoardId.'}
# Default: advertisement only. -ReadInfo additionally opens only our matching
# GATT service and reads its INFO attribute. No RX/CCCD write, pairing request,
# access request/prompt, adapter setting or application/SDK installation occurs.
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
    (Join-Path $env:windir 'System32/WinMetadata/Windows.Storage.winmd'),
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
using System.Text;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Foundation;
using Windows.Storage.Streams;
public static class PakarBluetoothAdvertisementProbe {
    public sealed class Candidate {
        public string Address, Name, Service, InfoJson, InfoError;
        public short Rssi;
        public bool VerifiedBoard = false;
        public bool InfoTeardownDeferred = false;
        public string Meaning = "Advertisement only; INFO/status identity and GATT RPC unverified";
    }
    public sealed class Result { public string Status; public int Seconds; public Candidate[] Candidates; }
    // IAsyncInfo.Close is legal only after completion. Timing out OUR wait is
    // not completion/cancellation of Windows' underlying Bluetooth operation.
    // Keep owners and late results alive until its terminal callback, rather
    // than disposing a device/service underneath an in-progress native call.
    private sealed class Lease {
        private static readonly List<Lease> Pending = new List<Lease>();
        public BluetoothLEDevice Device;
        public GattDeviceServicesResult Services;
        private object operation;
        private bool deferred;
        private int disposed;
        private static void DisposeResult(object value) {
            var services=value as GattDeviceServicesResult;
            if(services!=null)foreach(var service in services.Services)try {service.Dispose();} catch { }
            var device=value as BluetoothLEDevice;if(device!=null)try {device.Dispose();} catch { }
        }
        public void Defer<T>(IAsyncOperation<T> pending) {
            deferred=true;operation=pending;
            lock(Pending)Pending.Add(this);
            pending.Completed=(finished,status)=>{
                try { if(status==AsyncStatus.Completed)DisposeResult(finished.GetResults()); }
                catch { /* A terminal failed read has no usable late result. */ }
                finally {
                    try { finished.Close(); } catch { }
                    try {Cleanup();} catch { } // Never escape a native completion callback.
                    operation=null;lock(Pending)Pending.Remove(this);
                }
            };
        }
        private void Cleanup() {
            if(Interlocked.Exchange(ref disposed,1)!=0)return;
            try { DisposeResult(Services); } finally { DisposeResult(Device); }
        }
        public void Dispose() { if(!deferred)Cleanup(); }
    }
    private static T Await<T>(IAsyncOperation<T> operation, Stopwatch budget, Lease lease, string stage, Candidate candidate) {
        var timer=Stopwatch.StartNew();
        bool deferred=false;
        try {
            while(operation.Status == AsyncStatus.Started) {
                if(timer.ElapsedMilliseconds >= 15000 || budget.ElapsedMilliseconds >= 30000) {
                    candidate.InfoTeardownDeferred=true;lease.Defer(operation);deferred=true;
                    throw new TimeoutException(stage+": bounded wait expired; native operation retained until completion; no retry");
                }
                Thread.Sleep(20);
            }
            return operation.GetResults();
        } finally { if(!deferred && operation.Status!=AsyncStatus.Started)operation.Close(); }
    }
    public static void ReadInfo(Candidate candidate, Stopwatch budget) {
        var lease=new Lease();string stage="device-reference";
        try {
            if(budget.ElapsedMilliseconds >= 30000)throw new TimeoutException("Total probe budget exhausted");
            var device=lease.Device=Await(BluetoothLEDevice.FromBluetoothAddressAsync(Convert.ToUInt64(candidate.Address,16)),budget,lease,stage,candidate);
            if(device==null)throw new InvalidOperationException("Advertised device no longer available");
            // Never call RequestAccessAsync/PairAsync or FromIdAsync (UI consent).
            if(device.DeviceAccessInformation.CurrentStatus != DeviceAccessStatus.Allowed)
                throw new InvalidOperationException("Existing device access is not Allowed; no permission prompt requested");
            stage="service-discovery";
            if(budget.ElapsedMilliseconds >= 30000)throw new TimeoutException("Total probe budget exhausted");
            var services=lease.Services=Await(device.GetGattServicesForUuidAsync(new Guid(candidate.Service),BluetoothCacheMode.Uncached),budget,lease,stage,candidate);
            if(services.Status != GattCommunicationStatus.Success || services.Services.Count!=1)
                throw new InvalidOperationException("Expected one service; GATT status="+services.Status);
            stage="characteristic-discovery";
            if(budget.ElapsedMilliseconds >= 30000)throw new TimeoutException("Total probe budget exhausted");
            var characteristics=Await(services.Services[0].GetCharacteristicsForUuidAsync(
                new Guid("a4b68d13-5c3f-4c5a-9f12-50616b617243"),BluetoothCacheMode.Uncached),budget,lease,stage,candidate);
            if(characteristics.Status != GattCommunicationStatus.Success || characteristics.Characteristics.Count!=1)
                throw new InvalidOperationException("Expected one INFO characteristic; status="+characteristics.Status);
            var info=characteristics.Characteristics[0];
            if((info.CharacteristicProperties & GattCharacteristicProperties.Read)==0 || info.ProtectionLevel!=GattProtectionLevel.Plain)
                throw new InvalidOperationException("INFO does not allow a plain read; no security upgrade attempted");
            stage="info-read";
            if(budget.ElapsedMilliseconds >= 30000)throw new TimeoutException("Total probe budget exhausted");
            var read=Await(info.ReadValueAsync(BluetoothCacheMode.Uncached),budget,lease,stage,candidate);
            if(read.Status != GattCommunicationStatus.Success || read.Value==null || read.Value.Length==0 || read.Value.Length>512)
                throw new InvalidOperationException("INFO read failed or invalid bounded length; status="+read.Status);
            var bytes=new byte[(int)read.Value.Length];
            using(var reader=DataReader.FromBuffer(read.Value))reader.ReadBytes(bytes);
            candidate.InfoJson=new UTF8Encoding(false,true).GetString(bytes);
        } catch(Exception error) { candidate.InfoError=stage+": "+error.Message; }
        finally { lease.Dispose(); }
    }
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
$taskInfoClock=[Diagnostics.Stopwatch]::StartNew()
$taskResult=[PakarBluetoothAdvertisementProbe]::Run($Seconds)
if($ReadInfo){
  $taskInfoCount=0
  foreach($taskCandidate in $taskResult.Candidates){
    if($taskInfoCount++ -ge 4 -or $taskInfoClock.ElapsedMilliseconds -ge 30000){$taskCandidate.InfoError='Bounded INFO candidate/30-second total probe budget exhausted';continue}
    [PakarBluetoothAdvertisementProbe]::ReadInfo($taskCandidate,$taskInfoClock)
    if($taskCandidate.InfoTeardownDeferred){break} # Never queue another native operation after uncertainty.
    if($taskCandidate.InfoJson){
      try {
        $taskInfo=$taskCandidate.InfoJson | ConvertFrom-Json -ErrorAction Stop
        if($taskInfo.product -cne 'PakarController' -or $taskInfo.board_id -cne $ExpectedBoardId -or $taskInfo.boot_id -cnotmatch '^[0-9a-f]{32}$'){throw 'INFO product/board/boot identity mismatch'}
        $taskCandidate.VerifiedBoard=$true
        $taskCandidate.Meaning='Uncached INFO product/expected board/boot verified; RPC, actuation and Android runtime unverified'
      } catch {$taskCandidate.InfoError=$_.Exception.Message}
    }
  }
}
$taskResult | ConvertTo-Json -Depth 4
