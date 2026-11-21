# Packaging

Files in `packaging/`:

| File | Purpose |
|---|---|
| `installer.iss` | Inno Setup script → `SnapFormat_Setup.exe` |
| `AppxManifest.xml` | MSIX sparse package manifest |
| `Assets/` | Package logos (`StoreLogo`, `Square150x150Logo`, `Square44x44Logo`) |
| `register-msix.ps1` | Builds, signs and registers the sparse package |

`install.bat` at the repository root is the user-facing wrapper for `register-msix.ps1`.

## Inno Setup installer (`installer.iss`)

| Setting | Value |
|---|---|
| `AppName` / `AppVersion` | `SnapFormat` / `0.1.0` |
| `DefaultDirName` | `{userappdata}\SnapFormat` |
| `PrivilegesRequired` | `lowest` (per-user, no UAC) |
| `ArchitecturesInstallIn64BitMode` | `x64` |
| Output | `build\installer\SnapFormat_Setup.exe` |

Installed files: `converter-cli.exe`, `shell-ext.dll` (flag `regserver` → `DllRegisterServer` on install / `DllUnregisterServer` on uninstall) and `snapformat.ico`. A Start menu "Uninstall SnapFormat" shortcut is created.

### Components

| Component | Default | Contents |
|---|---|---|
| `main` — SnapFormat (images) | always (fixed) | The files above. |
| `documents` — Document conversion to PDF | selected (*Images and documents* type) | LibreOffice in `{app}\libreoffice`, where `converter-cli.exe` looks first ([ADR 0004](../adr/0004-document-conversion-engines.md)). |

`[InstallDelete]` always removes `{app}\libreoffice` before copying files, so an upgrade never mixes two LibreOffice versions and deselecting the component removes it. The uninstaller also deletes the private LibreOffice profile in `%LOCALAPPDATA%\SnapFormat\libreoffice-profile`.

### Bundled LibreOffice (`scripts/fetch-libreoffice.ps1`)

`installer.iss` refuses to compile until `build\libreoffice` exists. The script prepares it:

1. Downloads the pinned MSI (`$Version`, currently **26.8.3**) from `download.documentfoundation.org`, falling back to the TDF archive once a newer release replaces it.
2. Checks the **SHA-256** against the pinned `$Sha256` and stops on a mismatch.
3. Extracts the MSI with an administrative install (`msiexec /a`), which installs nothing and needs no admin rights.
4. Removes what headless PDF export does not use: help, UI translations and per-language configuration (English only), dictionaries and other extensions, Java, Python, the Firebird engine, icon themes except Colibre, templates, gallery, AutoText, PDF import, the online updater and the crash report uploader.
5. Moves OpenSymbol, Liberation, Carlito, Caladea and DejaVu fonts to `share\fonts\truetype` (the MSI would install them into `Windows\Fonts`), and copies the 64-bit Visual C++ runtime next to `soffice.exe`.
6. With `-SmokeTest`, converts a small RTF document to PDF through `converter-cli.exe`.

Sizes for 26.8.3: the full extraction is about 1.5 GB, the trimmed copy about **470 MB**. Most of what remains is LibreOffice's core (`mergedlo.dll` alone is 145 MB). `mergedlo.dll` links `CoinMP.dll`, `lpsolve55.dll` and `ifbclient.dll` directly, so those stay even though their features are unused.

**Licence.** LibreOffice is MPL-2.0. Its licence files (`LICENSE.html`, `license.txt`, `NOTICE`, `CREDITS.fodt`, `readmes\readme_en-US.txt`) are installed unchanged, and a Start menu shortcut opens `LICENSE.html`. LibreOffice is not rebranded.

**Updating LibreOffice.** Change `$Version` and `$Sha256` at the top of the script; the hash is published next to the MSI as `<file>.sha256`. Run `.\scripts\fetch-libreoffice.ps1 -Force -SmokeTest`, then test documents of each kind (Word, Excel, PowerPoint). Update for LibreOffice security releases.

The installer and the MSIX sparse package register the same CLSID, so only one may be installed. On start, Setup detects the MSIX package and offers to remove it; `register-msix.ps1` refuses to run while the registry install is present.

Explorer keeps `shell-ext.dll` loaded, so uninstalling cannot always delete it. The uninstaller then adds an HKCU `RunOnce` entry that deletes the install folder at the next sign-in; a later install clears that entry first.

Build: `iscc packaging\installer.iss` after a Release build.

