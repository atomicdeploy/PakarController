# Actual PU850 Tool/build-all.ps1 Write-Step, Assert-Executable and Invoke-Native
# adapted for shared warm-cache wrappers, NO_COLOR and preserved native exits.
function Write-PakarStep {
    param([Parameter(Mandatory)][string]$Message)
    if($null -ne [Environment]::GetEnvironmentVariable('NO_COLOR')){Write-Host "[STEP] $Message"}
    else{Write-Host "[STEP] $Message" -ForegroundColor Cyan}
}
function Assert-PakarExecutable {
    param([Parameter(Mandatory)][string]$Name,[Parameter(Mandatory)][string]$InstallHint)
    if(-not(Get-Command $Name -ErrorAction SilentlyContinue)){throw "Required command '$Name' absent. $InstallHint"}
}
function Invoke-PakarNative {
    param([Parameter(Mandatory)][string]$WorkingDirectory,[Parameter(Mandatory)][string]$FilePath,[string[]]$Arguments=@())
    Push-Location -LiteralPath $WorkingDirectory
    try{
        & $FilePath @Arguments
        if($LASTEXITCODE -ne 0){throw "$FilePath failed with exit code $LASTEXITCODE. Raw diagnostics above are preserved."}
    }finally{Pop-Location}
}
