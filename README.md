Aquí tens tot el contingut necessari per a la teva **SnapFormat** (versió 1.1.0) a punt per copiar i enganxar.

### 1. `.github/workflows/build.yml`

Substitueix el contingut del teu fitxer actual per aquest:

```yaml
name: Windows C++ Build & Release

on:
  push:
    branches: [ "main" ]
    tags:
      - 'v*'
  pull_request:
    branches: [ "main" ]

jobs:
  build:
    runs-on: windows-latest
    permissions:
      contents: write

    steps:
    - name: Checkout repository
      uses: actions/checkout@v4

    - name: Configure CMake
      run: cmake -S . -B build

    - name: Build Project
      run: cmake --build build --config Release

    - name: Prepare Release Structure
      run: |
        New-Item -ItemType Directory -Force -Path ReleaseDir\build\bin\Release
        New-Item -ItemType Directory -Force -Path ReleaseDir\packaging
        
        Copy-Item build\bin\Release\converter-cli.exe ReleaseDir\build\bin\Release\
        Copy-Item build\bin\Release\shell-ext.dll ReleaseDir\build\bin\Release\
        Copy-Item build\bin\Release\snapformat.ico ReleaseDir\build\bin\Release\
        
        Copy-Item -Recurse packaging\* ReleaseDir\packaging\
        
        Copy-Item install.bat ReleaseDir\

    - name: Create ZIP file
      run: Compress-Archive -Path ReleaseDir\* -DestinationPath SnapFormat.zip

    - name: Upload Artifact
      uses: actions/upload-artifact@v4
      with:
        name: SnapFormat-Binaries
        path: SnapFormat.zip

    - name: Publish GitHub Release
      if: startsWith(github.ref, 'refs/tags/')
      uses: softprops/action-gh-release@v2
      with:
        files: SnapFormat.zip
        generate_release_notes: true
      env:
        GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}

```

---

### 2. `install.bat` (Crea aquest fitxer a l'arrel)

Aquest és l'script d'instal·lació automàtica per als teus usuaris:

```bat
@echo off
color 0B
echo ==========================================
echo    SnapFormat Installer (Windows 11)
echo ==========================================
echo.

:: 1. Check for Administrator privileges
NET SESSION >nul 2>&1
if %errorLevel% == 0 (
    goto :admin
) else (
    echo Requesting administrator privileges...
    powershell -Command "Start-Process '%~dpnx0' -Verb RunAs"
    exit /B
)

:admin
:: 2. Set the directory to the location of the script
cd /d "%~dp0"

echo Registering the package and certificates in Windows...
powershell -ExecutionPolicy Bypass -File packaging\register-msix.ps1

echo.
echo ==========================================
echo   Installation completed successfully!
echo   You can now close this window.
echo ==========================================
pause

```

---

### 3. `README.md` (El teu nou README professional)

Substitueix tot el text de l'arxiu `README.md` pel següent:

```markdown
# SnapFormat

**SnapFormat** is a high-performance, native Windows 11 context menu extension that brings instant image conversion directly to your File Explorer. Built with modern C++ and leveraging the Windows Imaging Component (WIC), it provides a seamless, "right-click-and-convert" workflow.

## 🚀 Key Features

* **Native Integration**: Context menu extension built for the Windows 11 modern UI.
* **Instant Conversion**: Convert images (JPG, PNG, BMP, TIFF) without opening heavy software.
* **Selection Filtering**: Smart selection management—prevents system strain by intelligently filtering large batches.
* **High Performance**: Powered by a headless C++ engine using the Windows Imaging Component (WIC).
* **Lightweight**: Distributed as an MSIX Sparse Package for easy, clean deployment.

## 🛠 Architecture

The project follows a clean monorepo architecture:

* **`src/converter-cli`**: The headless conversion engine. It acts as the robust, testable CLI contract.
* **`src/shell-ext`**: A native In-proc COM server (`IExplorerCommand`) that bridges File Explorer interactions to the CLI.
* **`packaging`**: Contains the MSIX manifest and registration scripts for modern Windows deployment.

## 📦 Getting Started

### Prerequisites

* Windows 11 (22000.0 or later).
* [Visual Studio](https://visualstudio.microsoft.com/) with C++ desktop development tools.
* [CMake](https://cmake.org/).

### Installation

1. Download the latest `SnapFormat.zip` from the [Releases page](https://github.com/marcantero/win11-quick-converter/releases).
2. Extract the contents.
3. Run `install.bat` as Administrator.

### Building from Source

```bash
# Clone the repository
git clone [https://github.com/marcantero/win11-quick-converter.git](https://github.com/marcantero/win11-quick-converter.git)
cd win11-quick-converter

# Build using CMake
cmake -S . -B build
cmake --build build --config Release

```

## 📝 Roadmap & Status

| Task | Status |
| --- | --- |
| CLI Core & Conversion Engine | ✅ |
| Native Win11 COM Extension | ✅ |
| MSIX Sparse Packaging | ✅ |
| Selection Filtering | ✅ |
| GitHub Actions CI/CD | ✅ |

## 🛡 License

This project is licensed under the MIT License - see the [LICENSE](https://www.google.com/search?q=LICENSE) file for details.
