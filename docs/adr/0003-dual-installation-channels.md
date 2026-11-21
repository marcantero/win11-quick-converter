# ADR 0003: Ship Both an Inno Setup Installer and an MSIX Sparse Package

## Status

Accepted — recorded retroactively from issue #25 and commits `929ef70`, `206b643`.

## Context

Windows 11's modern context menu is designed around packaged apps: an `IExplorerCommand` handler declared in an MSIX manifest (`desktop4:FileExplorerContextMenus`). For an unpackaged Win32 app, a *sparse package* provides the required identity while keeping binaries in an external folder.

The sparse-package route proved heavy for end users: it needs the Windows SDK (`makeappx`, `signtool`), administrator rights, and a trusted signing certificate. A plain registry registration (`ExplorerCommandHandler` under `SystemFileAssociations`) via `DllRegisterServer` works per-user with no elevation.

## Decision

Publish two artifacts from CI on every tagged release:

1. **`SnapFormat_Setup.exe`** (Inno Setup, recommended) — per-user install to `%APPDATA%\SnapFormat`, registration through `DllRegisterServer` in `HKCU`.
2. **`SnapFormat.zip`** — binaries plus `install.bat`, which builds, signs and registers the MSIX sparse package locally.

## Consequences

- Users get a simple, no-admin installer by default.
- The MSIX path remains available for package-identity scenarios and future Store/Winget distribution.
- Registration metadata exists in two places (`DllRegisterServer` and `AppxManifest.xml`) and must be kept in sync when formats change.
- Installing both channels at once can produce duplicate menu entries.
- The MSIX path depends on a self-signed certificate; see the security notes in [Packaging](../deployment/packaging.md#signing-certificate).
