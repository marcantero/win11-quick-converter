#define NOMINMAX

#include "win11qc/wic_converter.h"

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <webp/encode.h>
#include <fstream>
#include <algorithm>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <memory>
#include <new>
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

ConversionResult conversion_failure(std::string message)
{
    return {ExitCode::ConversionFailed, std::move(message)};
}

ConversionResult conversion_failure(std::string_view what, HRESULT hr)
{
    return conversion_failure(std::string(what) + ": " + format_hresult(hr));
}

// The decoded first frame of the input image, shared by the WIC and WebP paths.
struct SourceImage {
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapFrameDecode> frame;
    UINT width = 0;
    UINT height = 0;
};

// Validates the input and output paths and decodes the first frame of the input.
// COM must already be initialized on the calling thread.
std::optional<ConversionResult> open_source_image(const CliOptions& options, SourceImage& source)
{
    std::error_code errorCode;
    if (!std::filesystem::exists(options.inputPath, errorCode)) {
        return ConversionResult{ExitCode::ValidationFailed, "Input file does not exist: " + narrow_from_path(options.inputPath)};
    }

    if (!std::filesystem::is_regular_file(options.inputPath, errorCode)) {
        return ConversionResult{ExitCode::ValidationFailed, "Input path is not a regular file: " + narrow_from_path(options.inputPath)};
    }

    std::string message;
    if (!ensure_parent_directory(options.outputPath, message)) {
        return ConversionResult{ExitCode::ValidationFailed, message};
    }

    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&source.factory));
    if (FAILED(hr)) {
        return conversion_failure("Failed to create WIC imaging factory", hr);
    }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = source.factory->CreateDecoderFromFilename(
        options.inputPath.wstring().c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder);
    if (FAILED(hr)) {
        return conversion_failure("Failed to open input image", hr);
    }

    hr = decoder->GetFrame(0, &source.frame);
    if (FAILED(hr)) {
        return conversion_failure("Failed to read the first frame from the input image", hr);
    }

    hr = source.frame->GetSize(&source.width, &source.height);
    if (FAILED(hr)) {
        return conversion_failure("Failed to read source image dimensions", hr);
    }

    if (source.width == 0 || source.height == 0) {
        return conversion_failure("Input image has no pixels.");
    }

    return std::nullopt;
}

ConversionResult convert_with_wic(
    const CliOptions& options,
    const ContainerFormatInfo& containerFormat,
    const SourceImage& source)
{
    IWICImagingFactory* factory = source.factory.Get();

    ComPtr<IWICStream> outputStream;
    HRESULT hr = factory->CreateStream(&outputStream);
    if (FAILED(hr)) {
        return conversion_failure("Failed to create WIC output stream", hr);
    }

    hr = outputStream->InitializeFromFilename(options.outputPath.wstring().c_str(), GENERIC_WRITE);
    if (FAILED(hr)) {
        return conversion_failure("Failed to initialize output stream", hr);
    }

    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(*containerFormat.containerGuid, nullptr, &encoder);
    if (FAILED(hr)) {
        return conversion_failure("Failed to create output encoder", hr);
    }

    hr = encoder->Initialize(outputStream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        return conversion_failure("Failed to initialize output encoder", hr);
    }

    ComPtr<IWICBitmapFrameEncode> frameEncode;
    ComPtr<IPropertyBag2> propertyBag;
    hr = encoder->CreateNewFrame(&frameEncode, &propertyBag);
    if (FAILED(hr)) {
        return conversion_failure("Failed to create destination frame", hr);
    }

    if (containerFormat.supportsQuality && options.quality.has_value()) {
        if (!set_jpeg_quality(propertyBag.Get(), *options.quality)) {
            return conversion_failure("Failed to set JPEG quality options.");
        }
    }

    hr = frameEncode->Initialize(propertyBag.Get());
    if (FAILED(hr)) {
        return conversion_failure("Failed to initialize destination frame", hr);
    }

    hr = frameEncode->SetSize(source.width, source.height);
    if (FAILED(hr)) {
        return conversion_failure("Failed to set destination frame size", hr);
    }

    WICPixelFormatGUID pixelFormat = containerFormat.pixelFormat;
    hr = frameEncode->SetPixelFormat(&pixelFormat);
    if (FAILED(hr)) {
        return conversion_failure("Failed to set destination pixel format", hr);
    }

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        return conversion_failure("Failed to create WIC format converter", hr);
    }

    hr = converter->Initialize(
        source.frame.Get(),
        pixelFormat,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        return conversion_failure("Failed to initialize image format conversion", hr);
    }

    hr = frameEncode->WriteSource(converter.Get(), nullptr);
    if (FAILED(hr)) {
        return conversion_failure("Failed to write encoded image data", hr);
    }

    hr = frameEncode->Commit();
    if (FAILED(hr)) {
        return conversion_failure("Failed to commit the output frame", hr);
    }

    hr = encoder->Commit();
    if (FAILED(hr)) {
        return conversion_failure("Failed to commit the encoder output", hr);
    }

    return {
        ExitCode::Success,
        "Converted " + narrow_from_path(options.inputPath) + " to " + narrow_from_path(options.outputPath) +
            " using " + containerFormat.canonicalName + " encoding."
    };
}

