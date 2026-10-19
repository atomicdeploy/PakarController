$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'android-viewport.ps1')

$taskLandscape='  Viewport INTERNAL: displayId=0, uniqueId=local:463, orientation=1, logicalFrame=[0, 0, 2960, 1848], physicalFrame=[0, 0, 2960, 1848], deviceSize=[1848, 2960], isActive=[1]'
$taskPortrait='  Viewport INTERNAL: displayId=0, orientation=0, logicalFrame=[0, 0, 1848, 2960], isActive=[1]'
function Assert-ViewportRejected([string]$InputDump) {
    $taskRejected=$false
    try {Get-AndroidSwipeViewport -InputDump $InputDump | Out-Null} catch {$taskRejected=$true}
    if(-not $taskRejected) {throw "Unsafe viewport accepted: $InputDump"}
}
$taskParsed=Get-AndroidSwipeViewport -InputDump "Physical size: 1848x2960`n$taskLandscape"
if($taskParsed.Width -ne 2960 -or $taskParsed.Height -ne 1848 -or $taskParsed.Orientation -ne 1) {
    throw 'Landscape input viewport was confused with natural portrait dimensions.'
}
$taskParsed=Get-AndroidSwipeViewport -InputDump "$taskLandscape`n$taskLandscape"
if($taskParsed.Width -ne 2960) {throw 'Identical viewport records were not deduplicated.'}
$taskParsed=Get-AndroidSwipeViewport -InputDump "$taskPortrait`n$($taskLandscape.Replace('isActive=[1]','isActive=[0]'))"
if($taskParsed.Height -ne 2960) {throw 'Inactive viewport affected the selected geometry.'}
Assert-ViewportRejected "$taskLandscape`n$taskPortrait"
Assert-ViewportRejected "$taskLandscape`n$($taskLandscape.Replace('displayId=0','displayId=1'))"
Assert-ViewportRejected ($taskLandscape.Replace('displayId=0','displayId=1'))
Assert-ViewportRejected ($taskLandscape.Replace('isActive=[1]','isActive=[0]'))
Assert-ViewportRejected ($taskLandscape.Replace('isActive=[1]','isActive=[true]'))
Assert-ViewportRejected ($taskLandscape.Replace('orientation=1','orientation=4'))
Assert-ViewportRejected ($taskLandscape.Replace('logicalFrame=[0, 0, 2960, 1848]','logicalFrame=[0, 0, 9000, 1848]'))
Assert-ViewportRejected ($taskLandscape.Replace('logicalFrame=[0, 0, 2960, 1848]','logicalFrame=[0, 0, 2960, 0]'))
Assert-ViewportRejected ($taskLandscape.Replace('logicalFrame=[0, 0, 2960, 1848]','logicalFrame=[-1, 0, 2960, 1848]'))
Assert-ViewportRejected 'Physical size: 1848x2960'
'PASS: 13 viewport fixtures (landscape, duplicate, inactive and unsafe geometry).'
