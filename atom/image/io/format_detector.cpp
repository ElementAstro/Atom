#include "format_detector.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>

// Define error macros to avoid atom error system namespace pollution
#define THROW_RUNTIME_ERROR(msg) throw std::runtime_error(msg)

namespace atom::image {

FormatDetector::FormatDetector() { initializeSignatures(); }

void FormatDetector::initializeSignatures() {
    // JPEG signatures
    signatures_.push_back({{0xFF, 0xD8, 0xFF},
                           0,
                           ImageFormat::JPEG,
                           DetectionConfidence::CERTAIN,
                           "JPEG image"});

    // PNG signature
    signatures_.push_back({{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A},
                           0,
                           ImageFormat::PNG,
                           DetectionConfidence::CERTAIN,
                           "PNG image"});

    // BMP signatures
    signatures_.push_back({{0x42, 0x4D},
                           0,
                           ImageFormat::BMP,
                           DetectionConfidence::CERTAIN,
                           "BMP image"});

    // TIFF signatures (little and big endian)
    signatures_.push_back({{0x49, 0x49, 0x2A, 0x00},
                           0,
                           ImageFormat::TIFF,
                           DetectionConfidence::CERTAIN,
                           "TIFF image (little endian)"});
    signatures_.push_back({{0x4D, 0x4D, 0x00, 0x2A},
                           0,
                           ImageFormat::TIFF,
                           DetectionConfidence::CERTAIN,
                           "TIFF image (big endian)"});

    // GIF signatures
    signatures_.push_back({{0x47, 0x49, 0x46, 0x38, 0x37, 0x61},
                           0,
                           ImageFormat::GIF,
                           DetectionConfidence::CERTAIN,
                           "GIF87a image"});
    signatures_.push_back({{0x47, 0x49, 0x46, 0x38, 0x39, 0x61},
                           0,
                           ImageFormat::GIF,
                           DetectionConfidence::CERTAIN,
                           "GIF89a image"});

    // WebP signature
    signatures_.push_back({{0x52, 0x49, 0x46, 0x46},
                           0,
                           ImageFormat::WEBP,
                           DetectionConfidence::HIGH,
                           "WebP container"});
    signatures_.push_back({{0x57, 0x45, 0x42, 0x50},
                           8,
                           ImageFormat::WEBP,
                           DetectionConfidence::CERTAIN,
                           "WebP image"});

    // AVIF signature (ftyp box with AVIF brand)
    signatures_.push_back({{0x66, 0x74, 0x79, 0x70, 0x61, 0x76, 0x69, 0x66},
                           4,
                           ImageFormat::AVIF,
                           DetectionConfidence::CERTAIN,
                           "AVIF image"});

    // HEIF signature
    signatures_.push_back({{0x66, 0x74, 0x79, 0x70, 0x68, 0x65, 0x69, 0x63},
                           4,
                           ImageFormat::HEIF,
                           DetectionConfidence::CERTAIN,
                           "HEIF image"});

    // TGA signature (no reliable magic number, check footer)
    // Note: TGA footer detection would need special handling for negative
    // offsets

    // PSD signature
    signatures_.push_back({{0x38, 0x42, 0x50, 0x53},
                           0,
                           ImageFormat::PSD,
                           DetectionConfidence::CERTAIN,
                           "Photoshop PSD"});

    // HDR signature
    signatures_.push_back(
        {{0x23, 0x3F, 0x52, 0x41, 0x44, 0x49, 0x41, 0x4E, 0x43, 0x45, 0x0A},
         0,
         ImageFormat::HDR,
         DetectionConfidence::CERTAIN,
         "Radiance HDR"});

    // EXR signature
    signatures_.push_back({{0x76, 0x2F, 0x31, 0x01},
                           0,
                           ImageFormat::EXR,
                           DetectionConfidence::CERTAIN,
                           "OpenEXR image"});

    // FITS signature
    signatures_.push_back(
        {{0x53, 0x49, 0x4D, 0x50, 0x4C, 0x45, 0x20, 0x20, 0x3D, 0x20,
          0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x54},
         0,
         ImageFormat::FITS,
         DetectionConfidence::CERTAIN,
         "FITS astronomical image"});

    // SER signature
    signatures_.push_back({{0x4C, 0x55, 0x43, 0x41, 0x4D, 0x2D, 0x52, 0x45,
                            0x43, 0x4F, 0x52, 0x44, 0x45, 0x52},
                           0,
                           ImageFormat::SER,
                           DetectionConfidence::CERTAIN,
                           "SER video sequence"});

    // DICOM signature
    signatures_.push_back({{0x44, 0x49, 0x43, 0x4D},
                           128,
                           ImageFormat::DCM,
                           DetectionConfidence::CERTAIN,
                           "DICOM medical image"});

    // Initialize extension mappings
    extensionMap_ = {{"jpg", ImageFormat::JPEG},   {"jpeg", ImageFormat::JPEG},
                     {"jpe", ImageFormat::JPEG},   {"png", ImageFormat::PNG},
                     {"bmp", ImageFormat::BMP},    {"dib", ImageFormat::BMP},
                     {"tif", ImageFormat::TIFF},   {"tiff", ImageFormat::TIFF},
                     {"gif", ImageFormat::GIF},    {"webp", ImageFormat::WEBP},
                     {"avif", ImageFormat::AVIF},  {"heif", ImageFormat::HEIF},
                     {"heic", ImageFormat::HEIF},  {"tga", ImageFormat::TGA},
                     {"targa", ImageFormat::TGA},  {"psd", ImageFormat::PSD},
                     {"hdr", ImageFormat::HDR},    {"pic", ImageFormat::HDR},
                     {"exr", ImageFormat::EXR},    {"fits", ImageFormat::FITS},
                     {"fit", ImageFormat::FITS},   {"fts", ImageFormat::FITS},
                     {"ser", ImageFormat::SER},    {"dcm", ImageFormat::DCM},
                     {"dicom", ImageFormat::DCM},  {"nii", ImageFormat::NII},
                     {"nii.gz", ImageFormat::NII}, {"h5", ImageFormat::HDF5},
                     {"hdf5", ImageFormat::HDF5},  {"raw", ImageFormat::RAW},
                     {"dng", ImageFormat::DNG},    {"cr2", ImageFormat::CR2},
                     {"nef", ImageFormat::NEF},    {"arw", ImageFormat::ARW},
                     {"orf", ImageFormat::ORF},    {"rw2", ImageFormat::RW2},
                     {"raf", ImageFormat::RAF},    {"pef", ImageFormat::PEF},
                     {"x3f", ImageFormat::X3F}};

    // Initialize MIME type mappings
    mimeTypeMap_ = {{ImageFormat::JPEG, "image/jpeg"},
                    {ImageFormat::PNG, "image/png"},
                    {ImageFormat::BMP, "image/bmp"},
                    {ImageFormat::TIFF, "image/tiff"},
                    {ImageFormat::GIF, "image/gif"},
                    {ImageFormat::WEBP, "image/webp"},
                    {ImageFormat::AVIF, "image/avif"},
                    {ImageFormat::HEIF, "image/heif"},
                    {ImageFormat::TGA, "image/x-tga"},
                    {ImageFormat::PSD, "image/vnd.adobe.photoshop"},
                    {ImageFormat::HDR, "image/vnd.radiance"},
                    {ImageFormat::EXR, "image/x-exr"},
                    {ImageFormat::FITS, "image/fits"},
                    {ImageFormat::SER, "application/x-ser"},
                    {ImageFormat::DCM, "application/dicom"},
                    {ImageFormat::RAW, "image/x-canon-raw"},
                    {ImageFormat::DNG, "image/x-adobe-dng"}};

    // Initialize format names
    formatNameMap_ = {
        {ImageFormat::UNKNOWN, "Unknown"}, {ImageFormat::JPEG, "JPEG"},
        {ImageFormat::PNG, "PNG"},         {ImageFormat::BMP, "BMP"},
        {ImageFormat::TIFF, "TIFF"},       {ImageFormat::GIF, "GIF"},
        {ImageFormat::WEBP, "WebP"},       {ImageFormat::AVIF, "AVIF"},
        {ImageFormat::HEIF, "HEIF"},       {ImageFormat::TGA, "TGA"},
        {ImageFormat::PSD, "PSD"},         {ImageFormat::HDR, "HDR"},
        {ImageFormat::EXR, "EXR"},         {ImageFormat::FITS, "FITS"},
        {ImageFormat::SER, "SER"},         {ImageFormat::DCM, "DICOM"},
        {ImageFormat::RAW, "RAW"},         {ImageFormat::DNG, "DNG"}};
}

FormatDetectionResult FormatDetector::detectFromFile(
    const std::filesystem::path& filePath) const {
    if (!std::filesystem::exists(filePath)) {
        THROW_RUNTIME_ERROR("File does not exist: " + filePath.string());
    }

    // First try extension-based detection
    auto extensionResult = detectFromExtension(filePath.extension().string());

    // Then try magic number detection
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        THROW_RUNTIME_ERROR("Cannot open file: " + filePath.string());
    }

