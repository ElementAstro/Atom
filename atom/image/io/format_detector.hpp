#ifndef ATOM_IMAGE_FORMAT_DETECTOR_HPP
#define ATOM_IMAGE_FORMAT_DETECTOR_HPP

/**
 * @file format_detector.hpp
 * @brief Image format detection and identification
 *
 * This module provides comprehensive image format detection capabilities
 * based on file headers, magic numbers, and file extensions.
 *
 * @author Atom Framework Team
 * @date 2025
 * @version 1.0.0
 */

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "../core/image_blob.hpp"

namespace atom::image {

/**
 * @brief Supported image formats
 */
enum class ImageFormat {
    UNKNOWN,
    JPEG,
    PNG,
    BMP,
    TIFF,
    GIF,
    WEBP,
    AVIF,
    HEIF,
    TGA,
    PSD,
    HDR,
    EXR,
    FITS,
    SER,
    RAW,
    DNG,
    CR2,
    NEF,
    ARW,
    ORF,
    RW2,
    RAF,
    PEF,
    X3F,
    DCM,  // DICOM
    NII,  // NIfTI
    HDF5,
    CUSTOM
};

/**
 * @brief Format detection confidence levels
 */
enum class DetectionConfidence {
    NONE = 0,
    LOW = 25,
    MEDIUM = 50,
    HIGH = 75,
    CERTAIN = 100
};

/**
 * @brief Format detection result
 */
struct FormatDetectionResult {
    ImageFormat format = ImageFormat::UNKNOWN;
    DetectionConfidence confidence = DetectionConfidence::NONE;
    std::string mimeType;
    std::string description;
    std::vector<std::string> possibleExtensions;
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief Magic number signature for format detection
 */
struct MagicSignature {
    std::vector<uint8_t> signature;
    size_t offset = 0;
    ImageFormat format;
    DetectionConfidence confidence;
    std::string description;
};

/**
 * @brief Advanced image format detector
 */
class FormatDetector {
public:
    FormatDetector();
    virtual ~FormatDetector() = default;

    /**
     * @brief Detect format from file path
     * @param filePath Path to the image file
     * @return Format detection result
     */
    virtual FormatDetectionResult detectFromFile(
        const std::filesystem::path& filePath) const;

    /**
     * @brief Detect format from memory buffer
     * @param data Pointer to image data
     * @param size Size of the data buffer
     * @return Format detection result
     */
    virtual FormatDetectionResult detectFromMemory(const void* data,
                                                   size_t size) const;

    /**
     * @brief Detect format from blob
     * @param imageBlob Image blob to analyze
     * @return Format detection result
     */
    virtual FormatDetectionResult detectFromBlob(const blob& imageBlob) const;

    /**
     * @brief Detect format from file extension
     * @param extension File extension (with or without dot)
     * @return Format detection result
     */
    virtual FormatDetectionResult detectFromExtension(
        const std::string& extension) const;

    /**
     * @brief Get MIME type for format
     * @param format Image format
     * @return MIME type string
     */
    virtual std::string getMimeType(ImageFormat format) const;

    /**
     * @brief Get file extensions for format
     * @param format Image format
     * @return Vector of possible extensions
     */
    virtual std::vector<std::string> getExtensions(ImageFormat format) const;

    /**
     * @brief Check if format is supported for reading
     * @param format Image format to check
     * @return True if format can be read
     */
    virtual bool isReadSupported(ImageFormat format) const;

    /**
     * @brief Check if format is supported for writing
     * @param format Image format to check
     * @return True if format can be written
     */
    virtual bool isWriteSupported(ImageFormat format) const;

    /**
     * @brief Register custom format detector
     * @param signature Magic signature for detection
     */
    virtual void registerCustomFormat(const MagicSignature& signature);

    /**
     * @brief Get format name as string
     * @param format Image format
     * @return Format name
     */
    virtual std::string getFormatName(ImageFormat format) const;

    /**
     * @brief Get all supported formats
     * @return Vector of supported formats
     */
    virtual std::vector<ImageFormat> getSupportedFormats() const;

protected:
    /**
     * @brief Initialize built-in format signatures
     */
    virtual void initializeSignatures();

    /**
     * @brief Check magic signature against data
     * @param data Data buffer to check
     * @param size Size of data buffer
     * @param signature Magic signature to match
     * @return True if signature matches
     */
    virtual bool checkSignature(const uint8_t* data, size_t size,
                                const MagicSignature& signature) const;

    /**
     * @brief Analyze file header for additional metadata
     * @param data Data buffer
     * @param size Size of data buffer
     * @param format Detected format
     * @return Additional metadata
     */
    virtual std::unordered_map<std::string, std::string> analyzeHeader(
        const uint8_t* data, size_t size, ImageFormat format) const;

private:
    std::vector<MagicSignature> signatures_;
    std::unordered_map<std::string, ImageFormat> extensionMap_;
    std::unordered_map<ImageFormat, std::string> mimeTypeMap_;
    std::unordered_map<ImageFormat, std::vector<std::string>> extensionsMap_;
    std::unordered_map<ImageFormat, std::string> formatNameMap_;
};

/**
 * @brief Create optimized format detector instance
 * @return Unique pointer to format detector
 */
std::unique_ptr<FormatDetector> createFormatDetector();

/**
 * @brief Quick format detection from file
 * @param filePath Path to image file
 * @return Detected image format
 */
ImageFormat quickDetectFormat(const std::filesystem::path& filePath);

/**
 * @brief Quick format detection from memory
 * @param data Pointer to image data
 * @param size Size of data buffer
 * @return Detected image format
 */
ImageFormat quickDetectFormat(const void* data, size_t size);

}  // namespace atom::image

#endif  // ATOM_IMAGE_FORMAT_DETECTOR_HPP
