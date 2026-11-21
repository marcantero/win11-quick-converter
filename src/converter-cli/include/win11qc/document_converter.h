#pragma once

#include "win11qc/cli_options.h"
#include "win11qc/wic_converter.h"

#include <filesystem>

namespace win11qc {

// True when the input is an office document (by extension) that goes through
// the document path instead of WIC.
bool is_document_input(const std::filesystem::path& inputPath);

// Converts an office document to PDF with LibreOffice. See ADR 0004.
ConversionResult convert_document(const CliOptions& options);

} // namespace win11qc
