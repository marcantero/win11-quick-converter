#include <Windows.h>
#include <ShObjIdl_core.h>
#include <objbase.h>
#include <wrl/client.h>
#include <iostream>
#include <filesystem>

#include "win11qc/shell_extension.h"

int wmain()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        std::wcerr << L"CoInitializeEx failed: 0x" << std::hex << hr << L"\n";
        return 1;
    }

    Microsoft::WRL::ComPtr<IExplorerCommand> cmd;

    hr = CoCreateInstance(win11qc::shell::kShellExtensionClsid, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IExplorerCommand), reinterpret_cast<void**>(cmd.GetAddressOf()));
    if (FAILED(hr)) {
        std::wcerr << L"CoCreateInstance failed: 0x" << std::hex << hr << L"\n";

        // Try direct LoadLibrary/GetProcAddress -> DllGetClassObject for diagnostics
        wchar_t modulePath[MAX_PATH] = {};
        // attempt to locate the dll next to this executable's directory
        if (GetModuleFileNameW(nullptr, modulePath, static_cast<DWORD>(std::size(modulePath))) > 0) {
            std::filesystem::path exePath{modulePath};
            std::filesystem::path exeDir = exePath.parent_path();
            std::vector<std::filesystem::path> candidates;
            candidates.push_back(exeDir / L"shell-ext.dll");
            // try build/src location
            if (exeDir.has_parent_path()) {
                auto buildDir = exeDir.parent_path().parent_path(); // build/tests/Release -> build
                candidates.push_back(buildDir / L"src" / L"shell-ext" / L"Release" / L"shell-ext.dll");
                candidates.push_back(buildDir / L"src" / L"shell-ext" / L"Debug" / L"shell-ext.dll");
            }

            HMODULE h = nullptr;
            for (auto &dllPath : candidates) {
                if (std::filesystem::exists(dllPath)) {
                    h = LoadLibraryW(dllPath.c_str());
                    if (h) break;
                }
            }
            if (h) {
                using DllGetClassObjectFn = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, void**);
                auto fn = reinterpret_cast<DllGetClassObjectFn>(GetProcAddress(h, "DllGetClassObject"));
                if (!fn) {
                    fn = reinterpret_cast<DllGetClassObjectFn>(GetProcAddress(h, "DllGetClassObject@12"));
                }
                if (fn) {
                    IClassFactory* factory = nullptr;
                    HRESULT hr2 = fn(win11qc::shell::kShellExtensionClsid, IID_PPV_ARGS(&factory));
                    std::wcerr << L"DllGetClassObject returned: 0x" << std::hex << hr2 << L"\n";
                    if (SUCCEEDED(hr2) && factory) {
                        IExplorerCommand* cmd2 = nullptr;
                        HRESULT hr3 = factory->CreateInstance(nullptr, __uuidof(IExplorerCommand), reinterpret_cast<void**>(&cmd2));
                        std::wcerr << L"ClassFactory::CreateInstance returned: 0x" << std::hex << hr3 << L"\n";
                        if (SUCCEEDED(hr3) && cmd2) {
                            LPWSTR title2 = nullptr;
                            hr3 = cmd2->GetTitle(nullptr, &title2);
                            if (SUCCEEDED(hr3) && title2) {
                                std::wcout << L"GetTitle (direct): " << title2 << L"\n";
                                CoTaskMemFree(title2);
                            }
                            cmd2->Release();
                        }

                        factory->Release();
                    }
                } else {
                    std::wcerr << L"GetProcAddress(DllGetClassObject) failed\n";
                }
                FreeLibrary(h);
            } else {
                std::wcerr << L"LoadLibrary: no candidate DLL found or all loads failed\n";
            }
        }

        CoUninitialize();
        return 2;
    }

    LPWSTR title = nullptr;
    hr = cmd->GetTitle(nullptr, &title);
    if (SUCCEEDED(hr) && title) {
        std::wcout << L"GetTitle: " << title << L"\n";
        CoTaskMemFree(title);
    } else {
        std::wcerr << L"GetTitle failed: 0x" << std::hex << hr << L"\n";
    }

    CoUninitialize();
    return 0;
}
