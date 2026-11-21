#include "win11qc/cli_options.h"
#include "win11qc/wic_converter.h"

#include <Windows.h>

#include <iostream>
#include <string>
#include <string_view>

namespace win11qc {

// Arguments arrive as UTF-16 through wmain; diagnostics are emitted as UTF-8.
static std::string utf8_from_wide(std::wstring_view text)
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
    WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

static bool parse_integer(std::wstring_view text, int& value)
{
    if (text.empty()) {
        return false;
    }

    int sign = 1;
    std::size_t index = 0;
    if (text.front() == L'+') {
        index = 1;
    } else if (text.front() == L'-') {
        sign = -1;
        index = 1;
    }

    if (index >= text.size()) {
        return false;
    }

    int result = 0;
    for (; index < text.size(); ++index) {
        const wchar_t character = text[index];
        if (character < L'0' || character > L'9') {
            return false;
        }

        const int digit = static_cast<int>(character - L'0');
        if (result > (2147483647 - digit) / 10) {
            return false;
        }

        result = result * 10 + digit;
    }

    value = result * sign;
    return true;
}

bool parse_arguments(int argc, wchar_t* argv[], CliOptions& options, std::string& errorMessage)
{
    for (int index = 1; index < argc; ++index) {
        const std::wstring_view argument = argv[index];

        if (argument == L"--help" || argument == L"-h") {
            options.showHelp = true;
            continue;
        }

        auto require_value = [&](std::wstring_view optionName) -> std::wstring_view {
            if (index + 1 >= argc) {
                errorMessage = "Missing value for option: " + utf8_from_wide(optionName);
                return {};
            }
            ++index;
            return argv[index];
        };

        if (argument == L"--input") {
            options.inputPath = std::filesystem::path{std::wstring(require_value(argument))};
            if (!errorMessage.empty()) {
                return false;
            }
            continue;
        }

        if (argument == L"--output") {
            options.outputPath = std::filesystem::path{std::wstring(require_value(argument))};
            if (!errorMessage.empty()) {
                return false;
            }
            continue;
        }

        if (argument == L"--format") {
            options.format = utf8_from_wide(require_value(argument));
            if (!errorMessage.empty()) {
                return false;
            }
            continue;
        }

        if (argument == L"--quality") {
            const std::wstring_view qualityText = require_value(argument);
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

        errorMessage = "Unknown argument: " + utf8_from_wide(argument);
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

// wmain receives the command line as UTF-16, so paths outside the ANSI code
// page (Japanese, emoji, ...) survive intact instead of degrading to '?'.
int wmain(int argc, wchar_t* argv[])
{
    // Messages contain UTF-8 encoded paths; make the console render them.
    SetConsoleOutputCP(CP_UTF8);

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
