#ifndef ATOM_IMAGE_METADATA_EXIF_READER_HPP
#define ATOM_IMAGE_METADATA_EXIF_READER_HPP

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../types/exif_types.hpp"
#include "../types/gps_types.hpp"
#include "ifd_parser.hpp"

namespace atom::image::metadata {

/**
 * @brief EXIF data reader
 *
 * Reads and parses EXIF metadata from image files or memory buffers.
 * Supports JPEG, TIFF, and other EXIF-containing formats.
 */
class ExifReader {
public:
    /**
     * @brief Default constructor
     */
    ExifReader() = default;

    /**
     * @brief Construct with file path
     * @param filename Path to image file
     */
    explicit ExifReader(std::string_view filename);

    /**
     * @brief Construct with file path
     * @param path Path to image file
     */
    explicit ExifReader(const std::filesystem::path& path);

    /**
     * @brief Destructor
     */
    virtual ~ExifReader() = default;

    // Non-copyable
    ExifReader(const ExifReader&) = delete;
    ExifReader& operator=(const ExifReader&) = delete;

    // Movable
    ExifReader(ExifReader&&) = default;
    ExifReader& operator=(ExifReader&&) = default;

    /**
     * @brief Read EXIF data from file
     * @param filename Path to image file
     * @return Parsed EXIF data or nullopt on error
     */
    [[nodiscard]] static std::optional<ExifData> fromFile(
        const std::filesystem::path& filename);

    /**
     * @brief Read EXIF data from file (string path)
     * @param filename Path to image file
     * @return Parsed EXIF data or nullopt on error
     */
    [[nodiscard]] static std::optional<ExifData> fromFile(
        std::string_view filename);

    /**
     * @brief Read EXIF data from memory buffer
     * @param data Pointer to image data
     * @param size Size of data buffer
     * @return Parsed EXIF data or nullopt on error
     */
    [[nodiscard]] static std::optional<ExifData> fromMemory(const void* data,
                                                            size_t size);

    /**
     * @brief Read EXIF data from byte vector
     * @param data Image data
     * @return Parsed EXIF data or nullopt on error
     */
    [[nodiscard]] static std::optional<ExifData> fromMemory(
        const std::vector<uint8_t>& data);

    /**
     * @brief Parse EXIF data (instance method)
     * @return True if parsing succeeded
     */
    bool parse();

    /**
     * @brief Parse from memory buffer
     * @param data Pointer to image data
     * @param size Size of data buffer
     * @return True if parsing succeeded
     */
    bool parseFromMemory(const void* data, size_t size);

    /**
     * @brief Get parsed EXIF data
     * @return Const reference to EXIF data
     */
    [[nodiscard]] const ExifData& getExifData() const noexcept {
        return exifData_;
    }

    /**
     * @brief Get mutable EXIF data
     * @return Reference to EXIF data
     */
    [[nodiscard]] ExifData& getExifData() noexcept { return exifData_; }

    /**
     * @brief Check if EXIF data was found
     */
    [[nodiscard]] bool hasExifData() const noexcept {
        return exifData_.hasData();
    }

    /**
     * @brief Check if GPS data is present
     */
    [[nodiscard]] bool hasGpsData() const noexcept {
        return exifData_.gpsData && exifData_.gpsData->hasPosition();
    }

    /**
     * @brief Get last error message
     */
    [[nodiscard]] const std::string& lastError() const noexcept {
        return lastError_;
    }

    /**
     * @brief Get file path
     */
    [[nodiscard]] const std::string& filename() const noexcept {
        return filename_;
    }

    /**
     * @brief Extract thumbnail image data
     * @return Thumbnail JPEG data or empty vector if not present
     */
    [[nodiscard]] std::vector<uint8_t> extractThumbnail() const;

    /**
     * @brief Check if thumbnail is present
     */
    [[nodiscard]] bool hasThumbnail() const noexcept {
        return exifData_.thumbnailData.has_value() &&
               !exifData_.thumbnailData->empty();
    }

    /**
     * @brief Validate parsed data integrity
     * @return True if data appears valid
     */
    [[nodiscard]] bool validateData() const;

    /**
     * @brief Optimize memory usage
     */
    void optimize();

    /**
     * @brief Clear all data
     */
    void clear();

    /**
     * @brief Clone the reader
     * @return Unique pointer to cloned reader
     */
    [[nodiscard]] std::unique_ptr<ExifReader> clone() const;

    /**
     * @brief Serialize EXIF data to JSON string
     * @return JSON representation
     */
    [[nodiscard]] std::string toJson() const;

    /**
     * @brief Serialize EXIF data to string (simple format)
     * @return String representation
     */
    [[nodiscard]] std::string serialize() const;

    /**
     * @brief Deserialize EXIF data from string
     * @param data Serialized data
     * @return Unique pointer to reader with parsed data
     */
    [[nodiscard]] static std::unique_ptr<ExifReader> deserialize(
        const std::string& data);

    /**
     * @brief Set load mode
     * @param mode Load mode (FULL, LAZY, MINIMAL)
     */
    void setLoadMode(ExifLoadMode mode) noexcept { loadMode_ = mode; }

    /**
     * @brief Get load mode
     */
    [[nodiscard]] ExifLoadMode getLoadMode() const noexcept {
        return loadMode_;
    }

protected:
    /**
     * @brief Load file into memory
     * @return True if file was loaded successfully
     */
    bool loadFile();

    /**
     * @brief Parse EXIF data from loaded buffer
     * @return True if parsing succeeded
     */
    bool parseBuffer();

    /**
     * @brief Parse JPEG EXIF data
     * @return True if parsing succeeded
     */
    bool parseJpegExif();

    /**
     * @brief Parse TIFF EXIF data
     * @return True if parsing succeeded
     */
    bool parseTiffExif();

    /**
     * @brief Extract EXIF data from IFD parse results
     */
    void extractExifData(const std::vector<IfdParseResult>& ifdChain,
                         IfdParser& parser);

    /**
     * @brief Extract camera information from IFD
     */
    void extractCameraInfo(const IfdParseResult& ifd);

    /**
     * @brief Extract camera settings from EXIF IFD
     */
    void extractCameraSettings(const IfdParseResult& ifd);

    /**
     * @brief Extract image properties from IFD
     */
    void extractImageProperties(const IfdParseResult& ifd);

    /**
     * @brief Extract lens information from EXIF IFD
     */
    void extractLensInfo(const IfdParseResult& ifd);

    /**
     * @brief Extract timestamps from IFD
     */
    void extractTimestamps(const IfdParseResult& ifd);

    /**
     * @brief Extract thumbnail data
     */
    void extractThumbnail(const IfdParseResult& ifd1, const ByteReader& reader);

private:
    std::string filename_;
    std::vector<std::byte> fileData_;
    ExifData exifData_;
    std::string lastError_;
    ExifLoadMode loadMode_ = ExifLoadMode::FULL;
};

// Backward compatibility alias
using ExifParser = ExifReader;

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_EXIF_READER_HPP
