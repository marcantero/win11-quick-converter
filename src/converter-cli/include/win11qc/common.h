#pragma once

#include <Windows.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace win11qc {

// Converts UTF-16 text (arguments, paths) to UTF-8 for diagnostics.
std::string utf8_from_wide(std::wstring_view text);

// A path as UTF-8, for diagnostics.
std::string utf8_from_path(const std::filesystem::path& path);

// Formats an HRESULT as 0xXXXXXXXX.
std::string format_hresult(HRESULT hr);

// ASCII lower-casing for format names and extensions.
std::string to_lower_ascii(std::string text);

// Creates the parent directory of outputPath if it is missing.
bool ensure_parent_directory(const std::filesystem::path& outputPath, std::string& errorMessage);

} // namespace win11qc
