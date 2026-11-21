# Shell Extension

`shell-ext.dll` is an in-process COM server implementing the Windows 11 modern context-menu contract (`IExplorerCommand`). It is built with WRL and contains no image-processing code.

Sources:

- `src/shell-ext/include/win11qc/shell_extension.h` — CLSID, verb and display strings.
- `src/shell-ext/src/dummy.cpp` — all COM classes, registration and CLI invocation (the filename is historical; it is the real implementation).
- `src/shell-ext/shell-ext.def` — exports `DllRegisterServer`, `DllUnregisterServer`, `DllGetClassObject`, `DllCanUnloadNow`.

## COM classes

| Class | Interface | Role |
|---|---|---|
| `QuickConvertClassFactory` | `IClassFactory` | Returned by `DllGetClassObject` for the SnapFormat CLSID; creates `QuickConvertCommand`. No aggregation. |
| `QuickConvertCommand` | `IExplorerCommand` | Top-level item "Convert with SnapFormat". Flags `ECF_HASSUBCOMMANDS`. Its own `Invoke` does nothing. |
| `CommandEnumerator` | `IEnumExplorerCommand` | Enumerates the format subcommands. |
| `FormatSubCommand` | `IExplorerCommand` | One per target format (`png`, `jpg`, `webp`, `bmp`, `tiff`). Performs the conversion on `Invoke`. |

Each object increments a global object counter on construction; `DllCanUnloadNow` returns `S_OK` only when both the object count and the `LockServer` count are zero.

### `QuickConvertCommand`

- `GetTitle` → `Convert with SnapFormat`
- `GetToolTip` → `Convert selected images using SnapFormat.`
- `GetIcon` → `snapformat.ico` located next to the DLL
- `GetCanonicalName` → the CLSID
- `GetState` → `ECS_HIDDEN` if there is no selection or **more than 15 items**, otherwise `ECS_ENABLED`
- `EnumSubCommands` → `To PNG`, `To JPG`, `To WebP`, `To BMP`, `To TIFF`

### `FormatSubCommand::Invoke`

1. Resolve `converter-cli.exe` in the same directory as the DLL (`GetModuleFileNameW`). If it is missing, return `ERROR_FILE_NOT_FOUND`.
2. Collect file-system paths for the selection (`SIGDN_FILESYSPATH`); items without one (e.g. virtual folders) are skipped.
3. Output path = input path with its extension replaced by the target (`photo.png` → `photo.jpg`).
4. Command line:
   `"<dir>\converter-cli.exe" --input "<in>" --output "<out>" --format <fmt>`
5. Execution:
   - **One file** — `CreateProcessW(..., CREATE_NO_WINDOW)` and return immediately (fire and forget).
   - **Several files** — start a detached `std::thread` (STA) that shows a native `IProgressDialog` ("SnapFormat Conversion"), runs the CLI **sequentially** for each file waiting for it to exit, updates the progress bar/current file name, and stops early if the user clicks *Cancel*.

The DLL does not inspect the CLI exit code or report failures to the user.

## Registration

There are two registration mechanisms, one per installation channel.

### `DllRegisterServer` (Inno Setup / `regsvr32`)

Writes **per-user** keys under `HKEY_CURRENT_USER\Software\Classes` (no admin rights needed):

```
CLSID\{8D5F6D5B-18CB-4C7B-9E2D-6D8B125F551C}
    (Default)          = Convert with SnapFormat
    InprocServer32
        (Default)      = <path>\shell-ext.dll
        ThreadingModel = Apartment

SystemFileAssociations\image\shell\snapformat.convert
SystemFileAssociations\.webp\shell\snapformat.convert
SystemFileAssociations\.heic\shell\snapformat.convert
SystemFileAssociations\.avif\shell\snapformat.convert
    (Default)               = Convert with SnapFormat
    MUIVerb                 = Convert with SnapFormat
    Icon                    = <path>\shell-ext.dll,-101   (embedded snapformat.ico, IDI_SNAPFORMAT)
    ExplorerCommandHandler  = {8D5F6D5B-...}
```

`image` is the *perceived type* key, which covers JPEG, PNG, BMP, TIFF, GIF and others. WebP, HEIC and AVIF are registered explicitly because their perceived type is not reliably `image` on every system.

On success it calls `SHChangeNotify(SHCNE_ASSOCCHANGED)` and always writes the resulting `HRESULT` to `%TEMP%\win11qc_register.log` (UTF-16) for diagnostics.

`DllUnregisterServer` deletes the same key trees.

### MSIX sparse package (`packaging/AppxManifest.xml`)

The manifest declares:

- `desktop4:FileExplorerContextMenus` with verb `snapFormatConvert` → CLSID, for `.jpg .jpeg .png .webp .bmp .tif .tiff .heic .avif`.
- `com:SurrogateServer` with the class `shell-ext.dll`, `ThreadingModel="STA"`.
- `uap10:AllowExternalContent` so binaries live in an external folder (`build\bin\Release` relative to the extracted ZIP).

This path does not call `DllRegisterServer`. See [Packaging](../deployment/packaging.md).

## Adding a new menu format

Add a line in `QuickConvertCommand::EnumSubCommands()`:

```cpp
formats.push_back(Make<FormatSubCommand>(L"<cli-format>", L"To <NAME>"));
```

The first argument is used both as the CLI `--format` value and as the output file extension, so it must be a value the CLI accepts.

## Adding a new input extension

- Registry path: add the extension to both `specificExts` arrays (register and unregister) if it is not covered by the `image` perceived type.
- MSIX path: add a `desktop5:ItemType` entry to `AppxManifest.xml`.

## Debugging tips

- Explorer caches shell extensions. After rebuilding, restart Explorer (`taskkill /f /im explorer.exe & start explorer`) or the DLL will stay locked/stale.
- Run `build\tests\Release\shell_ext_test_com.exe` to check COM activation without Explorer (see [Testing](../development/testing.md)).
- Check `%TEMP%\win11qc_register.log` after `regsvr32`.
