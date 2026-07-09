#include "win11qc/cli_options.h"
#include "win11qc/wic_converter.h"

#include <iostream>
#include <string_view>

namespace win11qc {

static bool parse_integer(std::string_view text, int& value)
{
    if (text.empty()) {
        return false;
    }

    int sign = 1;
    std::size_t index = 0;
    if (text.front() == '+') {
        index = 1;
    } else if (text.front() == '-') {
        sign = -1;
        index = 1;
    }

    if (index >= text.size()) {
        return false;
    }

    int result = 0;
    for (; index < text.size(); ++index) {
        const char character = text[index];
        if (character < '0' || character > '9') {
            return false;
        }

        const int digit = character - '0';
        if (result > (2147483647 - digit) / 10) {
            return false;
        }

        result = result * 10 + digit;
    }

    value = result * sign;
    return true;
}

bool parse_arguments(int argc, char* argv[], CliOptions& options, std::string& errorMessage)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];

        if (argument == "--help" || argument == "-h") {
            options.showHelp = true;
            continue;
        }

        auto require_value = [&](std::string_view optionName) -> std::string_view {
            if (index + 1 >= argc) {
                errorMessage = "Missing value for option: " + std::string(optionName);
                return {};
            }
            ++index;
            return argv[index];
        };

        if (argument == "--input") {
            options.inputPath = std::filesystem::path{std::string(require_value(argument))};
            if (!errorMessage.empty()) {
                return false;
            }
            continue;
        }

        if (argument == "--output") {
            options.outputPath = std::filesystem::path{std::string(require_value(argument))};
            if (!errorMessage.empty()) {
                return false;
            }
            continue;
        }

        if (argument == "--format") {
            options.format = std::string(require_value(argument));
            if (!errorMessage.empty()) {
                return false;
            }
            continue;
        }

        if (argument == "--quality") {
            const std::string_view qualityText = require_value(argument);
            if (!errorMessage.empty()) {
                return false;
            }

            int quality = 0;
            if (!parse_integer(qualityText, quality) || quality < 0 || quality > 100) {
                errorMessage = "Invalid quality value. Expected an integer between 0 and 100.";
                return false;
            }

            options.quality = quality;
            continue;
        }

        errorMessage = "Unknown argument: " + std::string(argument);
        return false;
    }

    if (options.showHelp) {
        return true;
    }

    if (options.inputPath.empty()) {
        errorMessage = "Missing required option: --input";
        return false;
    }

    if (options.outputPath.empty()) {
        errorMessage = "Missing required option: --output";
        return false;
    }

    if (options.format.empty()) {
        errorMessage = "Missing required option: --format";
        return false;
    }

    return true;
}

void print_usage()
{
    std::cout << "win11-quick-converter CLI\n"
              << "Usage:\n"
              << "  converter-cli --input <path> --output <path> --format <name> [--quality <0-100>]\n\n"
              << "Options:\n"
              << "  --input     Source image path.\n"
              << "  --output    Destination path.\n"
              << "  --format    Output format name.\n"
              << "  --quality   Optional codec quality value.\n"
              << "  --help      Show this message.\n";
}

} // namespace win11qc

int main(int argc, char* argv[])
{
    win11qc::CliOptions options;
    std::string errorMessage;

    if (!win11qc::parse_arguments(argc, argv, options, errorMessage)) {
        std::cerr << errorMessage << '\n';
        std::cerr << "Run with --help for usage information.\n";
        return static_cast<int>(win11qc::ExitCode::InvalidArguments);
    }

    if (options.showHelp) {
        win11qc::print_usage();
        return static_cast<int>(win11qc::ExitCode::Success);
    }

    const win11qc::ConversionResult conversionResult = win11qc::convert_image(options);
    if (!conversionResult.message.empty()) {
        std::cout << conversionResult.message << '\n';
    }

    return static_cast<int>(conversionResult.exitCode);
}
