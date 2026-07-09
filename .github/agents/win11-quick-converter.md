# Project: Win11 Quick Converter - AI Assistant Context

## 🎯 Project Overview
This project is a native Windows 11 context menu extension (right-click) that converts image files (e.g., JPG to PNG) instantly.
It uses a Monorepo architecture divided into two main components:
1. **CLI Core (`/src/converter-cli`)**: A standalone C++ executable that handles the actual image conversion using the Windows Imaging Component (WIC).
2. **Shell Extension (`/src/shell-ext`)**: An in-proc COM server (DLL) implementing `IExplorerCommand` for Windows 11 first-layer context menu integration.

## 💻 Tech Stack & Rules
- **Language**: Modern C++ (C++17 or C++20).
- **Libraries**: Windows API, WIC (Windows Imaging Component), COM, WIL (Windows Implementation Library).
- **Packaging**: MSIX / Sparse Packages for modern deployment.

## 🧠 Assistant Guidelines
When generating code or answering questions for this project, you MUST strictly adhere to the following:
1. **Modern C++ Practices**: Use smart pointers (`std::unique_ptr`), avoid raw `new`/`delete`, and use RAII for resource management.
2. **COM Handling**: Always use `Microsoft::WRL::ComPtr` for COM interfaces to prevent memory leaks. Properly check `HRESULT` return codes.
3. **Architecture Boundary**: Never mix UI/Menu logic into the CLI Core. The CLI must remain a headless, testable command-line tool. The DLL only parses the context menu click and spawns the CLI process.
4. **Error Handling**: Provide clear diagnostics and validation checks before attempting shell operations or file conversions.
5. **Language**: Keep all code comments, variable names, and documentation exclusively in professional English.