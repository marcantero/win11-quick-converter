#define NOMINMAX

#include "win11qc/document_converter.h"

#include "win11qc/common.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace win11qc {
namespace {

// Office formats LibreOffice can open. Keep in sync with the menu registration (#60).
constexpr std::array<std::wstring_view, 18> kDocumentExtensions = {
    L".doc", L".docx", L".docm", L".dot", L".dotx", L".odt", L".ott", L".rtf",
    L".xls", L".xlsx", L".xlsm", L".ods",
    L".ppt", L".pptx", L".pptm", L".pps", L".ppsx", L".odp",
};

constexpr DWORD kDefaultTimeoutSeconds = 120;
// After the launcher exits, helper processes it left behind get this long to finish.
constexpr DWORD kChildGraceMilliseconds = 5000;
// soffice exits with 81 after creating a fresh profile and asks to be restarted.
constexpr DWORD kRestartRequestedExitCode = 81;

// Developer and test overrides; not part of the CLI contract.
constexpr wchar_t kSofficeOverrideVariable[] = L"SNAPFORMAT_SOFFICE";
constexpr wchar_t kTimeoutOverrideVariable[] = L"SNAPFORMAT_SOFFICE_TIMEOUT";

// LibreOffice cannot run two conversions on one profile at the same time, so
// conversions from concurrent CLI processes are serialized.
constexpr wchar_t kConversionMutexName[] = L"Local\\SnapFormat.DocumentConversion";

// Disables macros in the private profile so converting a document never runs its code.
constexpr char kProfileSettings[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<oor:items xmlns:oor=\"http://openoffice.org/2001/registry\" "
    "xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" "
    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
    "<item oor:path=\"/org.openoffice.Office.Common/Security/Scripting\">"
    "<prop oor:name=\"DisableMacrosExecution\" oor:op=\"fuse\"><value>true</value></prop></item>\n"
    "<item oor:path=\"/org.openoffice.Office.Common/Security/Scripting\">"
    "<prop oor:name=\"MacroSecurityLevel\" oor:op=\"fuse\"><value>3</value></prop></item>\n"
    "</oor:items>\n";

class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE handle) noexcept : handle_(handle) {}
    ~UniqueHandle() { reset(); }

    UniqueHandle(UniqueHandle&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept
    {
        if (this != &other) {
            reset(std::exchange(other.handle_, nullptr));
        }
        return *this;
    }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    HANDLE get() const noexcept { return handle_; }
    bool valid() const noexcept { return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE; }

    void reset(HANDLE handle = nullptr) noexcept
    {
        if (valid()) {
            CloseHandle(handle_);
        }
        handle_ = handle;
    }

private:
    HANDLE handle_ = nullptr;
};

class ScopedMutexOwnership {
public:
    explicit ScopedMutexOwnership(HANDLE mutex) noexcept : mutex_(mutex) {}
    ~ScopedMutexOwnership() { ReleaseMutex(mutex_); }
    ScopedMutexOwnership(const ScopedMutexOwnership&) = delete;
    ScopedMutexOwnership& operator=(const ScopedMutexOwnership&) = delete;

private:
    HANDLE mutex_;
};

// A uniquely named directory under %TEMP%, removed with its contents on destruction.
class TemporaryDirectory {
public:
    TemporaryDirectory() = default;
    ~TemporaryDirectory()
    {
        if (!path_.empty()) {
            std::error_code errorCode;
            std::filesystem::remove_all(path_, errorCode);
        }
    }
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    bool create(std::string& errorMessage)
    {
        static std::atomic<unsigned> counter{0};
        std::error_code errorCode;
        const std::filesystem::path base = std::filesystem::temp_directory_path(errorCode);
        if (errorCode) {
            errorMessage = "Failed to find the temporary directory: " + errorCode.message();
            return false;
        }

        for (int attempt = 0; attempt < 100; ++attempt) {
            const std::filesystem::path candidate = base /
                (L"SnapFormat-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                 std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(counter++));
            if (std::filesystem::create_directory(candidate, errorCode)) {
                path_ = candidate;
                return true;
            }
            if (errorCode) {
                break;
            }
        }

        errorMessage = "Failed to create a temporary directory in " + utf8_from_path(base);
        return false;
    }

    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

ConversionResult validation_failure(std::string message)
{
    return {ExitCode::ValidationFailed, std::move(message)};
}

ConversionResult conversion_failure(std::string message)
{
    return {ExitCode::ConversionFailed, std::move(message)};
}

ConversionResult win32_failure(std::string_view what, DWORD error)
{
    return conversion_failure(std::string(what) + ": " + format_hresult(HRESULT_FROM_WIN32(error)));
}

std::optional<std::wstring> environment_value(const wchar_t* name)
{
    std::wstring value(256, L'\0');
    for (;;) {
        const DWORD length = GetEnvironmentVariableW(name, value.data(), static_cast<DWORD>(value.size()));
        if (length == 0) {
            return std::nullopt;
        }
        if (length < value.size()) {
            value.resize(length);
            return value;
        }
        value.resize(length);
    }
}

std::filesystem::path module_directory()
{
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
}

bool is_existing_file(const std::filesystem::path& path)
{
    std::error_code errorCode;
    return std::filesystem::is_regular_file(path, errorCode);
}

std::optional<std::filesystem::path> registered_libreoffice_program_dir(DWORD viewFlag)
{
    constexpr wchar_t kSubKey[] = L"SOFTWARE\\LibreOffice\\UNO\\InstallPath";
    DWORD size = 0;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, kSubKey, nullptr, RRF_RT_REG_SZ | viewFlag, nullptr, nullptr, &size) !=
            ERROR_SUCCESS ||
        size < sizeof(wchar_t)) {
        return std::nullopt;
    }

    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_LOCAL_MACHINE, kSubKey, nullptr, RRF_RT_REG_SZ | viewFlag, nullptr, value.data(), &size) !=
        ERROR_SUCCESS) {
        return std::nullopt;
    }
    value.resize(wcsnlen(value.c_str(), value.size()));
    if (value.empty()) {
        return std::nullopt;
    }
    return std::filesystem::path(value);
}

