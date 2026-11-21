// Stand-in for LibreOffice's soffice.exe so the document path can be tested
// without LibreOffice. Behaviour is selected with FAKE_SOFFICE_MODE:
//   ok           write <outdir>\<input stem>.pdf and exit 0 (default)
//   no-output    exit 0 without writing anything
//   fail         write a partial PDF and exit 1
//   hang         never exit
//   spawn-hang   start a hanging child, write the PDF and exit 0
//   restart-once exit 81 without output on the first run, then behave like ok
// When FAKE_SOFFICE_ARGS_FILE is set, the arguments are appended to it (UTF-8, one per line).

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

std::wstring environment_value(const wchar_t* name)
{
    wchar_t buffer[1024] = {};
    const DWORD length = GetEnvironmentVariableW(name, buffer, static_cast<DWORD>(std::size(buffer)));
    return (length > 0 && length < std::size(buffer)) ? std::wstring(buffer, length) : std::wstring();
}

std::string utf8(std::wstring_view text)
{
    if (text.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

bool write_file(const std::filesystem::path& path, std::string_view content)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(stream);
}

int spawn_hanging_child()
{
    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    SetEnvironmentVariableW(L"FAKE_SOFFICE_MODE", L"hang");
    SetEnvironmentVariableW(L"FAKE_SOFFICE_ARGS_FILE", nullptr);

    std::wstring commandLine = L"\"" + std::wstring(self) + L"\" child";
    STARTUPINFOW startupInfo = {sizeof(startupInfo)};
    PROCESS_INFORMATION processInfo = {};
    if (!CreateProcessW(self, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                        &startupInfo, &processInfo)) {
        return 1;
    }
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return 0;
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
    const std::wstring argsFile = environment_value(L"FAKE_SOFFICE_ARGS_FILE");
    if (!argsFile.empty()) {
        std::ofstream stream(std::filesystem::path(argsFile), std::ios::binary | std::ios::app);
        for (int index = 1; index < argc; ++index) {
            stream << utf8(argv[index]) << '\n';
        }
    }

    std::wstring mode = environment_value(L"FAKE_SOFFICE_MODE");
    if (mode.empty()) {
        mode = L"ok";
    }

    if (mode == L"hang") {
        Sleep(INFINITE);
    }

    std::filesystem::path outDir;
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::wstring_view(argv[index]) == L"--outdir") {
            outDir = argv[index + 1];
        }
    }
    if (outDir.empty() || argc < 2) {
        return 2;
    }
    const std::filesystem::path input = argv[argc - 1];
    const std::filesystem::path pdf = outDir / (input.stem().native() + L".pdf");

    if (mode == L"restart-once") {
        const std::filesystem::path marker = outDir / L"restart-marker";
        if (!std::filesystem::exists(marker)) {
            write_file(marker, "restarted");
            return 81;
        }
    }

    if (mode == L"no-output") {
        return 0;
    }

    if (mode == L"fail") {
        write_file(pdf, "%PDF-1.4\n% truncated");
        return 1;
    }

    if (mode == L"spawn-hang" && spawn_hanging_child() != 0) {
        return 3;
    }

    return write_file(pdf, "%PDF-1.4\n% fake output\n%%EOF\n") ? 0 : 4;
}
