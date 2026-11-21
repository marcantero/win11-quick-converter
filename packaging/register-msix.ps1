#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Builds, signs and registers the SnapFormat MSIX sparse package.

.DESCRIPTION
    Signing uses a throwaway self-signed certificate created on every run:
      * no PFX file and no password are written to disk;
      * only the public certificate is trusted, in LocalMachine\TrustedPeople
        (enough for MSIX sideloading; it cannot act as a root CA);
      * the private key is deleted right after signing, so the trusted
        certificate can never sign anything else.

    Older versions of this script installed a "Win11 Quick Converter Dev"
    certificate into LocalMachine\Root and left a password-protected PFX on
    disk. Those are removed automatically.
#>

$ErrorActionPreference = "Stop"

$Publisher       = "CN=MarcAntero"   # must match Identity/@Publisher in AppxManifest.xml
$FriendlyName    = "SnapFormat Sideload Signing"
$LegacyName      = "Win11 Quick Converter Dev"
$LegacyPfxName   = "Win11QuickConverter_DevCert.pfx"
$PackageName     = "SnapFormat.msix"

# Certificates whose private keys were published in the repository.
$LeakedThumbprints = @(
    "9FCDEE9C79466BAE789FC239807B004497D8E92A",
    "3C1E39D948D85B7C6D9CB5C549A5D0430B11F9BF"
)

function Remove-Certificates {
    param([string]$StorePath, [scriptblock]$Filter, [switch]$DeleteKey)
    Get-ChildItem $StorePath | Where-Object $Filter | ForEach-Object {
        Write-Host "  Removing $($_.Subject) ($($_.Thumbprint)) from $StorePath" -ForegroundColor DarkGray
        if ($DeleteKey -and $_.HasPrivateKey) { Remove-Item $_.PSPath -DeleteKey } else { Remove-Item $_.PSPath }
    }
}

# 0. Clean up certificates left by previous versions
Write-Host "Removing legacy development certificates..." -ForegroundColor Cyan
$isLegacy = { ($_.Subject -eq $Publisher -and $_.FriendlyName -eq $LegacyName) -or ($LeakedThumbprints -contains $_.Thumbprint) }
Remove-Certificates "Cert:\LocalMachine\Root"   $isLegacy
Remove-Certificates "Cert:\CurrentUser\Root"    $isLegacy
Remove-Certificates "Cert:\CurrentUser\My"      $isLegacy -DeleteKey
Remove-Certificates "Cert:\LocalMachine\TrustedPeople" { $LeakedThumbprints -contains $_.Thumbprint }
foreach ($pfx in @((Join-Path (Get-Location) $LegacyPfxName), (Join-Path $PSScriptRoot $LegacyPfxName))) {
    if (Test-Path $pfx) {
        Write-Host "  Deleting $pfx" -ForegroundColor DarkGray
        Remove-Item $pfx -Force
    }
}

# 1. Locate Windows SDK tools
Write-Host "Looking for Windows SDK tools..." -ForegroundColor Cyan
$sdkPath = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*" -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    Where-Object { Test-Path "$($_.FullName)\x64\makeappx.exe" } |
    Select-Object -First 1

if (-not $sdkPath) {
    Write-Error "Windows SDK not found. Install it from the Visual Studio Installer."
    exit 1
}

$makeappx = "$($sdkPath.FullName)\x64\makeappx.exe"
$signtool = "$($sdkPath.FullName)\x64\signtool.exe"
Write-Host "SDK found at: $($sdkPath.FullName)" -ForegroundColor Green

# 2. Pack (makeappx)
$TempDir = Join-Path $env:TEMP "SnapFormat-Appx-$([guid]::NewGuid())"
$PackagePath = Join-Path $PSScriptRoot $PackageName
New-Item -ItemType Directory -Path $TempDir | Out-Null
try {
    Copy-Item "$PSScriptRoot\AppxManifest.xml" -Destination $TempDir
    Copy-Item "$PSScriptRoot\Assets" -Destination $TempDir -Recurse

    Write-Host "Packing the MSIX..." -ForegroundColor Cyan
    & $makeappx pack /d $TempDir /p $PackagePath /nv /o | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "makeappx failed with exit code $LASTEXITCODE." }
} finally {
    Remove-Item $TempDir -Recurse -Force -ErrorAction SilentlyContinue
}

# 3. Sign with a throwaway certificate, then destroy its private key
Write-Host "Signing the MSIX with a throwaway certificate..." -ForegroundColor Cyan
$cert = New-SelfSignedCertificate -Type Custom -Subject $Publisher -FriendlyName $FriendlyName `
    -KeyUsage DigitalSignature -KeyExportPolicy NonExportable `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -NotAfter (Get-Date).AddYears(5) `
    -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
try {
    & $signtool sign /fd SHA256 /sha1 $cert.Thumbprint /s My $PackagePath | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "signtool failed with exit code $LASTEXITCODE." }
    $publicCert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2(,$cert.RawData)
} finally {
    Remove-Item $cert.PSPath -DeleteKey
}

# 4. Replace the previous package and its trusted certificate
Write-Host "Registering the sparse package..." -ForegroundColor Cyan
Get-AppxPackage -Name "SnapFormat" | Remove-AppxPackage -ErrorAction SilentlyContinue
Remove-Certificates "Cert:\LocalMachine\TrustedPeople" { $_.Subject -eq $Publisher -and $_.FriendlyName -eq $FriendlyName }

$store = New-Object System.Security.Cryptography.X509Certificates.X509Store("TrustedPeople", "LocalMachine")
$store.Open("ReadWrite")
try {
    $publicCert.FriendlyName = $FriendlyName
    $store.Add($publicCert)
} finally {
    $store.Close()
}

# The binaries live in build\bin\Release next to the packaging folder.
$RootPath = (Get-Item $PSScriptRoot).Parent.FullName
$ExternalLocation = Join-Path $RootPath "build\bin\Release"
Add-AppxPackage -Path $PackagePath -ExternalLocation $ExternalLocation

Write-Host "Done. The context menu entry should now be available." -ForegroundColor Green
