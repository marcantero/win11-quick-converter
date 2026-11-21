# Product Overview

**SnapFormat** adds a **"Convert with SnapFormat"** entry to the Windows 11 File Explorer context menu. Right-click one or more images, pick a target format, and the converted files appear next to the originals — no image editor required.

## Key features

- **Native Windows 11 context menu** — implemented as an `IExplorerCommand` COM handler, so it shows up in the modern (first-level) menu, with a submenu of target formats.
- **Headless conversion engine** — a small C++ command-line tool (`converter-cli.exe`) performs every conversion. It can also be used on its own from scripts.
- **Windows Imaging Component (WIC)** for decoding all inputs and for encoding PNG, JPEG, BMP and TIFF.
- **Google `libwebp`** for WebP encoding (lossless by default, lossy when a quality below 100 is requested).
- **Batch conversion** with a native Windows progress dialog that supports cancellation.
- **Selection guard** — the menu entry is hidden when more than 15 files are selected, to avoid overloading the system.
- **Two installation channels** — a per-user `.exe` installer (Inno Setup) or an MSIX sparse package.

## Supported formats

### Input (decoding)

Decoding is delegated to WIC, so any format with a WIC codec installed can be read. The context menu is registered for:

| Format | Extensions | Notes |
|---|---|---|
| JPEG | `.jpg`, `.jpeg` | Built into Windows. |
| PNG | `.png` | Built into Windows. |
| BMP | `.bmp` | Built into Windows. |
| TIFF | `.tif`, `.tiff` | Built into Windows. |
| WebP | `.webp` | WIC WebP decoder ships with Windows 11. |
| HEIC | `.heic` | Requires the **HEIF Image Extensions** from the Microsoft Store. |
| AVIF | `.avif` | Requires the **AV1 Video Extension** from the Microsoft Store. |

With the Inno Setup installer the menu is attached to the `image` perceived type, so it can also appear for other image types Windows knows about (for example `.gif` or `.ico`). Those usually convert fine as long as a WIC decoder exists; only the first frame is converted.

### Output (encoding)

| Format | Context menu entry | CLI `--format` values | Encoder | Quality support |
|---|---|---|---|---|
| PNG | To PNG | `png` | WIC | — |
| JPEG | To JPG | `jpg`, `jpeg` | WIC | Yes (`0–100`) |
| WebP | To WebP | `webp` | libwebp | Yes: `<100` lossy, otherwise lossless |
| BMP | To BMP | `bmp` | WIC | — |
| TIFF | To TIFF | `tif`, `tiff` | WIC | — |
| PDF (from office documents) | To PDF | `pdf` | LibreOffice ([ADR 0004](adr/0004-document-conversion-engines.md)) | — |

## System requirements

- Windows 11 (build 22000 or later), x64.
- For HEIC/AVIF input: the corresponding Microsoft Store codec extensions.
- For building from source: Visual Studio with the *Desktop development with C++* workload (MSVC, Windows SDK) and CMake 3.20+. See [Building from source](guides/building.md).

## Limitations

- Only the **first frame** of multi-frame images (animated GIF/WebP, multi-page TIFF) is converted.
- Metadata (EXIF, ICC profiles) is **not** copied to the output.
- The context menu always uses default encoder settings (JPEG default quality, lossless WebP); custom quality is only available from the CLI. A settings app is tracked in [#36](https://github.com/marcantero/win11-quick-converter/issues/36).
- Existing output files with the same name are **overwritten** without warning.
- Selections of more than 15 files hide the menu entry.

See [Project review](development/project-review.md) for the full list of known issues.