    // Read first 1KB for magic number detection
    constexpr size_t bufferSize = 1024;
    std::vector<uint8_t> buffer(bufferSize);
    file.read(reinterpret_cast<char*>(buffer.data()), bufferSize);
    size_t bytesRead = static_cast<size_t>(file.gcount());

    auto magicResult = detectFromMemory(buffer.data(), bytesRead);

    // Combine results - prefer magic number detection if confident
    FormatDetectionResult result;
    if (magicResult.confidence >= DetectionConfidence::HIGH) {
        result = magicResult;
    } else if (extensionResult.confidence >= DetectionConfidence::MEDIUM) {
        result = extensionResult;
        // But update confidence if magic detection found something
        if (magicResult.format != ImageFormat::UNKNOWN) {
            result.confidence =
                std::min(result.confidence, magicResult.confidence);
        }
    } else {
        result = magicResult.format != ImageFormat::UNKNOWN ? magicResult
                                                            : extensionResult;
    }

    return result;
}

FormatDetectionResult FormatDetector::detectFromMemory(const void* data,
                                                       size_t size) const {
    if (!data || size == 0) {
        return {};
    }

    const auto* bytes = static_cast<const uint8_t*>(data);

    // Check all signatures
    for (const auto& signature : signatures_) {
        if (checkSignature(bytes, size, signature)) {
            FormatDetectionResult result;
            result.format = signature.format;
            result.confidence = signature.confidence;
            result.description = signature.description;
            result.mimeType = getMimeType(signature.format);
            result.possibleExtensions = getExtensions(signature.format);
            result.metadata = analyzeHeader(bytes, size, signature.format);
            return result;
        }
    }

    return {};
}

