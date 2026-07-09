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