// Search order: developer override, LibreOffice bundled next to the CLI,
// then a LibreOffice installed on the system.
std::optional<std::filesystem::path> find_soffice(std::string& errorMessage)
{
    if (const std::optional<std::wstring> overridePath = environment_value(kSofficeOverrideVariable)) {
        if (is_existing_file(*overridePath)) {
            return std::filesystem::path(*overridePath);
        }
        errorMessage = "SNAPFORMAT_SOFFICE points to a file that does not exist: " + utf8_from_wide(*overridePath);
        return std::nullopt;
    }

    std::vector<std::filesystem::path> candidates;
    if (const std::filesystem::path appDir = module_directory(); !appDir.empty()) {
        candidates.push_back(appDir / L"libreoffice" / L"program" / L"soffice.exe");
    }
    for (const DWORD view : {static_cast<DWORD>(RRF_SUBKEY_WOW6464KEY), static_cast<DWORD>(RRF_SUBKEY_WOW6432KEY)}) {
        if (const std::optional<std::filesystem::path> programDir = registered_libreoffice_program_dir(view)) {
            candidates.push_back(*programDir / L"soffice.exe");
        }
    }
    for (const wchar_t* variable : {L"ProgramFiles", L"ProgramFiles(x86)"}) {
        if (const std::optional<std::wstring> programFiles = environment_value(variable)) {
            candidates.push_back(std::filesystem::path(*programFiles) / L"LibreOffice" / L"program" / L"soffice.exe");
        }
    }

    for (const std::filesystem::path& candidate : candidates) {
        if (is_existing_file(candidate)) {
            return candidate;
        }
    }

    errorMessage = "LibreOffice was not found. Reinstall SnapFormat with document conversion, or install LibreOffice.";
    return std::nullopt;
}

DWORD timeout_milliseconds()
{
    DWORD seconds = kDefaultTimeoutSeconds;
    if (const std::optional<std::wstring> text = environment_value(kTimeoutOverrideVariable)) {
        DWORD parsed = 0;
        bool valid = !text->empty() && text->size() <= 6;
        for (const wchar_t character : *text) {
            if (character < L'0' || character > L'9') {
                valid = false;
                break;
            }
            parsed = parsed * 10 + static_cast<DWORD>(character - L'0');
        }
        if (valid && parsed > 0) {
            seconds = parsed;
        }
    }
    return seconds * 1000;
}

// Builds a file:// URL as LibreOffice expects for -env:UserInstallation.
std::wstring file_url_from_path(const std::filesystem::path& path)
{
    const std::string utf8 = utf8_from_path(path);
    const bool isUnc = utf8.size() > 1 && utf8[0] == '\\' && utf8[1] == '\\';

    std::wstring url = isUnc ? L"file:" : L"file:///";
    constexpr char kHexDigits[] = "0123456789ABCDEF";
    for (const char raw : utf8) {
        const auto byte = static_cast<unsigned char>(raw);
        if (byte == '\\') {
            url += L'/';
        } else if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9') ||
                   byte == '-' || byte == '.' || byte == '_' || byte == '~' || byte == '/' || byte == ':') {
            url += static_cast<wchar_t>(byte);
        } else {
            url += L'%';
            url += static_cast<wchar_t>(kHexDigits[byte >> 4]);
            url += static_cast<wchar_t>(kHexDigits[byte & 0x0F]);
        }
    }
    return url;
}

