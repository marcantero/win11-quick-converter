#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <string_view>

namespace win11qc::shell {

// Office documents the CLI converts to PDF. Keep in sync with
// kDocumentExtensions in src/converter-cli/src/document_converter.cpp and with
// the ItemType list in packaging/AppxManifest.xml.
inline constexpr std::array<std::wstring_view, 18> kDocumentExtensions = {
    L".doc", L".docx", L".docm", L".dot", L".dotx", L".odt", L".ott", L".rtf",
    L".xls", L".xlsx", L".xlsm", L".ods",
    L".ppt", L".pptx", L".pptm", L".pps", L".ppsx", L".odp",
};

// Images whose perceived type is not reliably "image", so the menu is
// registered for them explicitly. Keep in sync with packaging/AppxManifest.xml.
inline constexpr std::array<std::wstring_view, 3> kExtraImageExtensions = {L".webp", L".heic", L".avif"};

// Output format offered for documents.
inline constexpr std::wstring_view kDocumentFormat = L"pdf";

// True when `path` has one of kDocumentExtensions (case-insensitive).
bool is_document_path(const std::filesystem::path& path);

// True when `path` already has the extension of `formatExt` (or an alias of it,
// e.g. .jpeg for jpg, .tif for tiff), compared case-insensitively.
bool has_format_extension(const std::filesystem::path& path, const std::wstring& formatExt);

// True when a "To <formatExt>" entry would convert `path`: documents only go
// to PDF, images only go to image formats other than their own.
bool converts_to(const std::filesystem::path& path, const std::wstring& formatExt);

} // namespace win11qc::shell
