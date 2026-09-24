# Install qc (Cosmopolitan APE) from the latest GitHub release on Windows.
# Usage (PowerShell):
#   irm https://raw.githubusercontent.com/probelabs/qc/main/scripts/install.ps1 | iex
# Optional env overrides (set before irm|iex, or in the same session):
#   $env:QC_INSTALL_DIR = "$env:USERPROFILE\bin"
#   $env:QC_VERSION = "v0.1.0"   # pin a release tag (default: latest)
#
# Default install dir: $env:LOCALAPPDATA\qc\bin\qc.exe
# APE binaries must be invoked from a shell (e.g. `qc help`), not opened as
# a document from a GUI file manager.

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

$Repo = 'probelabs/qc'
$DefaultDir = Join-Path $env:LOCALAPPDATA 'qc\bin'
$InstallDir = if ($env:QC_INSTALL_DIR -and $env:QC_INSTALL_DIR.Trim() -ne '') {
    $env:QC_INSTALL_DIR.Trim()
} else {
    $DefaultDir
}
$Version = if ($env:QC_VERSION -and $env:QC_VERSION.Trim() -ne '') {
    $env:QC_VERSION.Trim()
} else {
    'latest'
}

function Write-InstallErr([string]$Message) {
    Write-Error "qc install: $Message"
    exit 1
}

function Get-ReleaseDownloadUrl([string]$Asset) {
    if ($Version -eq 'latest') {
        return "https://github.com/$Repo/releases/latest/download/$Asset"
    }
    $tag = if ($Version.StartsWith('v')) { $Version } else { "v$Version" }
    return "https://github.com/$Repo/releases/download/$tag/$Asset"
}

$arch = $env:PROCESSOR_ARCHITECTURE
if (-not $arch) { $arch = 'unknown' }
Write-Host "qc install: detected Windows/$arch (APE binary is universal)"

$label = if ($Version -eq 'latest') { 'latest' } else {
    if ($Version.StartsWith('v')) { $Version } else { "v$Version" }
}

$tmpRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("qc-install-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmpRoot | Out-Null
$tmpBin = Join-Path $tmpRoot 'qc.exe'

try {
    $primaryUrl = Get-ReleaseDownloadUrl 'qc.exe'
    $fallbackUrl = Get-ReleaseDownloadUrl 'qc'
    $downloadUrl = $primaryUrl

    Write-Host "qc install: downloading $label → $primaryUrl"
    $downloaded = $false
    try {
        Invoke-WebRequest -Uri $primaryUrl -OutFile $tmpBin -UseBasicParsing
        $downloaded = $true
    } catch {
        Write-Host "qc install: qc.exe not found; falling back to qc asset"
        $downloadUrl = $fallbackUrl
        try {
            Invoke-WebRequest -Uri $fallbackUrl -OutFile $tmpBin -UseBasicParsing
            $downloaded = $true
        } catch {
            Write-InstallErr "download failed: $primaryUrl and $fallbackUrl"
        }
    }

    if (-not $downloaded -or -not (Test-Path -LiteralPath $tmpBin)) {
        Write-InstallErr "download produced no file: $downloadUrl"
    }

    $size = (Get-Item -LiteralPath $tmpBin).Length
    if ($size -lt 10000) {
        Write-InstallErr "downloaded file looks too small ($size bytes); release asset missing?"
    }

    if (-not (Test-Path -LiteralPath $InstallDir)) {
        New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
    }

    $dest = Join-Path $InstallDir 'qc.exe'
    Copy-Item -LiteralPath $tmpBin -Destination $dest -Force

    Write-Host "qc install: installed $dest ($size bytes)"
    Write-Host "qc install: note — run qc from a shell (APE); do not open the binary in a GUI"

    $pathEntries = $env:Path -split ';' | Where-Object { $_ -ne '' }
    $onPath = $false
    foreach ($entry in $pathEntries) {
        if ([string]::Equals($entry, $InstallDir, [System.StringComparison]::OrdinalIgnoreCase)) {
            $onPath = $true
            break
        }
    }

    if (-not $onPath) {
        Write-Host ""
        Write-Host "qc install: $InstallDir is not on your PATH."
        Write-Host "Add it for this session:"
        Write-Host "  `$env:Path = `"$InstallDir;`$env:Path`""
        Write-Host "Or permanently (User PATH), then open a new shell:"
        Write-Host "  [Environment]::SetEnvironmentVariable('Path', `"$InstallDir;`" + [Environment]::GetEnvironmentVariable('Path', 'User'), 'User')"
        Write-Host ""
    }

    Write-Host "Try: $dest help"
} finally {
    if (Test-Path -LiteralPath $tmpRoot) {
        Remove-Item -LiteralPath $tmpRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