FormatDetectionResult FormatDetector::detectFromBlob(
    const blob& imageBlob) const {
    // Use begin() since data() method may not be available
    return detectFromMemory(
        reinterpret_cast<const uint8_t*>(&(*imageBlob.begin())),
        imageBlob.size());
}

FormatDetectionResult FormatDetector::detectFromExtension(
    const std::string& extension) const {
    std::string ext = extension;

    // Remove leading dot if present
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }

    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    auto it = extensionMap_.find(ext);
    if (it != extensionMap_.end()) {
        FormatDetectionResult result;
        result.format = it->second;
        result.confidence = DetectionConfidence::MEDIUM;
        result.mimeType = getMimeType(it->second);
        result.possibleExtensions = getExtensions(it->second);
        result.description = getFormatName(it->second) + " (by extension)";
        return result;
    }

    return {};
}

std::string FormatDetector::getMimeType(ImageFormat format) const {
    auto it = mimeTypeMap_.find(format);
    return it != mimeTypeMap_.end() ? it->second : "application/octet-stream";
}

std::vector<std::string> FormatDetector::getExtensions(
    ImageFormat format) const {
    std::vector<std::string> extensions;
    for (const auto& [ext, fmt] : extensionMap_) {
        if (fmt == format) {
            extensions.push_back(ext);
        }
    }
    return extensions;
}

bool FormatDetector::isReadSupported(ImageFormat format) const {
    // Most formats are supported for reading
    switch (format) {
        case ImageFormat::UNKNOWN:
        case ImageFormat::CUSTOM:
            return false;
        default:
            return true;
    }
}

bool FormatDetector::isWriteSupported(ImageFormat format) const {
    // Common formats supported for writing
    switch (format) {
        case ImageFormat::JPEG:
        case ImageFormat::PNG:
        case ImageFormat::BMP:
        case ImageFormat::TIFF:
        case ImageFormat::TGA:
        case ImageFormat::HDR:
        case ImageFormat::FITS:
            return true;
        default:
            return false;
    }
}

void FormatDetector::registerCustomFormat(const MagicSignature& signature) {
    signatures_.push_back(signature);
}

std::string FormatDetector::getFormatName(ImageFormat format) const {
    auto it = formatNameMap_.find(format);
    return it != formatNameMap_.end() ? it->second : "Unknown";
}

std::vector<ImageFormat> FormatDetector::getSupportedFormats() const {
    std::vector<ImageFormat> formats;
    for (const auto& [format, name] : formatNameMap_) {
        if (format != ImageFormat::UNKNOWN) {
            formats.push_back(format);
        }
    }
    return formats;
}

bool FormatDetector::checkSignature(const uint8_t* data, size_t size,
                                    const MagicSignature& signature) const {
    size_t offset = signature.offset;

    // Handle negative offsets (from end of file)
    if (signature.offset < 0) {
        if (size < static_cast<size_t>(-signature.offset)) {
            return false;
        }
        offset = size + signature.offset;
    }

    if (offset + signature.signature.size() > size) {
        return false;
    }

    return std::equal(signature.signature.begin(), signature.signature.end(),
                      data + offset);
}

std::unordered_map<std::string, std::string> FormatDetector::analyzeHeader(
    const uint8_t* data, size_t size, ImageFormat format) const {
    std::unordered_map<std::string, std::string> metadata;

    // Basic format-specific header analysis
    switch (format) {
        case ImageFormat::JPEG:
            // Could extract EXIF data here
            metadata["format"] = "JPEG";
            break;
        case ImageFormat::PNG:
            // Could extract PNG chunks here
            metadata["format"] = "PNG";
            break;
        case ImageFormat::FITS:
            // Could extract FITS header keywords here
            metadata["format"] = "FITS";
            break;
        default:
            metadata["format"] = getFormatName(format);
            break;
    }

    return metadata;
}

std::unique_ptr<FormatDetector> createFormatDetector() {
    return std::make_unique<FormatDetector>();
}

ImageFormat quickDetectFormat(const std::filesystem::path& filePath) {
    auto detector = createFormatDetector();
    return detector->detectFromFile(filePath).format;
}

ImageFormat quickDetectFormat(const void* data, size_t size) {
    auto detector = createFormatDetector();
    return detector->detectFromMemory(data, size).format;
}

}  // namespace atom::image
