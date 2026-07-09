# win11-quick-converter

win11-quick-converter is a native Windows 11 image conversion toolchain built as a monorepo.

## Repository layout

- `src/converter-cli`: Headless C++ conversion engine and command-line contract.
- `src/shell-ext`: In-proc COM shell extension that will expose the Windows 11 context menu entry.
- `docs`: Architecture notes and implementation records.

## Current status

The CLI contract and WIC-based conversion engine are implemented.
The shell extension now registers as an `IExplorerCommand` and launches the CLI, but selection filtering and package deployment are still pending.
The remaining tracked work is centered on context-menu filtering, MSIX sparse packaging, and CI automation.

## Build

This repository uses CMake.

```bash
cmake -S . -B build
cmake --build build --config Release
```