// Quotes one argument so CommandLineToArgvW and the CRT parse it back unchanged.
std::wstring quote_argument(std::wstring_view argument)
{
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring_view::npos) {
        return std::wstring(argument);
    }

    std::wstring quoted = L"\"";
    for (auto it = argument.begin();; ++it) {
        std::size_t backslashes = 0;
        while (it != argument.end() && *it == L'\\') {
            ++it;
            ++backslashes;
        }

        if (it == argument.end()) {
            quoted.append(backslashes * 2, L'\\');
            break;
        }
        if (*it == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
        } else {
            quoted.append(backslashes, L'\\');
        }
        quoted += *it;
    }
    quoted += L'"';
    return quoted;
}

std::filesystem::path profile_directory()
{
    std::filesystem::path base;
    if (const std::optional<std::wstring> localAppData = environment_value(L"LOCALAPPDATA")) {
        base = *localAppData;
    } else {
        std::error_code errorCode;
        base = std::filesystem::temp_directory_path(errorCode);
    }
    return base / L"SnapFormat" / L"libreoffice-profile";
}

// Creates the private LibreOffice profile with macros disabled. LibreOffice
// keeps these settings when it later rewrites the file.
bool prepare_profile(const std::filesystem::path& profileDir, std::string& errorMessage)
{
    const std::filesystem::path userDir = profileDir / L"user";
    std::error_code errorCode;
    std::filesystem::create_directories(userDir, errorCode);
    if (errorCode) {
        errorMessage = "Failed to create the LibreOffice profile directory " + utf8_from_path(userDir) + ": " +
                       errorCode.message();
        return false;
    }

    const std::filesystem::path settingsFile = userDir / L"registrymodifications.xcu";
    if (std::filesystem::exists(settingsFile, errorCode)) {
        return true;
    }

    std::ofstream stream(settingsFile, std::ios::binary | std::ios::trunc);
    stream.write(kProfileSettings, static_cast<std::streamsize>(sizeof(kProfileSettings) - 1));
    stream.close();
    if (!stream) {
        std::filesystem::remove(settingsFile, errorCode);
        errorMessage = "Failed to write the LibreOffice profile settings: " + utf8_from_path(settingsFile);
        return false;
    }
    return true;
}

struct ProcessOutcome {
    enum class Status { Exited, TimedOut, LaunchFailed };
    Status status = Status::LaunchFailed;
    DWORD exitCode = 0;
    DWORD error = ERROR_SUCCESS;
    std::string step;
};

ProcessOutcome launch_failure(std::string step, DWORD error)
{
    ProcessOutcome outcome;
    outcome.status = ProcessOutcome::Status::LaunchFailed;
    outcome.step = std::move(step);
    outcome.error = error;
    return outcome;
}

DWORD active_process_count(HANDLE job)
{
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting = {};
    if (!QueryInformationJobObject(
            job, JobObjectBasicAccountingInformation, &accounting, sizeof(accounting), nullptr)) {
        return 0;
    }
    return accounting.ActiveProcesses;
}

