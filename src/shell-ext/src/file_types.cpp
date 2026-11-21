#include "win11qc/file_types.h"

#include <Windows.h>

#include <algorithm>

namespace win11qc::shell {

namespace {

bool equals_ignore_case(std::wstring_view left, std::wstring_view right)
{
    return CompareStringOrdinal(left.data(), static_cast<int>(left.size()), right.data(),
                                static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

} // namespace

bool is_document_path(const std::filesystem::path& path)
{
    const std::wstring ext = path.extension().wstring();
    return std::any_of(kDocumentExtensions.begin(), kDocumentExtensions.end(),
                       [&](std::wstring_view candidate) { return equals_ignore_case(ext, candidate); });
}

bool has_format_extension(const std::filesystem::path& path, const std::wstring& formatExt)
{
    const std::wstring ext = path.extension().wstring();
    if (ext.size() < 2) return false;

    const std::wstring_view bare = std::wstring_view(ext).substr(1);
    auto matches = [&](std::wstring_view candidate) { return equals_ignore_case(bare, candidate); };

    if (matches(formatExt)) return true;
    if (formatExt == L"jpg") return matches(L"jpeg") || matches(L"jpe");
    if (formatExt == L"tiff") return matches(L"tif");
    return false;
}

bool converts_to(const std::filesystem::path& path, const std::wstring& formatExt)
{
    const bool toDocumentFormat = formatExt == kDocumentFormat;
    if (is_document_path(path)) return toDocumentFormat;
    return !toDocumentFormat && !has_format_extension(path, formatExt);
}

} // namespace win11qc::shell
