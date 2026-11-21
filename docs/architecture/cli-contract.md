# CLI Contract

`converter-cli.exe` is the headless conversion engine. The shell extension invokes it once per file, and it can be used directly from a terminal or script. The contract was decided in [ADR 0001](../adr/0001-cli-contract.md); changes must stay backward compatible.

Source: `src/converter-cli/src/main.cpp`, `src/converter-cli/include/win11qc/cli_options.h`.

The input extension picks the path: office documents (`.doc`, `.docx`, `.docm`, `.dot`, `.dotx`, `.odt`, `.ott`, `.rtf`, `.xls`, `.xlsx`, `.xlsm`, `.ods`, `.ppt`, `.pptx`, `.pptm`, `.pps`, `.ppsx`, `.odp`) go to the document converter, everything else to the image converter.

## Synopsis

```
converter-cli --input <path> --output <path> --format <name> [--quality <0-100>]
converter-cli --help
```

## Options

| Option | Required | Description |
|---|---|---|
| `--input <path>` | Yes | Source image or office document. Must exist and be a regular file. Any WIC-decodable image is accepted. |
| `--output <path>` | Yes | Destination file. Missing parent directories are created. An existing file is overwritten. |
| `--format <name>` | Yes | Target format, case-insensitive. Images: `png`, `jpg`, `jpeg`, `bmp`, `tif`, `tiff`, `webp`. Documents: `pdf`. The output extension is **not** inferred — this option decides the encoder. |
| `--quality <0-100>` | No | Integer. JPEG: maps to WIC `ImageQuality` (`q/100`). WebP: `<100` → lossy at that quality, `100` or omitted → lossless. Ignored for PNG, BMP and TIFF. |
| `--help`, `-h` | No | Print usage and exit with `0`. Takes precedence over all other options (other options are still parsed and validated syntactically). |

Rules:

- Options can appear in any order. If an option is repeated, the last value wins.
- Any unknown argument is an error (exit `1`).
- Paths are received as narrow `argv` strings (active code page). See [Project review](../development/project-review.md#unicode-paths) for the Unicode caveat.

## Exit codes

| Code | Name (`win11qc::ExitCode`) | Meaning |
|---|---|---|
| `0` | `Success` | Output written (or `--help` shown). |
| `1` | `InvalidArguments` | Unknown option, missing option value, missing required option, or invalid `--quality`. |
| `2` | `ValidationFailed` | Unsupported `--format` (including a document to anything but `pdf`), input and output are the same file, input missing / not a regular file, or output directory could not be created. |
| `3` | `ConversionFailed` | COM/WIC/libwebp failure while decoding, converting, encoding or writing; for documents, LibreOffice not found, failed, or timed out. |

## Output

- On success, a single line is printed to **stdout**, e.g.
  `Converted C:\img\a.jpg to C:\img\a.png using PNG encoding.`
- Validation and conversion failure messages are also printed to **stdout** (WIC errors include the `HRESULT`, e.g. `Failed to open input image: 0x88982F50`).
- Argument parsing errors are printed to **stderr**, followed by `Run with --help for usage information.`

## Examples

```powershell
# PNG → JPEG at quality 85
converter-cli --input photo.png --output photo.jpg --format jpg --quality 85

# HEIC → lossless WebP
converter-cli --input IMG_0001.heic --output IMG_0001.webp --format webp

# Lossy WebP into a new folder
converter-cli --input scan.tiff --output out\scan.webp --format webp --quality 75

# Word document → PDF (needs LibreOffice, see ADR 0004)
converter-cli --input report.docx --output report.pdf --format pdf

# Check the result in a script
converter-cli --input a.bmp --output a.png --format png
if ($LASTEXITCODE -ne 0) { Write-Error "Conversion failed ($LASTEXITCODE)" }
```

## Environment variables

Development and test overrides for the document path. They are not part of the contract.

| Variable | Effect |
|---|---|
| `SNAPFORMAT_SOFFICE` | Full path of the `soffice.exe` to use instead of searching for LibreOffice. |
| `SNAPFORMAT_SOFFICE_TIMEOUT` | Timeout in seconds for one document conversion (default `120`). |