// Runs a process with no console and no inherited output, inside a job object
// so every process it starts is killed on timeout or when we return.
ProcessOutcome run_process(const std::filesystem::path& executable, std::wstring commandLine, DWORD timeoutMs)
{
    SECURITY_ATTRIBUTES inheritable = {sizeof(inheritable), nullptr, TRUE};
    UniqueHandle nul(CreateFileW(
        L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, OPEN_EXISTING, 0,
        nullptr));
    if (!nul.valid()) {
        return launch_failure("Failed to open NUL", GetLastError());
    }

    UniqueHandle job(CreateJobObjectW(nullptr, nullptr));
    if (!job.valid()) {
        return launch_failure("Failed to create a job object", GetLastError());
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        return launch_failure("Failed to configure the job object", GetLastError());
    }

    // Only the NUL handle may be inherited.
    SIZE_T attributeSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
    std::vector<std::byte> attributeBuffer(attributeSize);
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeBuffer.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attributeSize)) {
        return launch_failure("Failed to initialize process attributes", GetLastError());
    }
    struct AttributeListCleanup {
        LPPROC_THREAD_ATTRIBUTE_LIST list;
        ~AttributeListCleanup() { DeleteProcThreadAttributeList(list); }
    } attributeCleanup{attributes};

    HANDLE inheritedHandles[] = {nul.get()};
    if (!UpdateProcThreadAttribute(
            attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inheritedHandles, sizeof(inheritedHandles), nullptr,
            nullptr)) {
        return launch_failure("Failed to set inherited handles", GetLastError());
    }

    STARTUPINFOEXW startupInfo = {};
    startupInfo.StartupInfo.cb = sizeof(startupInfo);
    startupInfo.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.StartupInfo.hStdInput = nul.get();
    startupInfo.StartupInfo.hStdOutput = nul.get();
    startupInfo.StartupInfo.hStdError = nul.get();
    startupInfo.lpAttributeList = attributes;

    PROCESS_INFORMATION processInfo = {};
    if (!CreateProcessW(
            executable.c_str(), commandLine.data(), nullptr, nullptr, TRUE,
            CREATE_SUSPENDED | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
            &startupInfo.StartupInfo, &processInfo)) {
        return launch_failure("Failed to start LibreOffice (" + utf8_from_path(executable) + ")", GetLastError());
    }
    UniqueHandle process(processInfo.hProcess);
    UniqueHandle thread(processInfo.hThread);

    if (!AssignProcessToJobObject(job.get(), process.get())) {
        const DWORD error = GetLastError();
        TerminateProcess(process.get(), 1);
        return launch_failure("Failed to assign LibreOffice to a job object", error);
    }
    if (ResumeThread(thread.get()) == static_cast<DWORD>(-1)) {
        const DWORD error = GetLastError();
        TerminateJobObject(job.get(), 1);
        return launch_failure("Failed to resume LibreOffice", error);
    }

    const ULONGLONG start = GetTickCount64();
    const DWORD waitResult = WaitForSingleObject(process.get(), timeoutMs);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateJobObject(job.get(), 1);
        WaitForSingleObject(process.get(), 5000);
        ProcessOutcome outcome;
        outcome.status = ProcessOutcome::Status::TimedOut;
        return outcome;
    }
    if (waitResult != WAIT_OBJECT_0) {
        const DWORD error = GetLastError();
        TerminateJobObject(job.get(), 1);
        return launch_failure("Failed to wait for LibreOffice", error);
    }

    ProcessOutcome outcome;
    outcome.status = ProcessOutcome::Status::Exited;
    if (!GetExitCodeProcess(process.get(), &outcome.exitCode)) {
        return launch_failure("Failed to read the LibreOffice exit code", GetLastError());
    }

    // The launcher may leave helper processes behind; give them a moment to
    // finish writing, then close the job, which kills whatever is left.
    const ULONGLONG elapsed = GetTickCount64() - start;
    const ULONGLONG remaining = elapsed < timeoutMs ? timeoutMs - elapsed : 0;
    const ULONGLONG graceEnd = GetTickCount64() + std::min<ULONGLONG>(remaining, kChildGraceMilliseconds);
    while (active_process_count(job.get()) > 0 && GetTickCount64() < graceEnd) {
        Sleep(50);
    }
    TerminateJobObject(job.get(), 1);
    return outcome;
}

std::optional<std::filesystem::path> find_produced_pdf(const std::filesystem::path& directory)
{
    std::error_code errorCode;
    for (const auto& entry : std::filesystem::directory_iterator(directory, errorCode)) {
        std::error_code entryError;
        if (entry.is_regular_file(entryError) && to_lower_ascii(utf8_from_path(entry.path().extension())) == ".pdf" &&
            entry.file_size(entryError) > 0 && !entryError) {
            return entry.path();
        }
    }
    return std::nullopt;
}

// Moves the finished PDF into place. It is first copied next to the output
// (possibly across volumes) and then renamed, so the output path never holds
// a partial file and an existing output survives any failure.
std::optional<ConversionResult> publish_output(const std::filesystem::path& producedPdf,
                                               const std::filesystem::path& outputPath)
{
    std::filesystem::path stagingPath = outputPath;
    stagingPath.replace_filename(L"." + outputPath.filename().native() + L".snapformat-" +
                                 std::to_wstring(GetCurrentProcessId()) + L".tmp");

    if (!MoveFileExW(producedPdf.c_str(), stagingPath.c_str(),
                     MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        std::error_code removeError;
        std::filesystem::remove(stagingPath, removeError);
        return win32_failure("Failed to write the PDF next to " + utf8_from_path(outputPath), error);
    }

    if (!MoveFileExW(stagingPath.c_str(), outputPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        const DWORD error = GetLastError();
        std::error_code removeError;
        std::filesystem::remove(stagingPath, removeError);
        return win32_failure("Failed to write output file " + utf8_from_path(outputPath), error);
    }
    return std::nullopt;
}

} // namespace

bool is_document_input(const std::filesystem::path& inputPath)
{
    std::wstring extension = inputPath.extension().native();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t ch) {
        return (ch >= L'A' && ch <= L'Z') ? static_cast<wchar_t>(ch - L'A' + L'a') : ch;
    });
    return std::find(kDocumentExtensions.begin(), kDocumentExtensions.end(), extension) != kDocumentExtensions.end();
}

