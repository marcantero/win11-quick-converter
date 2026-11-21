#include "win11qc/batch_report.h"

#include <Windows.h>
#include <shlwapi.h>

#include <cwctype>

namespace win11qc::shell {

std::wstring format_display_name(const std::wstring& formatExt)
{
    if (CompareStringOrdinal(formatExt.c_str(), -1, L"webp", -1, TRUE) == CSTR_EQUAL) {
        return L"WebP";
    }

    std::wstring name = formatExt;
    for (auto& ch : name) {
        ch = static_cast<wchar_t>(std::towupper(ch));
    }
    return name;
}

std::wstring cli_failure_reason(const std::string& utf8Output, unsigned long exitCode)
{
    std::size_t start = 0;
    while (start < utf8Output.size()) {
        std::size_t end = utf8Output.find('\n', start);
        if (end == std::string::npos) end = utf8Output.size();

        std::string line = utf8Output.substr(start, end - start);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();

        if (!line.empty()) {
            const int size = MultiByteToWideChar(CP_UTF8, 0, line.data(), static_cast<int>(line.size()), nullptr, 0);
            if (size > 0) {
                std::wstring wide(static_cast<std::size_t>(size), L'\0');
                MultiByteToWideChar(CP_UTF8, 0, line.data(), static_cast<int>(line.size()), wide.data(), size);
                return wide;
            }
        }
        start = end + 1;
    }

    return L"converter-cli exited with code " + std::to_wstring(exitCode) + L".";
}

bool should_notify(const BatchReport& report)
{
    if (report.attempted == 0) {
        return false;
    }
    return report.attempted > 1 || report.cancelled || !report.failures.empty();
}

std::wstring xml_escape(const std::wstring& text)
{
    std::wstring escaped;
    escaped.reserve(text.size());
    for (const wchar_t ch : text) {
        switch (ch) {
        case L'&': escaped += L"&amp;"; break;
        case L'<': escaped += L"&lt;"; break;
        case L'>': escaped += L"&gt;"; break;
        case L'"': escaped += L"&quot;"; break;
        case L'\'': escaped += L"&apos;"; break;
        default: escaped += ch; break;
        }
    }
    return escaped;
}

std::wstring file_uri(const std::filesystem::path& path)
{
    // UrlCreateFromPathW percent-encodes spaces, '#', '%' and the like.
    constexpr DWORD kMaxUrlLength = 2048 + 32 + 4; // INTERNET_MAX_URL_LENGTH
    std::wstring buffer(kMaxUrlLength, L'\0');
    DWORD length = static_cast<DWORD>(buffer.size());
    if (FAILED(UrlCreateFromPathW(path.c_str(), buffer.data(), &length, 0))) {
        return {};
    }
    buffer.resize(length);
    return buffer;
}

namespace {

std::wstring plural(std::size_t count, const wchar_t* singular, const wchar_t* pluralForm)
{
    return std::to_wstring(count) + L" " + (count == 1 ? singular : pluralForm);
}

// What the converted files are called in the toast: PDFs come from documents.
bool is_document_report(const BatchReport& report)
{
    return CompareStringOrdinal(report.formatExt.c_str(), -1, L"pdf", -1, TRUE) == CSTR_EQUAL;
}

std::wstring items(const BatchReport& report, std::size_t count)
{
    return is_document_report(report) ? plural(count, L"document", L"documents") : plural(count, L"image", L"images");
}

std::wstring headline(const BatchReport& report, const std::wstring& format)
{
    const std::size_t succeeded = report.outputs.size();

    if (report.cancelled) {
        return L"Conversion to " + format + L" cancelled";
    }

    if (report.attempted == 1) {
        const std::wstring name = report.failures.empty()
            ? report.outputs.front().filename().wstring()
            : report.failures.front().input.filename().wstring();
        return (report.failures.empty() ? L"Converted " : L"Couldn't convert ") + name + L" to " + format;
    }

    if (report.failures.empty()) {
        return L"Converted " + items(report, succeeded) + L" to " + format;
    }
    if (succeeded == 0) {
        return L"Couldn't convert " + items(report, report.attempted) + L" to " + format;
    }
    return L"Converted " + std::to_wstring(succeeded) + L" of " +
           items(report, report.attempted) + L" to " + format;
}

std::vector<std::wstring> detail_lines(const BatchReport& report)
{
    std::vector<std::wstring> lines;

    if (report.cancelled) {
        lines.push_back(std::to_wstring(report.outputs.size()) + L" of " +
                        items(report, report.attempted) + L" converted.");
    }

    if (!report.failures.empty()) {
        const ConversionFailure& first = report.failures.front();
        lines.push_back(report.attempted == 1 ? first.reason
                                              : first.input.filename().wstring() + L": " + first.reason);
        if (report.failures.size() > 1) {
            lines.push_back(L"+" + std::to_wstring(report.failures.size() - 1) + L" more failed.");
        }
    }

    // ToastGeneric renders at most three text elements (headline + two).
    if (lines.size() > 2) {
        lines.resize(2);
    }
    return lines;
}

// Folder shared by every output, or empty when they span several folders.
std::filesystem::path common_output_folder(const BatchReport& report)
{
    if (report.outputs.empty()) {
        return {};
    }

    const std::filesystem::path folder = report.outputs.front().parent_path();
    for (const auto& output : report.outputs) {
        if (CompareStringOrdinal(output.parent_path().c_str(), -1, folder.c_str(), -1, TRUE) != CSTR_EQUAL) {
            return {};
        }
    }
    return folder;
}

} // namespace

std::wstring build_toast_xml(const BatchReport& report)
{
    const std::wstring format = format_display_name(report.formatExt);
    const std::filesystem::path folder = common_output_folder(report);
    const std::wstring folderUri = folder.empty() ? std::wstring{} : file_uri(folder);

    std::wstring xml = L"<toast";
    if (!folderUri.empty()) {
        // Clicking the toast body opens the destination folder too.
        xml += L" activationType=\"protocol\" launch=\"" + xml_escape(folderUri) + L"\"";
    }
    xml += L"><visual><binding template=\"ToastGeneric\">";
    xml += L"<text>" + xml_escape(headline(report, format)) + L"</text>";
    for (const auto& line : detail_lines(report)) {
        xml += L"<text>" + xml_escape(line) + L"</text>";
    }
    xml += L"</binding></visual>";

    std::wstring actions;
    if (!folderUri.empty()) {
        actions += L"<action content=\"Open folder\" activationType=\"protocol\" arguments=\"" +
                   xml_escape(folderUri) + L"\"/>";
    }
    if (report.outputs.size() == 1) {
        const std::wstring imageUri = file_uri(report.outputs.front());
        if (!imageUri.empty()) {
            const wchar_t* label = is_document_report(report) ? L"Open PDF" : L"Open image";
            actions += L"<action content=\"" + std::wstring{label} + L"\" activationType=\"protocol\" arguments=\"" +
                       xml_escape(imageUri) + L"\"/>";
        }
    }
    if (!actions.empty()) {
        xml += L"<actions>" + actions + L"</actions>";
    }

    xml += L"</toast>";
    return xml;
}

} // namespace win11qc::shell
