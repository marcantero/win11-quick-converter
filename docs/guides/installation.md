# Installation

SnapFormat can be installed in two ways. Use **one** of them — installing both registers the same CLSID twice and can produce duplicate menu entries.

| | Executable installer | MSIX sparse package |
|---|---|---|
| File | `SnapFormat_Setup.exe` | `SnapFormat.zip` |
| Admin rights | No (per-user) | Yes |
| Install location | `%APPDATA%\SnapFormat` | Wherever you extract the ZIP |
| Registration | `DllRegisterServer` → `HKCU` registry | Signed MSIX identity package |
| Uninstall | *Settings → Apps* or Start menu shortcut | `Get-AppxPackage SnapFormat \| Remove-AppxPackage` |
| Recommended | ✅ | For testing package identity |

Both artifacts are published on the [Releases page](https://github.com/marcantero/win11-quick-converter/releases).

## Prerequisites

- Windows 11 x64 (build 22000+).
- Optional, for HEIC input: [HEIF Image Extensions](https://apps.microsoft.com/detail/9PMMSR1CGPWG).
- Optional, for AVIF input: [AV1 Video Extension](https://apps.microsoft.com/detail/9MVZQVXJBQ9V).

## Method 1 — Executable installer (recommended)

1. Download `SnapFormat_Setup.exe`.
2. Run it. Windows SmartScreen may warn because the installer is not code-signed; choose *More info → Run anyway*.
3. Finish the wizard. The installer copies `converter-cli.exe`, `shell-ext.dll` and `snapformat.ico` to `%APPDATA%\SnapFormat` and registers the DLL.
4. Right-click an image — **Convert with SnapFormat** should appear. If not, restart Explorer (see [Troubleshooting](troubleshooting.md)).

### Uninstall

Use *Settings → Apps → Installed apps → SnapFormat → Uninstall* or the *Uninstall SnapFormat* Start menu shortcut. The uninstaller calls `DllUnregisterServer`. If Explorer still has the DLL loaded, the file may be removed only after sign-out/restart.

## Method 2 — MSIX sparse package

1. Download `SnapFormat.zip` and extract it to a **permanent** folder (the package points at the binaries in place — do not delete or move the folder afterwards).
2. Run `install.bat`. It requests elevation and runs `packaging\register-msix.ps1`, which:
   1. Locates `makeappx.exe` / `signtool.exe` in the Windows 10/11 SDK — **the Windows SDK must be installed** (via the Visual Studio Installer).
   2. Packs `SnapFormat.msix` from `packaging\AppxManifest.xml` and signs it with a throwaway certificate whose private key is deleted right after signing.
   3. Adds only the public certificate to *Local Machine → Trusted People* (not Trusted Root).
   4. Removes any previous `SnapFormat` package and registers the new one with `-ExternalLocation <folder>\build\bin\Release`.
3. Right-click an image to verify.

> ⚠️ If you installed SnapFormat with `install.bat` before v1.4, a development certificate was added to *Trusted Root*. Running the new `install.bat` removes it; see [Packaging](../deployment/packaging.md#legacy-certificates-before-v14) to remove it manually.

### Uninstall

```powershell
Get-AppxPackage -Name SnapFormat | Remove-AppxPackage
```

Optionally remove the `CN=MarcAntero` certificate from `certlm.msc` → *Trusted Root Certification Authorities* and from `certmgr.msc` → *Personal*.

## Manual registration (developers)

After [building from source](building.md):

```powershell
regsvr32 build\bin\Release\shell-ext.dll        # register (HKCU)
regsvr32 /u build\bin\Release\shell-ext.dll     # unregister
```

`converter-cli.exe` must be in the same folder as `shell-ext.dll`.
