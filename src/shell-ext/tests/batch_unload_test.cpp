// Regression test for issue #44: a batch conversion thread must keep the shell
// extension DLL loaded until it finishes, even after the host (Explorer) has
// released every COM object and dropped its own reference to the module.
//
// Usage: shell-ext-batch-test <path\to\shell-ext.dll> <scratch dir>
// converter-cli.exe must sit next to the DLL (the normal build layout).

#include <Windows.h>
#include <ShObjIdl_core.h>
#include <ShlObj.h>
#include <wrl/client.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

#include "win11qc/shell_extension.h"

using Microsoft::WRL::ComPtr;

namespace {

int g_failures = 0;

void check(bool condition, const char* what)
{
    std::cout << (condition ? "PASS: " : "FAIL: ") << what << '\n';
    if (!condition) {
        ++g_failures;
    }
}

void write_test_bmp(const std::filesystem::path& path)
{
    // Minimal 2x2 24-bit BMP (bottom-up, rows padded to 4 bytes).
    const std::array<std::uint8_t, 16> pixels{0, 0, 255, 0, 255, 0, 0, 0, 255, 0, 0, 255, 255, 255, 0, 0};
    std::array<std::uint8_t, 54> header{};
    auto put32 = [&](std::size_t offset, std::uint32_t value) {
        for (int i = 0; i < 4; ++i) header[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
    };
    header[0] = 'B';
    header[1] = 'M';
    put32(2, static_cast<std::uint32_t>(header.size() + pixels.size()));
    put32(10, static_cast<std::uint32_t>(header.size()));
    put32(14, 40);
    put32(18, 2);
    put32(22, 2);
    header[26] = 1;  // planes
    header[28] = 24; // bits per pixel
    put32(34, static_cast<std::uint32_t>(pixels.size()));

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(header.data()), header.size());
    out.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
}

ComPtr<IShellItemArray> make_item_array(const std::vector<std::filesystem::path>& files)
{
    std::vector<PIDLIST_ABSOLUTE> pidls;
    for (const auto& file : files) {
        PIDLIST_ABSOLUTE pidl = nullptr;
        if (SUCCEEDED(SHParseDisplayName(file.c_str(), nullptr, &pidl, 0, nullptr))) {
            pidls.push_back(pidl);
        }
    }

    ComPtr<IShellItemArray> array;
    if (pidls.size() == files.size()) {
        SHCreateShellItemArrayFromIDLists(static_cast<UINT>(pidls.size()),
                                          const_cast<PCIDLIST_ABSOLUTE_ARRAY>(pidls.data()), &array);
    }

    for (auto pidl : pidls) {
        CoTaskMemFree(pidl);
    }
    return array;
}

// Returns the first subcommand ("To PNG") of the extension's root command.
ComPtr<IExplorerCommand> get_png_subcommand(HMODULE module)
{
    using DllGetClassObjectFn = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, void**);
    auto getClassObject = reinterpret_cast<DllGetClassObjectFn>(GetProcAddress(module, "DllGetClassObject"));
    if (!getClassObject) return nullptr;

    ComPtr<IClassFactory> factory;
    if (FAILED(getClassObject(win11qc::shell::kShellExtensionClsid, IID_PPV_ARGS(&factory)))) return nullptr;

    ComPtr<IExplorerCommand> root;
    if (FAILED(factory->CreateInstance(nullptr, IID_PPV_ARGS(&root)))) return nullptr;

    ComPtr<IEnumExplorerCommand> subcommands;
    if (FAILED(root->EnumSubCommands(&subcommands))) return nullptr;

    ComPtr<IExplorerCommand> first;
    ULONG fetched = 0;
    if (subcommands->Next(1, &first, &fetched) != S_OK || fetched != 1) return nullptr;
    return first;
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 3) {
        std::cerr << "usage: shell-ext-batch-test <shell-ext.dll> <scratch dir>\n";
        return 2;
    }

    // CTest passes forward slashes; the shell parser only accepts backslashes.
    const std::filesystem::path dllPath = std::filesystem::path(argv[1]).make_preferred();
    const std::filesystem::path workDir =
        std::filesystem::absolute(std::filesystem::path(argv[2]) / L"batch_unload").make_preferred();
    std::filesystem::remove_all(workDir);
    std::filesystem::create_directories(workDir);

    std::vector<std::filesystem::path> inputs;
    for (int i = 0; i < 3; ++i) {
        inputs.push_back(workDir / (L"image" + std::to_wstring(i) + L".bmp"));
        write_test_bmp(inputs.back());
    }

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;

    HMODULE module = LoadLibraryW(dllPath.c_str());
    if (!module) {
        std::cerr << "LoadLibrary failed: " << GetLastError() << '\n';
        return 2;
    }

    using DllCanUnloadNowFn = HRESULT(STDAPICALLTYPE*)();
    auto canUnloadNow = reinterpret_cast<DllCanUnloadNowFn>(GetProcAddress(module, "DllCanUnloadNow"));

    {
        ComPtr<IExplorerCommand> toPng = get_png_subcommand(module);
        ComPtr<IShellItemArray> items = make_item_array(inputs);
        check(toPng != nullptr, "obtained the To PNG subcommand");
        check(items != nullptr, "built a 3-item shell selection");
        if (!toPng || !items) return 1;

        check(SUCCEEDED(toPng->Invoke(items.Get(), nullptr)), "Invoke started the batch");
    } // Release every COM object, as Explorer does once the menu closes.

    // The worker is still converting: COM must not be told the DLL is idle...
    check(canUnloadNow && canUnloadNow() == S_FALSE, "DllCanUnloadNow returns S_FALSE while the batch runs");

    // ...and even if the host drops its reference anyway, the module must stay mapped.
    FreeLibrary(module);
    check(GetModuleHandleW(dllPath.filename().c_str()) != nullptr, "module stays loaded after the host's FreeLibrary");

    // The worker drops the last reference itself when it finishes.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (GetModuleHandleW(dllPath.filename().c_str()) != nullptr && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    check(GetModuleHandleW(dllPath.filename().c_str()) == nullptr, "module unloads once the batch completes");

    bool allConverted = true;
    for (const auto& input : inputs) {
        allConverted = allConverted && std::filesystem::exists(std::filesystem::path(input).replace_extension(L".png"));
    }
    check(allConverted, "every file in the batch was converted");

    CoUninitialize();
    std::filesystem::remove_all(workDir);
    return g_failures == 0 ? 0 : 1;
}
