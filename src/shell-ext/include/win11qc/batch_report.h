#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace win11qc::shell {

struct ConversionFailure {
    std::filesystem::path input;
    std::wstring reason;
};

// Outcome of one "To <format>" invocation, used to build the end-of-run toast.
struct BatchReport {
    std::wstring formatExt;                         // e.g. L"png"
    std::size_t attempted = 0;                      // files queued for conversion
    std::vector<std::filesystem::path> outputs;     // successfully written files
    std::vector<ConversionFailure> failures;
    bool cancelled = false;
};

// Display name of a target format: L"png" -> L"PNG", L"webp" -> L"WebP".
std::wstring format_display_name(const std::wstring& formatExt);

// Batches always get a summary toast. A single file only gets one when it
// fails, so routine one-off conversions stay silent.
bool should_notify(const BatchReport& report);

// Toast XML (ToastGeneric) for the report, with protocol-activated
// "Open folder" / "Open image" actions when the outputs allow them.
std::wstring build_toast_xml(const BatchReport& report);

// Failure reason for a converter-cli run that exited with `exitCode`: the
// first non-empty line it printed (UTF-8), or a generic message.
std::wstring cli_failure_reason(const std::string& utf8Output, unsigned long exitCode);

// Helpers exposed for testing.
std::wstring xml_escape(const std::wstring& text);
std::wstring file_uri(const std::filesystem::path& path);

} // namespace win11qc::shell
