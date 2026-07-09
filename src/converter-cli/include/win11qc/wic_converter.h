#pragma once

#include "win11qc/cli_options.h"

#include <string>

namespace win11qc {

struct ConversionResult {
    ExitCode exitCode = ExitCode::Success;
    std::string message;
};

ConversionResult convert_image(const CliOptions& options);

} // namespace win11qc
