<#
.SYNOPSIS
    Downloads, verifies and trims the LibreOffice copy that the installer bundles.

.DESCRIPTION
    Produces build\libreoffice\ (program\soffice.exe, share\, licence files), which
    packaging\installer.iss installs to {app}\libreoffice\ as the optional
    "Document conversion" component. converter-cli.exe looks for
    <its folder>\libreoffice\program\soffice.exe first (ADR 0004).

    Steps:
      1. download the pinned LibreOffice MSI from The Document Foundation
         (the "stable" folder, or the archive once a newer release replaced it);
      2. check its SHA-256 against the pinned value and stop on a mismatch;
      3. extract it with an administrative install (msiexec /a), which unpacks
         the files without installing anything or touching the registry;
      4. remove what headless PDF export does not need (help, UI translations,
         dictionaries, Java, Python, Firebird, solvers, icon themes, templates,
         gallery, the online updater and the crash report uploader);
      5. move the bundled fonts into share\fonts\truetype, where LibreOffice
         loads them from, and copy the Visual C++ runtime next to soffice.exe.

    LibreOffice is MPL-2.0. Its licence files (LICENSE.html, license.txt, NOTICE,
    CREDITS.fodt, readmes\readme_en-US.txt) are kept and shipped unchanged. It is
    not rebranded.

    The result is reused when build\libreoffice\.snapformat-version already names
    the pinned version and hash. Use -Force to rebuild it.

    To update LibreOffice: change $Version and $Sha256 below. The hash is published
    next to the MSI as <file>.sha256 on download.documentfoundation.org.

.EXAMPLE
    .\scripts\fetch-libreoffice.ps1
    .\scripts\fetch-libreoffice.ps1 -SmokeTest
    .\scripts\fetch-libreoffice.ps1 -Force
