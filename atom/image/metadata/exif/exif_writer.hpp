#ifndef ATOM_IMAGE_METADATA_EXIF_WRITER_HPP
#define ATOM_IMAGE_METADATA_EXIF_WRITER_HPP

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../types/exif_types.hpp"
#include "../utils/byte_writer.hpp"

namespace atom::image::metadata {

/**
 * @brief EXIF data writer
 *
 * Writes EXIF metadata to image files or generates EXIF segments.
 */
class ExifWriter {
public:
    /**
     * @brief Default constructor
     */
    ExifWriter() = default;

    /**
     * @brief Construct with EXIF data
     */
    explicit ExifWriter(const ExifData& exifData);

    /**
     * @brief Destructor
     */
    virtual ~ExifWriter() = default;

    /**
     * @brief Set EXIF data to write
     */
    void setExifData(const ExifData& exifData) { exifData_ = exifData; }

    /**
     * @brief Get EXIF data
     */
    [[nodiscard]] const ExifData& getExifData() const noexcept {
        return exifData_;
    }

    /**
     * @brief Get mutable EXIF data
     */
    [[nodiscard]] ExifData& getExifData() noexcept { return exifData_; }

    /**
     * @brief Write EXIF data to file
     * @param inputFile Source image file
     * @param outputFile Output file (can be same as input)
     * @return True if successful
     */
    bool writeToFile(const std::filesystem::path& inputFile,
                     const std::filesystem::path& outputFile);

    /**
     * @brief Write EXIF data to file (in place)
     * @param file Image file to modify
     * @return True if successful
     */
    bool writeToFile(const std::filesystem::path& file);

    /**
     * @brief Embed EXIF data into existing image data
     * @param imageData Original image data
     * @return Modified image data with EXIF, or empty on error
     */
    [[nodiscard]] std::vector<uint8_t> embedInJpeg(
        const std::vector<uint8_t>& imageData);

    /**
     * @brief Generate EXIF APP1 segment
     * @return Raw EXIF segment data (without APP1 marker)
     */
    [[nodiscard]] std::vector<uint8_t> generateExifSegment();

    /**
     * @brief Generate complete APP1 segment with marker
     * @return Complete APP1 segment including marker and length
     */
    [[nodiscard]] std::vector<uint8_t> generateApp1Segment();

    /**
     * @brief Remove all EXIF data from JPEG
     * @param imageData Original JPEG data
     * @return JPEG data without EXIF
     */
    [[nodiscard]] static std::vector<uint8_t> stripExif(
        const std::vector<uint8_t>& imageData);

    /**
     * @brief Get last error message
     */
    [[nodiscard]] const std::string& lastError() const noexcept {
        return lastError_;
    }

    /**
     * @brief Set byte order for output
     */
    void setByteOrder(ByteOrder order) noexcept { byteOrder_ = order; }

    /**
     * @brief Get byte order
     */
    [[nodiscard]] ByteOrder getByteOrder() const noexcept { return byteOrder_; }

    /**
     * @brief Set whether to include thumbnail
     */
    void setIncludeThumbnail(bool include) noexcept {
        includeThumbnail_ = include;
    }

    /**
     * @brief Set whether to preserve unknown tags
     */
    void setPreserveUnknownTags(bool preserve) noexcept {
        preserveUnknownTags_ = preserve;
    }

    // Convenience methods for modifying specific fields

    /**
     * @brief Set camera make
     */
    void setCameraMake(const std::string& make) { exifData_.cameraMake = make; }

    /**
     * @brief Set camera model
     */
    void setCameraModel(const std::string& model) {
        exifData_.cameraModel = model;
    }

    /**
     * @brief Set software
     */
    void setSoftware(const std::string& software) {
        exifData_.software = software;
    }

    /**
     * @brief Set artist
     */
    void setArtist(const std::string& artist) { exifData_.artist = artist; }

    /**
     * @brief Set copyright
     */
    void setCopyright(const std::string& copyright) {
        exifData_.copyright = copyright;
    }

    /**
     * @brief Set image description
     */
    void setImageDescription(const std::string& description) {
        exifData_.imageDescription = description;
    }

    /**
     * @brief Set user comment
     */
    void setUserComment(const std::string& comment) {
        exifData_.userComment = comment;
    }

    /**
     * @brief Set date/time original
     */
    void setDateTimeOriginal(
        const std::chrono::system_clock::time_point& dateTime) {
        exifData_.dateTimeOriginal = dateTime;
    }

    /**
     * @brief Set GPS coordinates
     */
    void setGpsCoordinates(double latitude, double longitude,
                           std::optional<double> altitude = std::nullopt);

    /**
     * @brief Remove GPS data
     */
    void removeGpsData();

    /**
     * @brief Set orientation
     */
    void setOrientation(ExifOrientation orientation) {
        exifData_.imageProperties.orientation = orientation;
    }

    /**
     * @brief Set a custom tag
     */
    void setCustomTag(const std::string& key, const std::string& value) {
        exifData_.setCustomTag(key, value);
    }

protected:
    /**
     * @brief Build IFD0 entries
     */
    void buildIfd0Entries(std::vector<IfdEntry>& entries);

    /**
     * @brief Build EXIF IFD entries
     */
    void buildExifIfdEntries(std::vector<IfdEntry>& entries);

    /**
     * @brief Build GPS IFD entries
     */
    void buildGpsIfdEntries(std::vector<IfdEntry>& entries);

    /**
     * @brief Write IFD to buffer
     * @param writer ByteWriter to write to
     * @param entries IFD entries
     * @param dataAreaOffset Offset where extra data can be written
     * @return Offset after IFD (where next IFD offset is written)
     */
    size_t writeIfd(ByteWriter& writer, const std::vector<IfdEntry>& entries,
                    size_t dataAreaOffset);

    /**
     * @brief Calculate IFD size including data area
     */
    [[nodiscard]] size_t calculateIfdSize(
        const std::vector<IfdEntry>& entries) const;

    /**
     * @brief Write entry value to data area
     */
    void writeEntryValue(ByteWriter& writer, const IfdEntry& entry);

    /**
     * @brief Create string entry
     */
    [[nodiscard]] IfdEntry createStringEntry(uint16_t tag,
                                             const std::string& value);

    /**
     * @brief Create rational entry
     */
    [[nodiscard]] IfdEntry createRationalEntry(uint16_t tag, double value,
                                               uint32_t precision = 10000);

    /**
     * @brief Create rational array entry
     */
    [[nodiscard]] IfdEntry createRationalArrayEntry(
        uint16_t tag, const std::vector<double>& values,
        uint32_t precision = 10000);

    /**
     * @brief Create short entry
     */
    [[nodiscard]] IfdEntry createShortEntry(uint16_t tag, uint16_t value);

    /**
     * @brief Create long entry
     */
    [[nodiscard]] IfdEntry createLongEntry(uint16_t tag, uint32_t value);

private:
    ExifData exifData_;
    std::string lastError_;
    ByteOrder byteOrder_ = ByteOrder::LITTLE_ENDIAN;
    bool includeThumbnail_ = true;
    bool preserveUnknownTags_ = false;
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_EXIF_WRITER_HPP