## MSIX sparse package

A *sparse package* gives an unpackaged desktop app a package identity, which is what Windows 11 uses to place `IExplorerCommand` handlers in the modern context menu. The MSIX contains only the manifest and logos; the binaries stay in an **external location**.

### Manifest highlights

- Identity: `Name="SnapFormat"`, `Publisher="CN=MarcAntero"`, `Version="1.0.0.0"`, `x64`.
- `uap10:AllowExternalContent=true`, capabilities `runFullTrust` and `unvirtualizedResources`.
- `TargetDeviceFamily Windows.Desktop`, `MinVersion=10.0.22000.0`.
- Application `Executable="converter-cli.exe"`, `EntryPoint="Windows.FullTrustApplication"`.
- `desktop4:FileExplorerContextMenus` → verb `snapFormatConvert` for the image and document extensions.
- `com:SurrogateServer` → class `{8D5F6D5B-…}` in `shell-ext.dll`, `STA`.

The publisher in the manifest **must match** the certificate subject used to sign the package.

### `register-msix.ps1` steps

1. Find the newest Windows SDK `bin\<version>\x64` with `makeappx.exe`.
2. Copy manifest + assets into a temporary folder, run `makeappx pack /nv` into `packaging\SnapFormat.msix`, and delete the temp folder.
3. Create a throwaway self-signed certificate (`CN=MarcAntero`, non-exportable key) in `CurrentUser\My`, sign with `signtool sign /fd SHA256 /sha1 <thumbprint> /s My`, then delete the certificate **and its private key**.
4. Remove any existing `SnapFormat` package and the previous sideload certificate, add the new public certificate to `LocalMachine\TrustedPeople`, and run `Add-AppxPackage -ExternalLocation <root>\build\bin\Release`.

Before step 1 it removes legacy development certificates installed by earlier versions (see [Signing certificate](#signing-certificate)). It must run elevated (`#Requires -RunAsAdministrator`).

### Release ZIP layout

The CI produces `SnapFormat.zip` with the layout the script expects:

```
SnapFormat.zip
├── install.bat
├── build\bin\Release\converter-cli.exe
├── build\bin\Release\shell-ext.dll
├── build\bin\Release\snapformat.ico
└── packaging\  (manifest, assets, scripts, installer.iss)
```

The ZIP does not include LibreOffice (see [ADR 0004](../adr/0004-document-conversion-engines.md#bundling-61)). Document conversion with this channel uses a LibreOffice installed on the system, or a copy placed in `build\bin\Release\libreoffice`.


## Signing certificate

The sparse package is signed with a **throwaway certificate created on every install**:

- No PFX file or password is ever written to disk or committed.
- Only the public certificate is trusted, in `LocalMachine\TrustedPeople`. That is enough for MSIX sideloading, and unlike `Root` it cannot act as a certificate authority.
- The private key is deleted right after signing, so the trusted certificate can never sign anything else.

### Legacy certificates (before v1.4)

Earlier versions committed `Win11QuickConverter_DevCert.pfx` (private key + password) to the repository and the release ZIP, and installed a `CN=MarcAntero` / *Win11 Quick Converter Dev* certificate into **Trusted Root**. Anyone with that PFX could sign code your machine would trust.

`register-msix.ps1` now removes these certificates automatically. To remove them manually (elevated PowerShell) without reinstalling:

```powershell
Get-ChildItem Cert:\LocalMachine\Root, Cert:\CurrentUser\Root, Cert:\CurrentUser\My |
  Where-Object { $_.Subject -eq 'CN=MarcAntero' -and $_.FriendlyName -eq 'Win11 Quick Converter Dev' } |
  Remove-Item
```

Also delete any `Win11QuickConverter_DevCert.pfx` left in the folder where you extracted an older `SnapFormat.zip`.

For public releases, sign with a real code-signing certificate or Azure Trusted Signing ([#55](https://github.com/marcantero/win11-quick-converter/issues/55)), which also removes SmartScreen warnings for `SnapFormat_Setup.exe`.

## Versioning

Version numbers are currently set in three places and are not in sync:

| File | Value |
|---|---|
| `CMakeLists.txt` (`project VERSION`) | `0.1.0` |
| `packaging/installer.iss` (`AppVersion`) | `0.1.0` |
| `packaging/AppxManifest.xml` (`Identity Version`) | `1.0.0.0` |

Git tags go up to `v1.3.0`. When releasing, update all three to match the tag.