#>
param(
    [string]$OutputDir,
    [switch]$Force,
    # Converts a small document with the result through converter-cli.exe.
    [switch]$SmokeTest
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"   # Invoke-WebRequest is much faster without it.

# --- Pinned release -------------------------------------------------------
$Version = "26.8.3"
$Sha256  = "0e6918c273eabbdbbd800ee0dacbbf130a14c13c2e4cec861b06dd59e015bf94"
# --------------------------------------------------------------------------

$MsiName = "LibreOffice_${Version}_Win_x86-64.msi"
$Urls = @(
    "https://download.documentfoundation.org/libreoffice/stable/$Version/win/x86_64/$MsiName",
    "https://downloadarchive.documentfoundation.org/libreoffice/old/$Version/win/x86_64/$MsiName"
)

$RepoRoot    = Split-Path $PSScriptRoot -Parent
$BuildDir    = Join-Path $RepoRoot "build"
if (-not $OutputDir) { $OutputDir = Join-Path $BuildDir "libreoffice" }
$OutputDir   = [IO.Path]::GetFullPath($OutputDir)
$DownloadDir = Join-Path $BuildDir "libreoffice-download"
$StageDir    = Join-Path $BuildDir "libreoffice-stage"
$Msi         = Join-Path $DownloadDir $MsiName
$Marker      = Join-Path $OutputDir ".snapformat-version"
$MarkerText  = "$Version $Sha256"

# Paths (relative to the LibreOffice root) that headless PDF export does not use.
$RemovePaths = @(
    "help",                         # offline help
    "program\classes",              # Java (Base/HSQLDB, some wizards)
    "program\python-core-*",        # Python scripting runtime...
    "program\python*",              # ...and its interpreter and UNO bridge
    "program\pyuno.pyd",
    "program\Engine12.dll",         # Firebird engine of Base (mergedlo.dll links ifbclient.dll,
    "share\firebird",               # CoinMP.dll and lpsolve55.dll directly, so those stay)
    "program\xpdfimport.exe",      # PDF import (not export)
    "share\xpdfimport",
    "program\updater.exe",          # online update and crash report upload: a bundled
    "program\update_service.exe",   # copy is updated with SnapFormat, and must not
    "program\mar.exe",              # send anything off the machine
    "program\minidump_upload.exe",
    "program\shlxthdl",             # Explorer thumbnail handler, never registered
    "share\extensions",             # spelling/hyphenation dictionaries, wiki publisher, solver
    "share\gallery",                # clip art
    "share\template",               # document templates
    "share\autotext",               # AutoText entries
    "share\wizards",                # wizard templates
    "System",                      # 32-bit VC++ runtime (the 64-bit one is copied below)
    $MsiName                        # the copy msiexec /a leaves in the target folder
)

# Fonts kept from the package: OpenSymbol (bullets, math), and the metric-compatible
# replacements for Arial/Times/Courier (Liberation), Calibri (Carlito) and Cambria (Caladea).
$KeepFonts = @("opens___*", "Liberation*", "Carlito*", "Caladea*", "DejaVu*")

function Get-Sha256([string]$Path) {
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
}

function Get-FolderSizeMB([string]$Path) {
    $sum = (Get-ChildItem -LiteralPath $Path -Recurse -File -Force | Measure-Object Length -Sum).Sum
    return [math]::Round($sum / 1MB)
}

function Save-Msi {
    if ((Test-Path -LiteralPath $Msi) -and (Get-Sha256 $Msi) -eq $Sha256) {
        Write-Host "Using cached $MsiName" -ForegroundColor DarkGray
        return
    }

    New-Item -ItemType Directory -Force -Path $DownloadDir | Out-Null
    $partial = "$Msi.partial"
    foreach ($url in $Urls) {
        Write-Host "Downloading $url" -ForegroundColor Cyan
        try {
            Invoke-WebRequest -Uri $url -OutFile $partial -UseBasicParsing
            break
        } catch {
            Write-Host "  failed: $($_.Exception.Message)" -ForegroundColor Yellow
            Remove-Item -LiteralPath $partial -Force -ErrorAction SilentlyContinue
        }
    }
    if (-not (Test-Path -LiteralPath $partial)) {
        throw "Could not download $MsiName from any mirror."
    }

    $actual = Get-Sha256 $partial
    if ($actual -ne $Sha256) {
        Remove-Item -LiteralPath $partial -Force
        throw "SHA-256 mismatch for ${MsiName}: expected $Sha256, got $actual. The download was deleted."
    }
    Move-Item -LiteralPath $partial -Destination $Msi -Force
    Write-Host "SHA-256 verified: $Sha256" -ForegroundColor Green
}

function Expand-Msi {
    if (Test-Path -LiteralPath $StageDir) { Remove-Item -LiteralPath $StageDir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

    Write-Host "Extracting the MSI (administrative install, nothing is installed)..." -ForegroundColor Cyan
    $log = Join-Path $BuildDir "libreoffice-extract.log"
    $arguments = @("/a", "`"$Msi`"", "/qn", "TARGETDIR=`"$StageDir`"", "/L*v", "`"$log`"")
    $process = Start-Process msiexec.exe -ArgumentList $arguments -Wait -PassThru
    if ($process.ExitCode -ne 0) {
        throw "msiexec /a failed with exit code $($process.ExitCode). See $log."
    }
    if (-not (Test-Path -LiteralPath (Join-Path $StageDir "program\soffice.exe"))) {
        throw "The extracted MSI has no program\soffice.exe. See $log."
    }
}

function Optimize-Stage {
    $before = Get-FolderSizeMB $StageDir

    foreach ($pattern in $RemovePaths) {
        Get-Item -Path (Join-Path $StageDir $pattern) -Force -ErrorAction SilentlyContinue |
            Remove-Item -Recurse -Force
    }

    # English only: UI translations, per-language configuration and readmes.
    Get-ChildItem -LiteralPath (Join-Path $StageDir "program\resource") -Directory |
        Where-Object Name -ne "en-US" | Remove-Item -Recurse -Force
    Get-ChildItem -LiteralPath (Join-Path $StageDir "share\registry") -File -Filter "Langpack-*.xcd" |
        Where-Object Name -ne "Langpack-en-US.xcd" | Remove-Item -Force
    Get-ChildItem -LiteralPath (Join-Path $StageDir "share\registry\res") -File |
        Where-Object Name -notlike "*en-US.xcd" | Remove-Item -Force
    Get-ChildItem -LiteralPath (Join-Path $StageDir "readmes") -File |
        Where-Object Name -ne "readme_en-US.txt" | Remove-Item -Force

    # One icon theme is enough for a program that never shows a window.
    Get-ChildItem -LiteralPath (Join-Path $StageDir "share\config") -File -Filter "images_*.zip" |
        Where-Object Name -ne "images_colibre.zip" | Remove-Item -Force

    # The MSI installs its fonts into Windows\Fonts; a bundled copy loads them
    # from share\fonts\truetype instead.
    $fontSource = Join-Path $StageDir "Fonts"
    $fontTarget = Join-Path $StageDir "share\fonts\truetype"
    New-Item -ItemType Directory -Force -Path $fontTarget | Out-Null
    foreach ($pattern in $KeepFonts) {
        Get-ChildItem -LiteralPath $fontSource -Recurse -File |
            Where-Object Name -like $pattern | Move-Item -Destination $fontTarget -Force
    }
    Remove-Item -LiteralPath $fontSource -Recurse -Force

    # The MSI installs the Visual C++ runtime into System32; ship it app-local.
    $runtime = Join-Path $StageDir "System64"
    Get-ChildItem -LiteralPath $runtime -Recurse -File -Filter "*.dll" |
        Copy-Item -Destination (Join-Path $StageDir "program") -Force
    Remove-Item -LiteralPath $runtime -Recurse -Force

    $after = Get-FolderSizeMB $StageDir
    Write-Host "Trimmed LibreOffice from $before MB to $after MB" -ForegroundColor Green
}

function Invoke-SmokeTest {
    $cli = Join-Path $BuildDir "bin\Release\converter-cli.exe"
    if (-not (Test-Path -LiteralPath $cli)) { throw "Smoke test needs a Release build: $cli not found." }

    $work = Join-Path $BuildDir "libreoffice-smoke"
    if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $work | Out-Null
    $source = Join-Path $work "smoke.rtf"
    $output = Join-Path $work "smoke.pdf"
    Set-Content -LiteralPath $source -Encoding Ascii -Value "{\rtf1\ansi{\fonttbl{\f0 Calibri;}}\f0\fs24 SnapFormat smoke test \bullet\par}"

    Write-Host "Smoke test: converting an RTF document with the bundled LibreOffice..." -ForegroundColor Cyan
    $env:SNAPFORMAT_SOFFICE = Join-Path $OutputDir "program\soffice.exe"
    try {
        & $cli --input $source --output $output --format pdf
        $exitCode = $LASTEXITCODE
    } finally {
        Remove-Item Env:\SNAPFORMAT_SOFFICE
    }
    if ($exitCode -ne 0) { throw "Smoke test failed: converter-cli exited with $exitCode." }

    $header = [IO.File]::ReadAllBytes($output)[0..3]
    if ([Text.Encoding]::ASCII.GetString($header) -ne "%PDF") { throw "Smoke test failed: $output is not a PDF." }
    Remove-Item -LiteralPath $work -Recurse -Force
    Write-Host "Smoke test passed." -ForegroundColor Green
}

if (-not $Force -and (Test-Path -LiteralPath $Marker) -and (Get-Content -LiteralPath $Marker -Raw).Trim() -eq $MarkerText) {
    Write-Host "LibreOffice $Version is already prepared in $OutputDir" -ForegroundColor Green
} else {
    Save-Msi
    Expand-Msi
    Optimize-Stage

    if (Test-Path -LiteralPath $OutputDir) { Remove-Item -LiteralPath $OutputDir -Recurse -Force }
    Move-Item -LiteralPath $StageDir -Destination $OutputDir
    Set-Content -LiteralPath $Marker -Value $MarkerText -Encoding Ascii
    Write-Host "LibreOffice $Version prepared in $OutputDir ($(Get-FolderSizeMB $OutputDir) MB)" -ForegroundColor Green
}

if ($SmokeTest) { Invoke-SmokeTest }
