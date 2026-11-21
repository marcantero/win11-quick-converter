# Building from Source

## Prerequisites

| Tool | Version | Notes |
|---|---|---|
| Windows | 11 x64 | Windows 10 builds but the modern context menu requires 11. |
| Visual Studio | 2022 or later | Workload *Desktop development with C++* (MSVC + Windows 11 SDK). |
| CMake | ≥ 3.20 | Bundled with Visual Studio or from cmake.org. |
| Git | any | Needed by `FetchContent` to download libwebp. |
| Inno Setup | 6.x | Optional — only to build `SnapFormat_Setup.exe` (`iscc`). |
| Windows SDK tools | — | Optional — `makeappx`/`signtool` for the MSIX package. |

An internet connection is required on the first configure: CMake clones **libwebp v1.4.0** from `github.com/webmproject/libwebp`.

## Build

```powershell
git clone https://github.com/marcantero/win11-quick-converter.git
cd win11-quick-converter

cmake -S . -B build
cmake --build build --config Release
```

Use `--config Debug` for a debug build. The default generator on Windows is the installed Visual Studio version (multi-config).

## Outputs

| Path | Description |
|---|---|
| `build\bin\Release\converter-cli.exe` | Conversion engine |
| `build\bin\Release\shell-ext.dll` | Shell extension |
| `build\bin\Release\snapformat.ico` | Menu icon (post-build copy) |
| `build\tests\Release\shell_ext_test_com.exe` | COM activation smoke test |
| `build\lib\Release\` | Import libraries |

`converter-cli.exe`, `shell-ext.dll` and `snapformat.ico` must stay in the same folder — the DLL locates the other two relative to itself.

## Build configuration

- C++20, extensions off (`CMAKE_CXX_STANDARD 20`).
- MSVC flags: `/W4 /permissive-` on both targets.
- libwebp is configured with all command-line tools disabled (`WEBP_BUILD_* OFF`) and linked statically into `converter-cli`.
- Link libraries: `windowscodecs ole32 oleaut32 uuid webp` (CLI) and `advapi32 ole32 shell32 uuid` (DLL).

## Iterating on the shell extension

Explorer keeps `shell-ext.dll` loaded, so rebuilding fails with a "file in use" link error. Typical loop:

```powershell
taskkill /f /im explorer.exe
cmake --build build --config Release
regsvr32 /s build\bin\Release\shell-ext.dll
start explorer
```

## Packaging locally

```powershell
# Executable installer → build\installer\SnapFormat_Setup.exe
.\scripts\fetch-libreoffice.ps1 -SmokeTest   # once: bundled LibreOffice → build\libreoffice
iscc packaging\installer.iss

# MSIX sparse package (elevated PowerShell) → registers from build\bin\Release
powershell -ExecutionPolicy Bypass -File packaging\register-msix.ps1
```

See [Packaging](../deployment/packaging.md) for details.
