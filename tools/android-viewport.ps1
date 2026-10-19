# Pure parsing only: querying or sending input remains the caller's responsibility.
function Get-AndroidSwipeViewport {
    param([Parameter(Mandatory=$true)][string]$InputDump)

    $taskViewports=@{}
    foreach($taskLine in ($InputDump -split "`n")) {
        if($taskLine -notmatch '^\s*Viewport\s+[^:]+:') {continue}
        $taskActive=[regex]::Matches($taskLine,'\bisActive=\[(0|1)\]')
        if($taskActive.Count -ne 1) {
            throw 'Cannot establish active input viewport; no swipe sent.'
        }
        if($taskActive[0].Groups[1].Value -eq '0') {continue}
        $taskDisplay=[regex]::Matches($taskLine,'\bdisplayId=(\d+)(?:,|\s|$)')
        $taskOrientation=[regex]::Matches($taskLine,'\borientation=([0-3])(?:,|\s|$)')
        $taskFrame=[regex]::Matches($taskLine,'\blogicalFrame=\[\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)\s*\]')
        if($taskDisplay.Count -ne 1 -or $taskOrientation.Count -ne 1 -or $taskFrame.Count -ne 1) {
            throw 'Malformed active input viewport; no swipe sent.'
        }
        $taskLeft=[int]$taskFrame[0].Groups[1].Value
        $taskTop=[int]$taskFrame[0].Groups[2].Value
        $taskRight=[int]$taskFrame[0].Groups[3].Value
        $taskBottom=[int]$taskFrame[0].Groups[4].Value
        $taskWidth=$taskRight-$taskLeft
        $taskHeight=$taskBottom-$taskTop
        if($taskLeft -lt 0 -or $taskTop -lt 0 -or $taskRight -gt 8192 -or $taskBottom -gt 8192 -or
            $taskWidth -lt 320 -or $taskHeight -lt 320) {
            throw 'Unsupported active input viewport; no swipe sent.'
        }
        $taskDisplayId=[int]$taskDisplay[0].Groups[1].Value
        $taskRotation=[int]$taskOrientation[0].Groups[1].Value
        $taskKey="$taskDisplayId/$taskRotation/$taskLeft/$taskTop/$taskRight/$taskBottom"
        $taskViewports[$taskKey]=[pscustomobject]@{
            DisplayId=$taskDisplayId;Orientation=$taskRotation;
            Left=$taskLeft;Top=$taskTop;Width=$taskWidth;Height=$taskHeight
        }
    }
    if($taskViewports.Count -ne 1) {
        throw 'Missing or ambiguous active input viewport; no swipe sent.'
    }
    $taskViewport=@($taskViewports.Values)[0]
    # Default ADB touch input addresses display 0; never guess another display.
    if($taskViewport.DisplayId -ne 0) {
        throw 'No unique active default-display viewport; no swipe sent.'
    }
    return $taskViewport
}
