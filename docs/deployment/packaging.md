# Packaging

Files in `packaging/`:

| File | Purpose |
|---|---|
| `installer.iss` | Inno Setup script → `SnapFormat_Setup.exe` |
| `AppxManifest.xml` | MSIX sparse package manifest |
| `Assets/` | Package logos (`StoreLogo`, `Square150x150Logo`, `Square44x44Logo`) |
| `register-msix.ps1` | Builds, signs and registers the sparse package |
| `Win11QuickConverter_DevCert.pfx` | Development signing certificate (see security notes) |

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

Build: `iscc packaging\installer.iss` after a Release build.

## MSIX sparse package

A *sparse package* gives an unpackaged desktop app a package identity, which is what Windows 11 uses to place `IExplorerCommand` handlers in the modern context menu. The MSIX contains only the manifest and logos; the binaries stay in an **external location**.

### Manifest highlights

- Identity: `Name="SnapFormat"`, `Publisher="CN=MarcAntero"`, `Version="1.0.0.0"`, `x64`.
- `uap10:AllowExternalContent=true`, capabilities `runFullTrust` and `unvirtualizedResources`.
- `TargetDeviceFamily Windows.Desktop`, `MinVersion=10.0.22000.0`.
- Application `Executable="converter-cli.exe"`, `EntryPoint="Windows.FullTrustApplication"`.
- `desktop4:FileExplorerContextMenus` → verb `snapFormatConvert` for 9 extensions.
- `com:SurrogateServer` → class `{8D5F6D5B-…}` in `shell-ext.dll`, `STA`.

The publisher in the manifest **must match** the certificate subject used to sign the package.

### `register-msix.ps1` steps

1. Find the newest Windows SDK `bin\<version>\x64` with `makeappx.exe`.
2. If `Win11QuickConverter_DevCert.pfx` is not in the current directory: create a self-signed code-signing certificate (`CN=MarcAntero`) in `CurrentUser\My`, export it to that PFX, and add it to `LocalMachine\Root`.
3. Copy manifest + assets into `AppxTemp`, run `makeappx pack /nv`, and delete the temp folder.
4. `signtool sign /fd SHA256` with the PFX.
5. Remove any existing `SnapFormat` package and `Add-AppxPackage -ExternalLocation <repo>\build\bin\Release`.

It must run elevated (`#Requires -RunAsAdministrator`). The script output messages are in Catalan.

### Release ZIP layout

The CI produces `SnapFormat.zip` with the layout the script expects:

```
SnapFormat.zip
├── install.bat
├── build\bin\Release\converter-cli.exe
├── build\bin\Release\shell-ext.dll
├── build\bin\Release\snapformat.ico
└── packaging\  (manifest, assets, scripts, installer.iss, PFX)
```

Because `install.bat` changes to the ZIP root and the script looks for the PFX in the current directory, a **new certificate is generated on each user machine** (the PFX under `packaging\` is not used).

## Signing certificate

> ⚠️ **Security note.** The development certificate's private key (`Win11QuickConverter_DevCert.pfx`, two copies) and its password (hard-coded in `register-msix.ps1`) are committed to a public repository. Any machine on which this exact certificate was added to *Trusted Root* will trust **any** code signed with it — including code signed by a third party who downloaded the PFX.

Recommendations (also listed in [Project review](../development/project-review.md)):

- Remove the PFX files from the repository and rotate the certificate; add `*.pfx` to `.gitignore`.
- Read the password from an environment variable / CI secret, or generate a throwaway certificate per machine (current ZIP behaviour) and do **not** add it to `LocalMachine\Root` — `TrustedPeople` is sufficient for MSIX sideloading.
- For public releases, sign with a real code-signing certificate (or Azure Trusted Signing), which also removes SmartScreen warnings for `SnapFormat_Setup.exe`.

## Versioning

Version numbers are currently set in three places and are not in sync:

| File | Value |
|---|---|
| `CMakeLists.txt` (`project VERSION`) | `0.1.0` |
| `packaging/installer.iss` (`AppVersion`) | `0.1.0` |
| `packaging/AppxManifest.xml` (`Identity Version`) | `1.0.0.0` |

Git tags go up to `v1.3.0`. When releasing, update all three to match the tag.