ConversionResult convert_to_webp(const CliOptions& options, const SourceImage& source)
{
    const UINT width = source.width;
    const UINT height = source.height;

    if (width > WEBP_MAX_DIMENSION || height > WEBP_MAX_DIMENSION) {
        return conversion_failure(
            "Image is " + std::to_string(width) + "x" + std::to_string(height) + " pixels; WebP supports at most " +
            std::to_string(WEBP_MAX_DIMENSION) + "x" + std::to_string(WEBP_MAX_DIMENSION) + ".");
    }

    // Compute the buffer size in size_t and make sure it also fits the UINT that CopyPixels takes.
    constexpr std::size_t kBytesPerPixel = 4;
    const std::size_t stride = static_cast<std::size_t>(width) * kBytesPerPixel;
    if (stride > std::numeric_limits<UINT>::max() || height > std::numeric_limits<UINT>::max() / stride) {
        return conversion_failure("Image is too large to convert.");
    }
    const std::size_t bufferSize = stride * height;

    std::vector<std::uint8_t> pixels;
    try {
        pixels.resize(bufferSize);
    } catch (const std::bad_alloc&) {
        return conversion_failure(
            "Not enough memory to convert a " + std::to_string(width) + "x" + std::to_string(height) + " image.");
    }

    ComPtr<IWICFormatConverter> converter;
    HRESULT hr = source.factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        return conversion_failure("Failed to create WIC format converter", hr);
    }

    hr = converter->Initialize(
        source.frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        return conversion_failure("Failed to initialize RGBA format conversion", hr);
    }

    hr = converter->CopyPixels(nullptr, static_cast<UINT>(stride), static_cast<UINT>(bufferSize), pixels.data());
    if (FAILED(hr)) {
        return conversion_failure("Failed to copy pixels from the input image", hr);
    }

    // Dimensions are bounded by WEBP_MAX_DIMENSION, so these fit in int.
    const int webpWidth = static_cast<int>(width);
    const int webpHeight = static_cast<int>(height);
    const int webpStride = static_cast<int>(stride);

    std::uint8_t* rawWebpData = nullptr;
    std::size_t webpSize = 0;
    if (options.quality.has_value() && *options.quality < 100) {
        webpSize = WebPEncodeRGBA(
            pixels.data(), webpWidth, webpHeight, webpStride, static_cast<float>(*options.quality), &rawWebpData);
    } else {
        webpSize = WebPEncodeLosslessRGBA(pixels.data(), webpWidth, webpHeight, webpStride, &rawWebpData);
    }
    const std::unique_ptr<std::uint8_t, decltype(&WebPFree)> webpData(rawWebpData, &WebPFree);

    if (webpSize == 0 || !webpData) {
        return conversion_failure("Failed to encode WebP image.");
    }

    std::ofstream outFile(options.outputPath, std::ios::binary | std::ios::trunc);
    if (!outFile) {
        return conversion_failure("Failed to open output file: " + narrow_from_path(options.outputPath));
    }
    outFile.write(reinterpret_cast<const char*>(webpData.get()), static_cast<std::streamsize>(webpSize));
    outFile.close();
    if (!outFile) {
        // Do not leave a truncated image behind.
        std::error_code removeError;
        std::filesystem::remove(options.outputPath, removeError);
        return conversion_failure("Failed to write output file: " + narrow_from_path(options.outputPath));
    }

    return {
        ExitCode::Success,
        "Converted " + narrow_from_path(options.inputPath) + " to " + narrow_from_path(options.outputPath) +
            " using WebP encoding."
    };
}

} // namespace

ConversionResult convert_image(const CliOptions& options)
{
    // Writing over the source would either fail with a sharing violation (WIC)
    // or truncate the input after decoding (WebP), so refuse it up front.
    // equivalent() resolves case, relative segments, short names and links.
    std::error_code errorCode;
    if (std::filesystem::equivalent(options.inputPath, options.outputPath, errorCode) && !errorCode) {
        return {
            ExitCode::ValidationFailed,
            "Input and output refer to the same file: " + narrow_from_path(options.inputPath)
        };
    }

    const bool toWebp = to_lower_ascii(options.format) == "webp";
    const std::optional<ContainerFormatInfo> containerFormat =
        toWebp ? std::nullopt : resolve_container_format(options.format);
    if (!toWebp && !containerFormat.has_value()) {
        return {
            ExitCode::ValidationFailed,
            "Unsupported output format: " + options.format + ". Supported formats are png, jpg, jpeg, bmp, tif, tiff, and webp."
        };
    }

    ScopedComInitialization comInitialization;
    if (!comInitialization.initialized()) {
        return conversion_failure("Failed to initialize COM for WIC processing.");
    }

    SourceImage source;
    if (std::optional<ConversionResult> failure = open_source_image(options, source)) {
        return *failure;
    }

    return toWebp ? convert_to_webp(options, source) : convert_with_wic(options, *containerFormat, source);
}

} // namespace win11qc
