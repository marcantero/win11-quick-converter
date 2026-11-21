# Conversion Pipeline

Source: `src/converter-cli/src/wic_converter.cpp` (images) and `src/converter-cli/src/document_converter.cpp` (documents).

`main.cpp` sends office documents (by input extension) to `convert_document()` and everything else to `convert_image()`.

`convert_image()` dispatches on `--format`:

- `webp` → `convert_to_webp()` (WIC decode + libwebp encode)
- `png`, `jpg`/`jpeg`, `bmp`, `tif`/`tiff` → `convert_with_wic()` (WIC decode + WIC encode)
- anything else → exit `2` with the list of supported formats

Both paths only convert **frame 0** of the source and do not copy metadata.

## WIC path (`convert_with_wic`)

```
validate input ─► ensure output dir ─► CoInitializeEx(MTA)
   ─► IWICImagingFactory
   ─► CreateDecoderFromFilename(input) ─► GetFrame(0) ─► GetSize
   ─► IWICStream::InitializeFromFilename(output, GENERIC_WRITE)
   ─► CreateEncoder(container) ─► Initialize(stream)
   ─► CreateNewFrame(+ IPropertyBag2) ─► [JPEG: ImageQuality]
   ─► frame Initialize / SetSize / SetPixelFormat
   ─► IWICFormatConverter(source → target pixel format)
   ─► WriteSource ─► frame Commit ─► encoder Commit
```

Target pixel formats come from `resolve_container_format()`:

| Format | WIC container GUID | Pixel format | Quality |
|---|---|---|---|
| PNG | `GUID_ContainerFormatPng` | 32bpp BGRA (keeps alpha) | No |
| JPEG | `GUID_ContainerFormatJpeg` | 24bpp BGR (alpha dropped) | Yes |
| BMP | `GUID_ContainerFormatBmp` | 32bpp BGRA | No |
| TIFF | `GUID_ContainerFormatTiff` | 32bpp BGRA | No |

`SetPixelFormat` may adjust the requested format to the closest one the encoder supports; the converter is initialised with the adjusted value. Every `HRESULT` is checked and reported as `0xXXXXXXXX` in the error message.

## WebP path (`convert_to_webp`)

WIC's WebP support on Windows is decode-only, so encoding uses libwebp ([ADR 0002](../adr/0002-libwebp-for-webp-encoding.md)):

1. Validate input and output directory, initialise COM, create the WIC factory.
2. Decode frame 0 with WIC and convert it to **32bpp RGBA**.
3. Copy the pixels into a `std::vector<uint8_t>` (`width * height * 4` bytes).
4. Encode:
   - `--quality` given and `< 100` → `WebPEncodeRGBA(..., quality)` (lossy)
   - otherwise → `WebPEncodeLosslessRGBA(...)`
5. Write the buffer with `std::ofstream` and release it with `WebPFree`.

The whole decoded image is held in memory, so very large images need `4 × width × height` bytes of RAM plus the encoder's working set.

## Document path (`convert_document`)

Office documents convert to PDF with LibreOffice ([ADR 0004](../adr/0004-document-conversion-engines.md)):

```
validate (--format pdf, not the same file, input exists) ─► ensure output dir
   ─► find soffice.exe: SNAPFORMAT_SOFFICE ─► {cli dir}\libreoffice\program
                        ─► registry UNO\InstallPath ─► %ProgramFiles%\LibreOffice
   ─► lock the "Local\SnapFormat.DocumentConversion" mutex
   ─► prepare the private profile (%LOCALAPPDATA%\SnapFormat\libreoffice-profile, macros disabled)
   ─► soffice -env:UserInstallation=<profile> --headless --convert-to pdf --outdir <temp dir> <input>
        (no console, output to NUL, inside a job object, 120 s timeout; exit 81 = restart, retried once)
   ─► find the PDF in the temp dir ─► move it next to the output ─► rename over the output
```

- The job object kills LibreOffice and every helper process on timeout, and when the CLI returns.
- The temporary directory is always removed. A failed conversion leaves no output and keeps an existing output file unchanged.
- Tests use `tests/fake_soffice.cpp`, a stand-in `soffice.exe`, so CI does not need LibreOffice.

## Resource management

- `ScopedComInitialization` (RAII) pairs `CoInitializeEx` / `CoUninitialize`.
- All COM pointers are `Microsoft::WRL::ComPtr`.
- libwebp output buffers are released with `WebPFree` on every path.

## Extending with a new output format

1. If WIC has an encoder for it, add an entry in `resolve_container_format()` (container GUID, pixel format, quality support). Otherwise add a dedicated `convert_to_<fmt>()` path and dispatch it in `convert_image()`.
2. Update the "Unsupported output format" message.
3. Add a `FormatSubCommand` in `QuickConvertCommand::EnumSubCommands()` (`src/shell-ext/src/dummy.cpp`).
4. Update [CLI contract](cli-contract.md) and [Product overview](../overview.md).

Adding a new **input** format only requires a WIC decoder on the machine plus menu registration — see [Shell extension → Registration](shell-extension.md#registration).
