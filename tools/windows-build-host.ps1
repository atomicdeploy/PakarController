# Adapted actual helpers from Pealayer scripts/libmpv-windows.ps1.
# Host-selected cache/file identity retained; mpv resolution is intentionally absent.
Set-StrictMode -Version Latest

function Get-PakarWindowsHostPaths {
    [CmdletBinding()]
    param([Parameter(Mandatory=$true)][string]$RepositoryRoot)
    $programRoot = Join-Path $env:LOCALAPPDATA 'Programs\PakarController'
    [pscustomobject]@{
        ProgramRoot = $programRoot
        ProfileDirectory = Join-Path $programRoot 'config'
        CargoTargetDirectory = Join-Path $RepositoryRoot 'desktop/target'
    }
}

function Get-PakarCargoTargetDirectory {
    [CmdletBinding()]
    param([Parameter(Mandatory=$true)][string]$RepositoryRoot)
    if (-not [string]::IsNullOrWhiteSpace($env:CARGO_TARGET_DIR)) {
        $configured = [Environment]::ExpandEnvironmentVariables($env:CARGO_TARGET_DIR)
        if (-not [IO.Path]::IsPathRooted($configured)) {
            $configured = Join-Path $RepositoryRoot $configured
        }
        return [IO.Path]::GetFullPath($configured)
    }
    return (Get-PakarWindowsHostPaths -RepositoryRoot $RepositoryRoot).CargoTargetDirectory
}

function Get-PakarFileIdentity {
    [CmdletBinding()]
    param([Parameter(Mandatory=$true)][string]$Path)
    $item = Get-Item -LiteralPath $Path -ErrorAction Stop
    [ordered]@{
        path = $item.FullName
        file_name = $item.Name
        bytes = $item.Length
        sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        file_version = if ($item.VersionInfo.FileVersion) { $item.VersionInfo.FileVersion } else { $null }
        product_version = if ($item.VersionInfo.ProductVersion) { $item.VersionInfo.ProductVersion } else { $null }
    }
}

# Actual Pealayer Test-PealayerFileContentEqual; only the product name changes.
function Test-PakarFileContentEqual {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$Left,
        [Parameter(Mandatory = $true)][string]$Right
    )
    if (-not (Test-Path -LiteralPath $Left -PathType Leaf) -or
        -not (Test-Path -LiteralPath $Right -PathType Leaf)) { return $false }
    $leftFile = Get-Item -LiteralPath $Left
    $rightFile = Get-Item -LiteralPath $Right
    if ($leftFile.Length -ne $rightFile.Length) { return $false }
    return (Get-FileHash -LiteralPath $Left -Algorithm SHA256).Hash -eq
        (Get-FileHash -LiteralPath $Right -Algorithm SHA256).Hash
}

# Adapted actual source-stability check from Pealayer scripts/package-windows.ps1.
function Assert-PakarPackageSourceStable([string]$RepositoryRoot, [string]$ExpectedCommit) {
    $currentCommit = (& git -C $RepositoryRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $currentCommit -ne $ExpectedCommit) {
        throw 'Package source commit changed during the build; refusing publication.'
    }
    $unexpected = @(& git -C $RepositoryRoot status --porcelain --untracked-files=all)
    if ($LASTEXITCODE -ne 0) { throw 'Could not verify package source status.' }
    if ($unexpected.Count -ne 0) {
        throw "Package source changed during the build: $($unexpected -join '; ')"
    }
}