ConversionResult convert_document(const CliOptions& options)
{
    if (to_lower_ascii(options.format) != "pdf") {
        return validation_failure(
            "Unsupported output format for documents: " + options.format + ". Documents can only be converted to pdf.");
    }

    std::error_code errorCode;
    if (std::filesystem::equivalent(options.inputPath, options.outputPath, errorCode) && !errorCode) {
        return validation_failure("Input and output refer to the same file: " + utf8_from_path(options.inputPath));
    }

    if (!std::filesystem::exists(options.inputPath, errorCode)) {
        return validation_failure("Input file does not exist: " + utf8_from_path(options.inputPath));
    }
    if (!std::filesystem::is_regular_file(options.inputPath, errorCode)) {
        return validation_failure("Input path is not a regular file: " + utf8_from_path(options.inputPath));
    }

    std::string message;
    if (!ensure_parent_directory(options.outputPath, message)) {
        return validation_failure(message);
    }

    const std::filesystem::path inputPath = std::filesystem::absolute(options.inputPath, errorCode);
    if (errorCode) {
        return validation_failure("Failed to resolve the input path: " + utf8_from_path(options.inputPath));
    }

    const std::optional<std::filesystem::path> soffice = find_soffice(message);
    if (!soffice) {
        return conversion_failure(message);
    }

    const DWORD timeoutMs = timeout_milliseconds();

    UniqueHandle mutex(CreateMutexW(nullptr, FALSE, kConversionMutexName));
    if (!mutex.valid()) {
        return win32_failure("Failed to create the document conversion lock", GetLastError());
    }
    const DWORD lockResult = WaitForSingleObject(mutex.get(), timeoutMs);
    if (lockResult == WAIT_TIMEOUT) {
        return conversion_failure("Timed out waiting for another document conversion to finish.");
    }
    if (lockResult != WAIT_OBJECT_0 && lockResult != WAIT_ABANDONED) {
        return win32_failure("Failed to acquire the document conversion lock", GetLastError());
    }
    ScopedMutexOwnership lock(mutex.get());

    const std::filesystem::path profileDir = profile_directory();
    if (!prepare_profile(profileDir, message)) {
        return conversion_failure(message);
    }

    TemporaryDirectory workDir;
    if (!workDir.create(message)) {
        return conversion_failure(message);
    }

    const std::wstring commandLine =
        quote_argument(soffice->native()) + L" " +
        quote_argument(L"-env:UserInstallation=" + file_url_from_path(profileDir)) +
        L" --headless --invisible --nologo --nodefault --norestore --nolockcheck"
        L" --convert-to pdf --outdir " +
        quote_argument(workDir.path().native()) + L" " + quote_argument(inputPath.native());

    ProcessOutcome outcome = run_process(*soffice, commandLine, timeoutMs);
    // A fresh profile makes soffice exit once with a restart request.
    if (outcome.status == ProcessOutcome::Status::Exited && outcome.exitCode == kRestartRequestedExitCode &&
        !find_produced_pdf(workDir.path())) {
        outcome = run_process(*soffice, commandLine, timeoutMs);
    }

    switch (outcome.status) {
    case ProcessOutcome::Status::LaunchFailed:
        return win32_failure(outcome.step, outcome.error);
    case ProcessOutcome::Status::TimedOut:
        return conversion_failure("LibreOffice did not finish within " + std::to_string(timeoutMs / 1000) +
                                  " seconds; the conversion was cancelled.");
    case ProcessOutcome::Status::Exited:
        break;
    }

    const std::optional<std::filesystem::path> producedPdf = find_produced_pdf(workDir.path());
    if (outcome.exitCode != 0 || !producedPdf) {
        return conversion_failure("LibreOffice could not convert " + utf8_from_path(options.inputPath) +
                                  " (exit code " + std::to_string(outcome.exitCode) + ").");
    }

    if (std::optional<ConversionResult> failure = publish_output(*producedPdf, options.outputPath)) {
        return *failure;
    }

    return {
        ExitCode::Success,
        "Converted " + utf8_from_path(options.inputPath) + " to " + utf8_from_path(options.outputPath) +
            " using LibreOffice."
    };
}

} // namespace win11qc
