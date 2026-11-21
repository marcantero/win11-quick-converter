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
- [#37](https://github.com/marcantero/win11-quick-converter/issues/37) Native progress dialog for batch conversions — implemented in `ee774f6`, **pending push**; issue still open.

## Open feature requests

| Issue | Feature | Notes |
|---|---|---|
| [#36](https://github.com/marcantero/win11-quick-converter/issues/36) | Quality configuration and settings app | Would feed `--quality` from the menu. |
| [#38](https://github.com/marcantero/win11-quick-converter/issues/38) | Basic image resizing | Needs a CLI contract extension (e.g. `--max-size`) via a new ADR; WIC `IWICBitmapScaler`. |
| [#39](https://github.com/marcantero/win11-quick-converter/issues/39) | Toast notifications for batch conversions | Also solves silent failures. |
| [#40](https://github.com/marcantero/win11-quick-converter/issues/40) | FFmpeg integration for video and audio | Large scope; separate engine/process recommended. |
| [#41](https://github.com/marcantero/win11-quick-converter/issues/41) | Watch folders (auto-conversion service) | Can reuse the CLI as-is. |
| [#42](https://github.com/marcantero/win11-quick-converter/issues/42) | Publish to Winget | Requires stable versioning and ideally a signed installer. |

## Suggested next steps

1. Push `ee774f6` and close #37.
2. Address the 🔴 items in [Project review](development/project-review.md) (certificate, detached thread, same-format overwrite).
3. Unicode-safe CLI (`wmain`).
4. Automated CLI tests in CI.
5. Then #36 / #39, which build on a more robust error-reporting path.

## Releases

| Tag | Commit | Highlights (from commit history) |
|---|---|---|
| `v1.0.0` | `e5601f6` | CLI + WIC pipeline, COM shell extension, MSIX sparse package, CI publishing to Releases |
| `v1.1.0` | `238a995` | Logo/icon, installer executable |
| `v1.2.0` | `206b643` | WebP via libwebp, Inno Setup installer built in CI |
| `v1.3.0` | `4b77369` | HEIC and AVIF input |

> `v0.1.0` points to the same commit as `v1.2.0` (`206b643`), probably by mistake. Consider deleting it to avoid confusion.
