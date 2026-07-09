#define NOMINMAX

#include "win11qc/wic_converter.h"

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace win11qc {
namespace {

using Microsoft::WRL::ComPtr;

class ScopedComInitialization {
public:
    ScopedComInitialization()
        : initialized_(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
    {
    }

    ~ScopedComInitialization()
    {
        if (initialized_) {
            CoUninitialize();
        }
    }

    ScopedComInitialization(const ScopedComInitialization&) = delete;
    ScopedComInitialization& operator=(const ScopedComInitialization&) = delete;

    bool initialized() const noexcept
    {
        return initialized_;
    }

private:
    bool initialized_ = false;
};

std::string to_lower_ascii(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::string narrow_from_path(const std::filesystem::path& path)
{
    const std::wstring widePath = path.wstring();
    if (widePath.empty()) {
        return {};
    }

    const int requiredSize = WideCharToMultiByte(
        CP_UTF8,
        0,
        widePath.c_str(),
        static_cast<int>(widePath.size()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (requiredSize <= 0) {
        return {};
    }

    std::string utf8Path(static_cast<std::size_t>(requiredSize), '\0');
    const int convertedSize = WideCharToMultiByte(
        CP_UTF8,
        0,
        widePath.c_str(),
        static_cast<int>(widePath.size()),
        utf8Path.data(),
        requiredSize,
        nullptr,
        nullptr);

    if (convertedSize <= 0) {
        return {};
    }

    return utf8Path;
}

std::string format_hresult(HRESULT hr)
{
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
           << static_cast<unsigned long>(hr);
    return stream.str();
}

struct ContainerFormatInfo {
    const char* canonicalName = nullptr;
    const GUID* containerGuid = nullptr;
    GUID pixelFormat = GUID_WICPixelFormat32bppBGRA;
    bool supportsQuality = false;
};

std::optional<ContainerFormatInfo> resolve_container_format(std::string_view formatName)
{
    const std::string normalized = to_lower_ascii(std::string(formatName));

    if (normalized == "png") {
        return ContainerFormatInfo{"PNG", &GUID_ContainerFormatPng, GUID_WICPixelFormat32bppBGRA, false};
    }

    if (normalized == "jpg" || normalized == "jpeg") {
        return ContainerFormatInfo{"JPEG", &GUID_ContainerFormatJpeg, GUID_WICPixelFormat24bppBGR, true};
    }

    if (normalized == "bmp") {
        return ContainerFormatInfo{"BMP", &GUID_ContainerFormatBmp, GUID_WICPixelFormat32bppBGRA, false};
    }

    if (normalized == "tif" || normalized == "tiff") {
        return ContainerFormatInfo{"TIFF", &GUID_ContainerFormatTiff, GUID_WICPixelFormat32bppBGRA, false};
    }

    return std::nullopt;
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
        errorMessage = "Failed to create output directory: " + narrow_from_path(parentPath);
        return false;
    }

    return true;
}

bool set_jpeg_quality(IPropertyBag2* propertyBag, int qualityPercent)
{
    PROPBAG2 option = {};
    option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");

    VARIANT qualityValue;
    VariantInit(&qualityValue);
    qualityValue.vt = VT_R4;
    qualityValue.fltVal = static_cast<FLOAT>(qualityPercent) / 100.0f;

    const HRESULT writeResult = propertyBag->Write(1, &option, &qualityValue);
    VariantClear(&qualityValue);
    return SUCCEEDED(writeResult);
}

ConversionResult convert_with_wic(const CliOptions& options, const ContainerFormatInfo& containerFormat)
{
    ConversionResult result;

    std::error_code errorCode;
    if (!std::filesystem::exists(options.inputPath, errorCode)) {
        result.exitCode = ExitCode::ValidationFailed;
        result.message = "Input file does not exist: " + narrow_from_path(options.inputPath);
        return result;
    }

    if (!std::filesystem::is_regular_file(options.inputPath, errorCode)) {
        result.exitCode = ExitCode::ValidationFailed;
        result.message = "Input path is not a regular file: " + narrow_from_path(options.inputPath);
        return result;
    }

    if (!ensure_parent_directory(options.outputPath, result.message)) {
        result.exitCode = ExitCode::ValidationFailed;
        return result;
    }

    ScopedComInitialization comInitialization;
    if (!comInitialization.initialized()) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to initialize COM for WIC processing.";
        return result;
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to create WIC imaging factory: " + format_hresult(hr);
        return result;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(
        options.inputPath.wstring().c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder);
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to open input image: " + format_hresult(hr);
        return result;
    }

    ComPtr<IWICBitmapFrameDecode> sourceFrame;
    hr = decoder->GetFrame(0, &sourceFrame);
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to read the first frame from the input image: " + format_hresult(hr);
        return result;
    }

    UINT width = 0;
    UINT height = 0;
    hr = sourceFrame->GetSize(&width, &height);
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to read source image dimensions: " + format_hresult(hr);
        return result;
    }

    ComPtr<IWICStream> outputStream;
    hr = factory->CreateStream(&outputStream);
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to create WIC output stream: " + format_hresult(hr);
        return result;
    }

    hr = outputStream->InitializeFromFilename(options.outputPath.wstring().c_str(), GENERIC_WRITE);
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to initialize output stream: " + format_hresult(hr);
        return result;
    }

    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(*containerFormat.containerGuid, nullptr, &encoder);
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to create output encoder: " + format_hresult(hr);
        return result;
    }

    hr = encoder->Initialize(outputStream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to initialize output encoder: " + format_hresult(hr);
        return result;
    }

    ComPtr<IWICBitmapFrameEncode> frameEncode;
    ComPtr<IPropertyBag2> propertyBag;
    hr = encoder->CreateNewFrame(&frameEncode, &propertyBag);
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to create destination frame: " + format_hresult(hr);
        return result;
    }

    if (containerFormat.supportsQuality && options.quality.has_value()) {
        if (!set_jpeg_quality(propertyBag.Get(), *options.quality)) {
            result.exitCode = ExitCode::ConversionFailed;
            result.message = "Failed to set JPEG quality options.";
            return result;
        }
    }

    hr = frameEncode->Initialize(propertyBag.Get());
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to initialize destination frame: " + format_hresult(hr);
        return result;
    }

    hr = frameEncode->SetSize(width, height);
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to set destination frame size: " + format_hresult(hr);
        return result;
    }

    WICPixelFormatGUID pixelFormat = containerFormat.pixelFormat;
    hr = frameEncode->SetPixelFormat(&pixelFormat);
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to set destination pixel format: " + format_hresult(hr);
        return result;
    }

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to create WIC format converter: " + format_hresult(hr);
        return result;
    }

    hr = converter->Initialize(
        sourceFrame.Get(),
        pixelFormat,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to initialize image format conversion: " + format_hresult(hr);
        return result;
    }

    hr = frameEncode->WriteSource(converter.Get(), nullptr);
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to write encoded image data: " + format_hresult(hr);
        return result;
    }

    hr = frameEncode->Commit();
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to commit the output frame: " + format_hresult(hr);
        return result;
    }

    hr = encoder->Commit();
    if (FAILED(hr)) {
        result.exitCode = ExitCode::ConversionFailed;
        result.message = "Failed to commit the encoder output: " + format_hresult(hr);
        return result;
    }

    result.exitCode = ExitCode::Success;
    result.message = "Converted " + narrow_from_path(options.inputPath) + " to " + narrow_from_path(options.outputPath) +
        " using " + containerFormat.canonicalName + " encoding.";
    return result;
}

} // namespace

ConversionResult convert_image(const CliOptions& options)
{
    const std::optional<ContainerFormatInfo> containerFormat = resolve_container_format(options.format);
    if (!containerFormat.has_value()) {
        return {
            ExitCode::ValidationFailed,
            "Unsupported output format: " + options.format + ". Supported formats are png, jpg, jpeg, bmp, tif, and tiff."
        };
    }

    return convert_with_wic(options, *containerFormat);
}

} // namespace win11qc
