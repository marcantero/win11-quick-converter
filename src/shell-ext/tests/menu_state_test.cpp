// Tests for which "To <format>" entries the menu shows for a selection (issue #60):
// documents only offer PDF, images only offer image formats, and a mixed
// selection offers both.
//
// Usage: shell-ext-menu-test <path\to\shell-ext.dll> <scratch dir>

#include <Windows.h>
#include <ShObjIdl_core.h>
#include <ShlObj.h>
#include <wrl/client.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "win11qc/file_types.h"
#include "win11qc/shell_extension.h"

using Microsoft::WRL::ComPtr;
using namespace win11qc::shell;

namespace {

int g_failures = 0;

void check(bool condition, const char* what)
{
    std::cout << (condition ? "PASS: " : "FAIL: ") << what << '\n';
    if (!condition) {
        ++g_failures;
    }
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

// Subcommands of the root command, keyed by title ("To PNG", "To PDF", ...).
std::map<std::wstring, ComPtr<IExplorerCommand>> get_subcommands(HMODULE module)
{
    std::map<std::wstring, ComPtr<IExplorerCommand>> result;

    using DllGetClassObjectFn = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, void**);
    auto getClassObject = reinterpret_cast<DllGetClassObjectFn>(GetProcAddress(module, "DllGetClassObject"));
    if (!getClassObject) return result;

    ComPtr<IClassFactory> factory;
    if (FAILED(getClassObject(kShellExtensionClsid, IID_PPV_ARGS(&factory)))) return result;

    ComPtr<IExplorerCommand> root;
    if (FAILED(factory->CreateInstance(nullptr, IID_PPV_ARGS(&root)))) return result;

    ComPtr<IEnumExplorerCommand> subcommands;
    if (FAILED(root->EnumSubCommands(&subcommands))) return result;

    ComPtr<IExplorerCommand> command;
    ULONG fetched = 0;
    while (subcommands->Next(1, &command, &fetched) == S_OK && fetched == 1) {
        LPWSTR title = nullptr;
        if (SUCCEEDED(command->GetTitle(nullptr, &title)) && title) {
            result[title] = command;
            CoTaskMemFree(title);
        }
        command.Reset();
    }
    return result;
}

// Titles of the entries shown for `files`, in menu order of `commands`' keys.
std::wstring visible_entries(const std::map<std::wstring, ComPtr<IExplorerCommand>>& commands,
                             const std::vector<std::filesystem::path>& files)
{
    ComPtr<IShellItemArray> items = make_item_array(files);
    if (!items) return L"<no selection>";

    std::wstring visible;
    for (const auto& [title, command] : commands) {
        EXPCMDSTATE state = ECS_HIDDEN;
        if (SUCCEEDED(command->GetState(items.Get(), FALSE, &state)) && state == ECS_ENABLED) {
            if (!visible.empty()) visible += L",";
            visible += title;
        }
    }
    return visible;
}

void touch(const std::filesystem::path& path)
{
    std::ofstream(path, std::ios::binary).put('x');
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 3) {
        std::cerr << "usage: shell-ext-menu-test <shell-ext.dll> <scratch dir>\n";
        return 2;
    }

    // --- file type helpers ---
    check(is_document_path(L"C:\\a\\Report.DOCX"), "document extensions match case-insensitively");
    check(is_document_path(L"sheet.xlsx") && is_document_path(L"deck.odp"), "spreadsheets and slides are documents");
    check(!is_document_path(L"photo.png") && !is_document_path(L"notes.pdf"), "images and PDFs are not documents");
    check(converts_to(L"a.docx", L"pdf") && !converts_to(L"a.docx", L"png"), "documents only convert to PDF");
    check(!converts_to(L"a.png", L"pdf") && converts_to(L"a.png", L"jpg"), "images only convert to image formats");
    check(!converts_to(L"a.JPEG", L"jpg") && !converts_to(L"a.tif", L"tiff"), "images skip their own format");

    // --- menu state through the DLL ---
    const std::filesystem::path dllPath = std::filesystem::path(argv[1]).make_preferred();
    const std::filesystem::path workDir =
        std::filesystem::absolute(std::filesystem::path(argv[2]) / L"menu_state").make_preferred();
    std::filesystem::remove_all(workDir);
    std::filesystem::create_directories(workDir);

    const auto docx = workDir / L"letter.docx";
    const auto xlsx = workDir / L"budget.xlsx";
    const auto png = workDir / L"photo.png";
    for (const auto& file : {docx, xlsx, png}) touch(file);

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;

    HMODULE module = LoadLibraryW(dllPath.c_str());
    if (!module) {
        std::cerr << "LoadLibrary failed: " << GetLastError() << '\n';
        return 2;
    }

    {
        const auto commands = get_subcommands(module);
        check(commands.size() == 6, "the menu has six entries");
        check(commands.count(L"To PDF") == 1, "the menu has a To PDF entry");

        // std::map orders the titles alphabetically.
        check(visible_entries(commands, {docx}) == L"To PDF", "a document only offers To PDF");
        check(visible_entries(commands, {docx, xlsx}) == L"To PDF", "several documents only offer To PDF");
        check(visible_entries(commands, {png}) == L"To BMP,To JPG,To TIFF,To WebP",
              "an image offers the other image formats and no PDF");
        check(visible_entries(commands, {docx, png}) == L"To BMP,To JPG,To PDF,To TIFF,To WebP",
              "a mixed selection offers both, minus the image's own format");
    }

    FreeLibrary(module);
    CoUninitialize();
    std::filesystem::remove_all(workDir);
    return g_failures == 0 ? 0 : 1;
}
