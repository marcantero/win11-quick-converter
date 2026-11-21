# ADR 0004: Document-to-PDF Conversion Engines

## Status

Accepted

## Context

Users want to convert office documents (`.docx`, `.doc`, `.odt`, `.rtf`, `.xlsx`, `.pptx`, …) to PDF from the context menu ([#58](https://github.com/marcantero/win11-quick-converter/issues/58)). Three requirements shape the choice:

- **Fidelity.** The PDF must look like the original document: page layout, page breaks, headers and footers, tables, text boxes.
- **No prerequisites.** Users must not need Microsoft Word or any other program installed.
- **Local only.** Documents never leave the user's machine.

Windows has no built-in engine that reads office documents (WIC covers images only). The options considered:

| Option | Fidelity | Notes |
|---|---|---|
| Microsoft Word via COM | Exact | Only when Office is installed. |
| LibreOffice (`soffice --headless --convert-to pdf`) | High | Free (MPL-2.0), redistributable, handles every office format. Adds ~200–250 MB. |
| WebView2 + `docx-preview`, `PrintToPdf` | Medium | Tiny and built into Windows 11, but layout is approximate and only `.docx` is supported. |
| Pandoc + Typst | Low | Keeps content, not layout. ~150 MB, GPL. |
| Commercial libraries (Aspose, Apryse, Syncfusion) | High | Licences cost thousands per year. |
| Cloud APIs (CloudConvert, Microsoft Graph) | High | Uploads private documents, needs a network, an API key and per-use payment. |

## Decision

- **LibreOffice is the engine that is always available.** The installer ships it as an optional component, checked by default ([#61](https://github.com/marcantero/win11-quick-converter/issues/61)), in `{app}\libreoffice\`.
- **Word is preferred when installed** ([#59](https://github.com/marcantero/win11-quick-converter/issues/59)), because it renders its own documents exactly. If Word is missing or fails, the CLI falls back to LibreOffice. Detection (`CLSIDFromProgID`) is a registry lookup that costs well under a millisecond, so it runs on every conversion and is not cached.
- Both engines run **inside `converter-cli.exe`**, behind the existing CLI contract ([ADR 0001](0001-cli-contract.md)): `--format pdf` with a document input. The shell extension only adds menu entries ([#60](https://github.com/marcantero/win11-quick-converter/issues/60)).
- LibreOffice lookup order: the `SNAPFORMAT_SOFFICE` override (development and tests), the bundled copy next to the CLI, then a system install (registry `HKLM\SOFTWARE\LibreOffice\UNO\InstallPath`, then `%ProgramFiles%`).
- LibreOffice runs with a **private profile** in `%LOCALAPPDATA%\SnapFormat\libreoffice-profile`, so it never touches or collides with the user's own LibreOffice. The profile **disables macros**.
- Conversions are **serialized** with a named mutex, because one LibreOffice profile cannot run two conversions at once.
- Every LibreOffice process runs in a **job object** with a timeout (120 s), so a hung conversion is killed together with any helper processes.
- The PDF is written to a temporary directory and only moved over the output path when the conversion succeeded, so a failed conversion never leaves a partial file and never damages an existing one.

The rejected options stay rejected for the reasons in the table: WebView2 and Pandoc do not meet the fidelity requirement, commercial libraries are too expensive for this project, and cloud APIs would send private documents off the machine.

## Consequences

- Document conversion works on every machine with the default install, with no other software.
- The installer grows by roughly 100+ MB to download and 200–250 MB on disk when the component is selected.
- The same document can produce slightly different PDFs depending on whether Word is installed. Conversion messages name the engine that was used.
- Fidelity also depends on fonts. A document that uses a font missing on the machine (for example **Aptos**, the Office default since 2023, which does not ship with Windows) is rendered with a substitute, which can shift text between pages. Fonts like Aptos cannot be redistributed.
- LibreOffice starts in 2–5 s, so document conversion is much slower than image conversion. Batches should reuse one engine instance where possible.
- Releases must ship LibreOffice's MPL-2.0 licence notices and must not rebrand it. The bundled version has to be updated for security fixes.
- Word automation must keep macros disabled (`AutomationSecurity = msoAutomationSecurityForceDisable`) and must never `Quit` a Word instance it did not start.
