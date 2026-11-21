<#
.SYNOPSIS
    Builds SnapFormat and installs it for the current user, like the real installer does.

.DESCRIPTION
    Mirrors packaging\installer.iss without needing Inno Setup:
      1. builds the project (Release by default);
      2. stops Explorer so the old shell-ext.dll is no longer locked;
      3. copies converter-cli.exe, shell-ext.dll and snapformat.ico to %APPDATA%\SnapFormat;
      4. registers the DLL with regsvr32 (per user, no admin rights);
      5. starts Explorer again.

    Run it again after every change. Use -Uninstall to remove everything.

    The entry is registered through the registry, so on Windows 11 it appears under
    "Show more options" (Shift+F10). The modern top-level menu needs the MSIX sparse
    package (packaging\register-msix.ps1); do not use both at the same time.

.EXAMPLE
    .\scripts\dev-install.ps1
    .\scripts\dev-install.ps1 -SkipBuild
    .\scripts\dev-install.ps1 -Config Debug
    .\scripts\dev-install.ps1 -Uninstall
#>
param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",
    [switch]$SkipBuild,
    [switch]$Uninstall
)

$ErrorActionPreference = "Stop"

$RepoRoot   = Split-Path $PSScriptRoot -Parent
$BuildDir   = Join-Path $RepoRoot "build"
$BinDir     = Join-Path $BuildDir "bin\$Config"
$InstallDir = Join-Path $env:APPDATA "SnapFormat"
$Dll        = Join-Path $InstallDir "shell-ext.dll"

function Invoke-Regsvr32 {
    param([string[]]$Arguments)
    $p = Start-Process regsvr32.exe -ArgumentList $Arguments -Wait -PassThru -WindowStyle Hidden
    return $p.ExitCode
}

function Find-CMake {
    # 1. cmake on PATH (standalone install or a Developer PowerShell)
    $onPath = Get-Command cmake -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }

    # 2. the copy bundled with Visual Studio
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPaths = & $vswhere -all -prerelease -products * -sort -property installationPath
        foreach ($vs in $vsPaths) {
            $candidate = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $candidate) { return $candidate }
        }
    }

    throw "CMake not found. Install the 'C++ CMake tools for Windows' component in Visual Studio, or add cmake to PATH."
}

function Stop-Explorer {
    Write-Host "Stopping Explorer..." -ForegroundColor Cyan
    Get-Process explorer -ErrorAction SilentlyContinue | Stop-Process -Force
    # Windows may restart the shell on its own; keep killing it until the DLL is free.
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 250
        $running = Get-Process explorer -ErrorAction SilentlyContinue
        if (-not $running) { return }
        $running | Stop-Process -Force
    }
}

function Start-Explorer {
    if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) {
        Write-Host "Starting Explorer..." -ForegroundColor Cyan
        Start-Process explorer.exe
    }
}

function Copy-WithRetry {
    param([string]$Source, [string]$Destination)
    for ($i = 1; $i -le 10; $i++) {
        try {
            Copy-Item $Source $Destination -Force
            return
        } catch {
            if ($i -eq 10) { throw }
            Start-Sleep -Milliseconds 500
        }
    }
}

# ---- Uninstall ---------------------------------------------------------------
if ($Uninstall) {
    if (-not (Test-Path $InstallDir)) {
        Write-Host "Nothing to uninstall: $InstallDir does not exist." -ForegroundColor Yellow
        return
    }
    Stop-Explorer
    try {
        if (Test-Path $Dll) {
            $code = Invoke-Regsvr32 @("/u", "/s", "`"$Dll`"")
            if ($code -ne 0) { Write-Warning "regsvr32 /u returned $code" }
        }
        Remove-Item $InstallDir -Recurse -Force
        Write-Host "SnapFormat uninstalled." -ForegroundColor Green
    } finally {
        Start-Explorer
    }
    return
}

# The MSIX sparse package registers the same CLSID; both together show the entry twice.
if (Get-AppxPackage -Name "SnapFormat" -ErrorAction SilentlyContinue) {
    throw "The SnapFormat MSIX package is installed. Remove it first: Get-AppxPackage -Name SnapFormat | Remove-AppxPackage"
}

# ---- Build -------------------------------------------------------------------
if (-not $SkipBuild) {
    $cmake = Find-CMake
    Write-Host "Using $cmake" -ForegroundColor DarkGray
    if (-not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
        Write-Host "Configuring CMake..." -ForegroundColor Cyan
        & $cmake -S $RepoRoot -B $BuildDir
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
    }
    Write-Host "Building ($Config)..." -ForegroundColor Cyan
    & $cmake --build $BuildDir --config $Config
    if ($LASTEXITCODE -ne 0) { throw "Build failed." }
}

$files = @(
    (Join-Path $BinDir "converter-cli.exe"),
    (Join-Path $BinDir "shell-ext.dll"),
    (Join-Path $BinDir "snapformat.ico")
)
foreach ($f in $files) {
    if (-not (Test-Path $f)) { throw "Missing build output: $f" }
}

# ---- Install -----------------------------------------------------------------
Stop-Explorer
try {
    New-Item -ItemType Directory -Force $InstallDir | Out-Null

    Write-Host "Copying files to $InstallDir..." -ForegroundColor Cyan
    foreach ($f in $files) { Copy-WithRetry $f $InstallDir }

    Write-Host "Registering shell-ext.dll..." -ForegroundColor Cyan
    $code = Invoke-Regsvr32 @("/s", "`"$Dll`"")
    if ($code -ne 0) {
        throw "regsvr32 failed with exit code $code. See %TEMP%\win11qc_register.log."
    }
} finally {
    Start-Explorer
}

Write-Host ""
Write-Host "SnapFormat ($Config) installed in $InstallDir" -ForegroundColor Green
Write-Host "Right-click an image -> Show more options -> Convert with SnapFormat."
