#Requires -RunAsAdministrator

$ErrorActionPreference = "Stop"

$Publisher = "CN=MarcAntero"
$Password = ConvertTo-SecureString -String "Win11QC_Dev!" -Force -AsPlainText
$CertName = "Win11QuickConverter_DevCert.pfx"
$PackageName = "SnapFormat.msix"

Write-Host "Buscant les eines del Windows SDK..." -ForegroundColor Cyan
$sdkPaths = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*" -Directory | Sort-Object Name -Descending
$sdkPath = $sdkPaths | Where-Object { Test-Path "$($_.FullName)\x64\makeappx.exe" } | Select-Object -First 1

if (-not $sdkPath) {
    Write-Error "No s'ha trobat el Windows SDK. Assegurat de tenir-lo instal.lat des del Visual Studio Installer."
    exit 1
}

$makeappx = "$($sdkPath.FullName)\x64\makeappx.exe"
$signtool = "$($sdkPath.FullName)\x64\signtool.exe"

Write-Host "SDK trobat a: $($sdkPath.FullName)" -ForegroundColor Green

# 1. Creacio del certificat local
if (-not (Test-Path $CertName)) {
    Write-Host "Creant un certificat local de proves..." -ForegroundColor Cyan
    $cert = New-SelfSignedCertificate -Type Custom -Subject $Publisher -KeyUsage DigitalSignature -FriendlyName "Win11 Quick Converter Dev" -CertStoreLocation "Cert:\CurrentUser\My" -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
    
    # Exportar el certificat a un fitxer .pfx
    Export-PfxCertificate -Cert $cert -FilePath $CertName -Password $Password | Out-Null
    
    # Instal.lar-lo a la llista de certificats arrel de confianca de la maquina
    Write-Host "Instal.lant el certificat a la llista d'arrels de confianca..." -ForegroundColor Cyan
    $store = New-Object System.Security.Cryptography.X509Certificates.X509Store("Root", "LocalMachine")
    $store.Open("ReadWrite")
    $store.Add($cert)
    $store.Close()
} else {
    Write-Host "El certificat de proves ja existeix." -ForegroundColor Green
}

# 2. Empaquetar (makeappx)
$TempDir = "AppxTemp"
if (Test-Path $TempDir) { Remove-Item $TempDir -Recurse -Force }
New-Item -ItemType Directory -Path $TempDir | Out-Null
Copy-Item "$PSScriptRoot\AppxManifest.xml" -Destination $TempDir
Copy-Item "$PSScriptRoot\Assets" -Destination $TempDir -Recurse

Write-Host "Empaquetant l'MSIX..." -ForegroundColor Cyan
& $makeappx pack /d $TempDir /p $PackageName /nv /o
Remove-Item $TempDir -Recurse -Force

# 3. Signar (signtool)
Write-Host "Signant l'MSIX..." -ForegroundColor Cyan
& $signtool sign /fd SHA256 /a /f $CertName /p "Win11QC_Dev!" $PackageName | Out-Null

# 4. Registrar a Windows (Sparse/External Location)
Write-Host "Registrant el Sparse Package a Windows..." -ForegroundColor Cyan

# Calculemos la ruta de la carpeta build, que esta un nivel por encima de 'packaging'
$RootPath = (Get-Item $PSScriptRoot).Parent.FullName
$ExternalLocation = Join-Path $RootPath "build\bin\Release"

# Desinstal.la versions previes si n'hi ha
Get-AppxPackage -Name "SnapFormat" | Remove-AppxPackage -ErrorAction SilentlyContinue

# Registra amb la ubicacio externa
Add-AppxPackage -Path $PackageName -ExternalLocation $ExternalLocation

Write-Host "Proces completat amb exit! El menu contextual ja hauria d'estar disponible." -ForegroundColor Green