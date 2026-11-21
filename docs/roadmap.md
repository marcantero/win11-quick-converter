# Roadmap

Status of the GitHub issues and milestones (snapshot: November 2026). The live board is the GitHub Project *Win11 Quick Converter - Kanban*.

## Delivered

### M1 — Conversion Engine (CLI Core)
- [#20](https://github.com/marcantero/win11-quick-converter/issues/20) Define CLI architecture and command contract — ✅
- [#21](https://github.com/marcantero/win11-quick-converter/issues/21) Implement WIC-based conversion pipeline — ✅

### M2 — Native Win11 Extension
- [#22](https://github.com/marcantero/win11-quick-converter/issues/22) Scaffold COM in-proc server & IExplorerCommand — ✅
- [#23](https://github.com/marcantero/win11-quick-converter/issues/23) Context menu behavior and selection filtering — ✅

### M3 — Integration & Packaging
- [#24](https://github.com/marcantero/win11-quick-converter/issues/24) Integrate COM invocation with CLI execution — ✅
- [#25](https://github.com/marcantero/win11-quick-converter/issues/25) Author MSIX Sparse Package (AppxManifest) — ✅

### M4 — CI/CD & Automation
- [#26](https://github.com/marcantero/win11-quick-converter/issues/26) GitHub Actions C++ build workflow — ✅

### Post-M4 features
- WebP output via libwebp, Inno Setup installer, release pipeline for `SnapFormat_Setup.exe`.
- [#35](https://github.com/marcantero/win11-quick-converter/issues/35) HEIC and AVIF input — ✅
- [#37](https://github.com/marcantero/win11-quick-converter/issues/37) Native progress dialog for batch conversions — ✅ (`ee774f6`, not yet in a tagged release)

## Planned work

Issues are grouped into milestones and labelled by `type:`, `area:`, `priority:` and `size:`.

### M5 — v1.4 Robustness
Fix security and stability problems before adding features.

| Issue | Title | Priority |
|---|---|---|
| [#43](https://github.com/marcantero/win11-quick-converter/issues/43) | Remove development certificate private key from the repository | high |
| [#44](https://github.com/marcantero/win11-quick-converter/issues/44) | Detached batch thread can outlive the shell extension DLL | high |
| [#45](https://github.com/marcantero/win11-quick-converter/issues/45) | Same-format conversion overwrites or corrupts the source file | high |
| [#46](https://github.com/marcantero/win11-quick-converter/issues/46) | File names with non-ANSI characters fail to convert | high |
| [#39](https://github.com/marcantero/win11-quick-converter/issues/39) | Toast notifications / visible error reporting | high |
| [#47](https://github.com/marcantero/win11-quick-converter/issues/47) | Unchecked HRESULTs and size overflow in the WebP path | medium |
| [#48](https://github.com/marcantero/win11-quick-converter/issues/48) | Never overwrite existing files silently | medium |
| [#49](https://github.com/marcantero/win11-quick-converter/issues/49) | Automated CLI tests in CI | medium |
| [#50](https://github.com/marcantero/win11-quick-converter/issues/50) | Repository cleanup | low |

### M6 — v1.5 Power features

| Issue | Title | Priority |
|---|---|---|
| [#36](https://github.com/marcantero/win11-quick-converter/issues/36) | Quality configuration and settings app | medium |
| [#38](https://github.com/marcantero/win11-quick-converter/issues/38) | Basic image resizing | medium |
| [#51](https://github.com/marcantero/win11-quick-converter/issues/51) | Preserve EXIF orientation and metadata | medium |
| [#52](https://github.com/marcantero/win11-quick-converter/issues/52) | Remove the 15-file limit and convert batches in parallel | medium |
| [#53](https://github.com/marcantero/win11-quick-converter/issues/53) | AVIF and HEIC output | low |

### M7 — Distribution & ecosystem

| Issue | Title | Priority |
|---|---|---|
| [#54](https://github.com/marcantero/win11-quick-converter/issues/54) | Derive the version number from a single source | medium |
| [#55](https://github.com/marcantero/win11-quick-converter/issues/55) | Code-sign binaries and installer | medium |
| [#42](https://github.com/marcantero/win11-quick-converter/issues/42) | Publish to Winget (blocked by #54, #55) | medium |
| [#56](https://github.com/marcantero/win11-quick-converter/issues/56) | Installer polish | low |
| [#41](https://github.com/marcantero/win11-quick-converter/issues/41) | Watch folders (auto-conversion service) | low |
| [#40](https://github.com/marcantero/win11-quick-converter/issues/40) | FFmpeg integration for video and audio | low |

### Document conversion

Office documents to PDF, decided in [ADR 0004](adr/0004-document-conversion-engines.md).

| Issue | Title | Priority |
|---|---|---|
| [#58](https://github.com/marcantero/win11-quick-converter/issues/58) | CLI: convert office documents to PDF with LibreOffice — ✅ | medium |
| [#59](https://github.com/marcantero/win11-quick-converter/issues/59) | Use Microsoft Word when installed | medium |
| [#60](https://github.com/marcantero/win11-quick-converter/issues/60) | Context menu and registration for office documents — ✅ | medium |
| [#61](https://github.com/marcantero/win11-quick-converter/issues/61) | Bundle LibreOffice in the installer — ✅ | medium |

## Releases

| Tag | Commit | Highlights (from commit history) |
|---|---|---|
| `v1.0.0` | `e5601f6` | CLI + WIC pipeline, COM shell extension, MSIX sparse package, CI publishing to Releases |
| `v1.1.0` | `238a995` | Logo/icon, installer executable |
| `v1.2.0` | `206b643` | WebP via libwebp, Inno Setup installer built in CI |
| `v1.3.0` | `4b77369` | HEIC and AVIF input |
| `v1.4.0` | see tag | Office documents to PDF with bundled LibreOffice, progress dialog, toast notifications, Unicode file names, security and robustness fixes |

> `v0.1.0` points to the same commit as `v1.2.0` (`206b643`), probably by mistake. Consider deleting it to avoid confusion.
