# Actual PU850 Tool/build-all.ps1 selection, preflight and staged native runner.
[CmdletBinding()]
param([string]$Only='All',[switch]$Run,[switch]$Release)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'script-runtime.ps1')
$taskSelected=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$taskRequested=@($Only.Split(',') | ForEach-Object {$_.Trim()} | Where-Object {$_})
if(-not $taskRequested.Count -or @($taskRequested | Where-Object {$_ -notin @('All','Firmware','Desktop','Mobile')}).Count){throw 'Only must be a comma-separated subset of All,Firmware,Desktop,Mobile'}
foreach($taskName in $taskRequested){if($taskName -eq 'All'){foreach($taskItem in @('Firmware','Desktop','Mobile')){[void]$taskSelected.Add($taskItem)}}else{[void]$taskSelected.Add($taskName)}}
Assert-PakarExecutable -Name 'node' -InstallHint 'Use the existing Node.js20.19+ runtime.'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskStarted=[Diagnostics.Stopwatch]::StartNew()
foreach($taskName in @('Firmware','Desktop','Mobile')){
    if(-not $taskSelected.Contains($taskName)){continue}
    Write-PakarStep "$taskName · $(if($Run){'execute'}else{'plan only'})"
    $taskArguments=switch($taskName){
        Firmware {@{Target='esp32s3'}}
        Desktop {@{Mode=$(if($Release){'Release'}else{'Check'})}}
        Mobile {if($Release){@{Mode='ApkPreview';BuildOnly=$true}}else{@{Mode='Analyze'}}}
    }
    if(-not $Run){$taskArguments.Plan=$true}
    $taskScript='build-'+$taskName.ToLowerInvariant()+'.ps1'
    if($Run){& (Join-Path $PSScriptRoot $taskScript) @taskArguments}
    else{
        # Flush each heterogeneous plan before the next stage: PowerShell's
        # deferred first-object table otherwise misaligns/collapses later plans.
        $taskPlans=@(& (Join-Path $PSScriptRoot $taskScript) @taskArguments)
        foreach($taskPlan in $taskPlans){$taskPlan | Format-List | Out-Host}
    }
}
Write-Host ('Selected components finished in {0:n2}s; no deployment was requested.' -f $taskStarted.Elapsed.TotalSeconds)
