# Usage

## From File Explorer

1. Select one or more images or office documents (up to **15**).
2. Right-click → **Convert with SnapFormat**.
3. Choose a target:
   - Images: **To PNG**, **To JPG**, **To WebP**, **To BMP** or **To TIFF**.
   - Office documents (Word, Excel, PowerPoint and OpenDocument files): **To PDF**.

The converted files are written **next to the originals**, with the same base name and the new extension:

```
holiday.heic  ──To JPG──►  holiday.jpg
logo.png      ──To WebP─►  logo.webp
report.docx   ──To PDF──►  report.pdf
```

### Documents and mixed selections

- Documents only offer **To PDF**, and images only offer image formats.
- Supported documents: `.doc .docx .docm .dot .dotx .odt .ott .rtf .xls .xlsx .xlsm .ods .ppt .pptx .pptm .pps .ppsx .odp`.
- When the selection mixes images and documents, the menu shows both groups and each entry converts only the files it applies to: **To PDF** converts the documents and leaves the images alone, **To PNG** converts the images and leaves the documents alone.
- Document conversion uses LibreOffice, which takes a few seconds per file. It needs the *Document conversion* component of the installer or LibreOffice installed on the machine; otherwise the notification says "LibreOffice was not found".

### Single file vs. batch

- **One file** — conversion runs in the background with no window. The new file appears in Explorer when finished (usually under a second).
- **Several files** — a Windows progress dialog shows the current file and overall progress. Files are converted one at a time. Click **Cancel** to stop after the current file; files already converted are kept.

### Things to know

- **Overwrites**: if `holiday.jpg` already exists it is replaced without asking.
- **Same format**: choosing the format the file already has (e.g. *To PNG* on a `.png`) targets the same path as the input and will fail; avoid it. `.jpeg → To JPG` and `.tif → To TIFF` create a new file with the other extension.
- **Encoder settings** are fixed in the menu: JPEG uses the WIC default quality, WebP is **lossless**. Use the CLI for custom quality.
- **Transparency** is preserved for PNG, WebP, BMP and TIFF, and discarded (flattened) for JPEG.
- **Animated / multi-page** files: only the first frame/page is converted.
- **More than 15 files** selected: the menu entry is hidden. Convert in smaller groups.
- **Errors are silent** in the menu: if a file can't be converted (e.g. HEIC without the codec), no output appears. Run the CLI manually to see the error — see [Troubleshooting](troubleshooting.md).

## From the command line

`converter-cli.exe` lives next to the DLL (`%APPDATA%\SnapFormat` with the installer).

```powershell
$cli = "$env:APPDATA\SnapFormat\converter-cli.exe"

& $cli --input photo.png --output photo.jpg --format jpg --quality 90
& $cli --input photo.png --output photo.webp --format webp --quality 80   # lossy
& $cli --input photo.png --output photo.webp --format webp               # lossless
```

Batch conversion of a folder in PowerShell:

```powershell
Get-ChildItem *.heic | ForEach-Object {
    & $cli --input $_.FullName --output ([IO.Path]::ChangeExtension($_.FullName, 'jpg')) --format jpg --quality 85
}
```

Full reference: [CLI contract](../architecture/cli-contract.md).
