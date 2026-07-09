#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace win11qc {

enum class ExitCode : int {
    Success = 0,
    InvalidArguments = 1,
    ValidationFailed = 2,
    ConversionFailed = 3,
};

struct CliOptions {
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;
    std::string format;
    std::optional<int> quality;
    bool showHelp = false;
};

bool parse_arguments(int argc, char* argv[], CliOptions& options, std::string& errorMessage);
void print_usage();

} // namespace win11qc
