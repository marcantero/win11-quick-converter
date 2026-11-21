# Troubleshooting

## The menu entry does not appear

1. **Restart Explorer** — the context menu is cached:
   ```powershell
   taskkill /f /im explorer.exe; start explorer
   ```
2. **Too many files** — the entry is hidden when more than 15 items are selected.
3. **Wrong file type** — the entry is registered for images only (perceived type `image`, plus `.webp`, `.heic`, `.avif`; with MSIX, the explicit list in the manifest).
4. **Look under "Show more options"** — with the registry (Inno Setup) install, some Windows builds only show `ExplorerCommandHandler` verbs in the classic menu (`Shift+F10`).
5. **Check registration**:
   ```powershell
   # Installer / regsvr32 path
   Get-Content "$env:TEMP\win11qc_register.log"      # expect HRESULT=0x00000000
   reg query "HKCU\Software\Classes\CLSID\{8D5F6D5B-18CB-4C7B-9E2D-6D8B125F551C}\InprocServer32"
   reg query "HKCU\Software\Classes\SystemFileAssociations\image\shell\snapformat.convert"

   # MSIX path
   Get-AppxPackage -Name SnapFormat
   ```
6. **Both installation methods present** — uninstall one of them.

## I click a format but no file is created

The menu does not report errors. Reproduce with the CLI to see the message and exit code:

```powershell
& "$env:APPDATA\SnapFormat\converter-cli.exe" --input "C:\path\image.heic" --output "C:\path\image.jpg" --format jpg
$LASTEXITCODE
```

| Symptom | Cause / fix |
|---|---|
| `Failed to open input image: 0x88982F50` (`WINCODEC_ERR_COMPONENTNOTFOUND`) | No WIC decoder for this file. Install HEIF Image Extensions (HEIC) or AV1 Video Extension (AVIF). |
| `Failed to open input image: 0x88982F07` (`WINCODEC_ERR_UNKNOWNIMAGEFORMAT`) | File is corrupt or its extension does not match the content. |
| `Failed to initialize output stream: 0x80070020` | Output file is in use (open in another app) or is the same file as the input (same-format conversion). |
| `Failed to initialize output stream: 0x80070005` | Access denied — read-only folder (e.g. `C:\Program Files`) or protected location. |
| Exit code `2`, `Input file does not exist` | The path contains characters outside the system code page; see [Unicode paths](../development/project-review.md#unicode-paths). |
| Nothing happens at all, even for simple PNGs | `converter-cli.exe` is missing next to `shell-ext.dll`. Reinstall. |

## `install.bat` fails

- **"No s'ha trobat el Windows SDK"** — the MSIX script needs `makeappx.exe`. Install the Windows SDK via the Visual Studio Installer, or use the `.exe` installer instead.
- **`Add-AppxPackage` deployment failed (0x800B0109)** — the signing certificate isn't trusted. Delete `Win11QuickConverter_DevCert.pfx` in the extracted folder and rerun so a new certificate is generated and trusted.
- **`0x80073CF3` / package conflicts** — remove the old package first: `Get-AppxPackage SnapFormat | Remove-AppxPackage`.

## Rebuild fails with "cannot open file shell-ext.dll"

Explorer (or `dllhost.exe` for MSIX) has the DLL loaded. Kill `explorer.exe` (and `dllhost.exe` if using MSIX) before building. See [Building](building.md#iterating-on-the-shell-extension).

## Uninstall leaves `shell-ext.dll` behind

The DLL was loaded by Explorer during uninstall. Sign out or restart, then delete `%APPDATA%\SnapFormat`.

## Reporting a bug

Open an issue at <https://github.com/marcantero/win11-quick-converter/issues> including: Windows build (`winver`), installation method, the input file type, the CLI output and exit code, and `%TEMP%\win11qc_register.log` if registration is involved.
