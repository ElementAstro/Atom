#ifndef ATOM_IMAGE_EXIF_HPP
#define ATOM_IMAGE_EXIF_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace atom::image {

/**
 * @class ExifException
 * @brief Exception class for EXIF parsing errors
 */
class ExifException : public std::runtime_error {
public:
    explicit ExifException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @struct GpsCoordinate
 * @brief Structure to represent GPS coordinate data
 */
struct GpsCoordinate {
    double degrees;
    double minutes;
    double seconds;
    char direction;

    /**
     * @brief Convert to decimal degrees
     * @return Decimal degree representation
     */
    [[nodiscard]] double toDecimalDegrees() const noexcept {
        double value = degrees + minutes / 60.0 + seconds / 3600.0;
        return (direction == 'S' || direction == 'W') ? -value : value;
    }

    /**
     * @brief Create from decimal degrees
     * @param decimal Decimal degree value
     * @param isLatitude True if latitude, false if longitude
     * @return GpsCoordinate instance
     */
    [[nodiscard]] static GpsCoordinate fromDecimalDegrees(
        double decimal, bool isLatitude) noexcept;

    /**
     * @brief Convert to string representation
     * @return String representation of GPS coordinate
     */
    [[nodiscard]] std::string toString() const;
};

/**
 * @struct ExifData
 * @brief Structure to hold EXIF data for an image.
 */
struct alignas(128) ExifData {
    std::string cameraMake;
    std::string cameraModel;
    std::string dateTime;
    std::string exposureTime;
    std::string fNumber;
    std::string isoSpeed;
    std::string focalLength;
    std::optional<GpsCoordinate> gpsLatitude;
    std::optional<GpsCoordinate> gpsLongitude;
    std::string orientation;
    std::string compression;
    std::string imageWidth;
    std::string imageHeight;
    std::string colorSpace;
    std::string software;
};

/**
 * @class ExifParser
 * @brief Class to handle the parsing of EXIF data.
 */
class ExifParser {
public:
    /**
     * @brief Constructs an ExifParser with the specified filename.
     * @param filename The name of the file to parse.
     */
    explicit ExifParser(const std::string& filename);

    /**
     * @brief Parses the EXIF data from the file.
     * @return True if parsing was successful, false otherwise.
     */
    auto parse() -> bool;

    /**
     * @brief Gets the parsed EXIF data.
     * @return A constant reference to the ExifData structure.
     */
    [[nodiscard]] auto getExifData() const -> const ExifData&;

    virtual ~ExifParser() = default;

    /**
     * @brief Optimize memory usage of EXIF data
     */
    virtual void optimize();

    /**
     * @brief Validate data integrity
     * @return True if data is valid, false otherwise
     */
    [[nodiscard]] bool validateData() const;

    /**
     * @brief Clone the parser instance
     * @return Unique pointer to cloned parser
     */
    [[nodiscard]] virtual std::unique_ptr<ExifParser> clone() const;

    /**
     * @brief Serialize EXIF data to string
     * @return Serialized string representation
     */
    [[nodiscard]] virtual std::string serialize() const;

    /**
     * @brief Deserialize EXIF data from string
     * @param data Serialized data string
     * @return Unique pointer to ExifParser instance
     */
    static std::unique_ptr<ExifParser> deserialize(const std::string& data);

protected:
    /**
     * @brief Parses the Image File Directory (IFD) from the EXIF data.
     * @param data Pointer to the EXIF data.
     * @param isLittleEndian Boolean indicating if the data is in little-endian
     * format.
     * @param tiffStart Pointer to the start of the TIFF header.
     * @param bufferSize The size of the EXIF data buffer.
     * @return True if parsing was successful, false otherwise.
     */
    auto parseIFD(const std::byte* data, bool isLittleEndian,
                  const std::byte* tiffStart, size_t bufferSize) -> bool;

    /**
     * @brief Parses a GPS coordinate from the EXIF data.
     * @param data Pointer to the EXIF data.
     * @param isLittleEndian Boolean indicating if the data is in little-endian
     * format.
     * @return The parsed GPS coordinate as a string.
     */
    auto parseGPSCoordinate(const std::byte* data, bool isLittleEndian)
        -> std::string;

    /**
     * @brief Parses a rational number from the EXIF data.
     * @param data Pointer to the EXIF data.
     * @param isLittleEndian Boolean indicating if the data is in little-endian
     * format.
     * @return The parsed rational number as a double.
     */
    auto parseRational(const std::byte* data, bool isLittleEndian) -> double;

    /**
     * @brief Reads a 16-bit unsigned integer from big-endian data.
     * @param data Pointer to the data.
     * @return The 16-bit unsigned integer.
     */
    auto readUint16Be(const std::byte* data) -> uint16_t;

    /**
     * @brief Reads a 32-bit unsigned integer from big-endian data.
     * @param data Pointer to the data.
     * @return The 32-bit unsigned integer.
     */
    auto readUint32Be(const std::byte* data) -> uint32_t;

    /**
     * @brief Reads a 16-bit unsigned integer from little-endian data.
     * @param data Pointer to the data.
     * @return The 16-bit unsigned integer.
     */
    auto readUint16Le(const std::byte* data) -> uint16_t;

    /**
     * @brief Reads a 32-bit unsigned integer from little-endian data.
     * @param data Pointer to the data.
     * @return The 32-bit unsigned integer.
     */
    auto readUint32Le(const std::byte* data) -> uint32_t;

    /**
     * @brief Validate buffer bounds
     * @param ptr Pointer to validate
     * @param size Size to validate
     * @return True if bounds are valid, false otherwise
     */
    virtual bool validateBufferBounds(const std::byte* ptr, size_t size) const;

    /**
     * @brief Clear EXIF data
     */
    virtual void clearExifData();

private:
    std::string m_filename;
    ExifData m_exifData;

    auto parseColorSpace(const std::byte* data, bool isLittleEndian)
        -> std::string;
    auto parseOrientation(const std::byte* data, bool isLittleEndian)
        -> std::string;
};

}  // namespace atom::image

#endif  // ATOM_IMAGE_EXIF_HPP
