# win11-quick-converter

win11-quick-converter is a native Windows 11 image conversion toolchain built as a monorepo.

## Repository layout

- `src/converter-cli`: Headless C++ conversion engine and command-line contract.
- `src/shell-ext`: In-proc COM shell extension that will expose the Windows 11 context menu entry.
- `docs`: Architecture notes and implementation records.

## Current status

The initial implementation focuses on the CLI contract and repository structure.
The conversion engine and shell extension will be implemented incrementally.

## Build

This repository uses CMake.

```bash
cmake -S . -B build
cmake --build build --config Release
```
