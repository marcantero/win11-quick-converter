#include "win11qc/common.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace win11qc {

std::string utf8_from_wide(std::wstring_view text)
{
    if (text.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(size), '\0');
    const int converted = WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    if (converted <= 0) {
        return {};
    }
    return result;
}

std::string utf8_from_path(const std::filesystem::path& path)
{
    return utf8_from_wide(path.native());
}

std::string format_hresult(HRESULT hr)
{
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
           << static_cast<unsigned long>(hr);
    return stream.str();
}

std::string to_lower_ascii(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

bool ensure_parent_directory(const std::filesystem::path& outputPath, std::string& errorMessage)
{
    const std::filesystem::path parentPath = outputPath.parent_path();
    if (parentPath.empty()) {
        return true;
    }

    std::error_code errorCode;
    if (std::filesystem::exists(parentPath, errorCode)) {
        return true;
    }

    if (!std::filesystem::create_directories(parentPath, errorCode) && errorCode) {
        errorMessage = "Failed to create output directory: " + utf8_from_path(parentPath);
        return false;
    }

    return true;
}

} // namespace win11qc
