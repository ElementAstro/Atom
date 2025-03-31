#ifndef ATOM_IMAGE_EXIF_HPP
#define ATOM_IMAGE_EXIF_HPP

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <memory>

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
    char direction;  // 'N', 'S', 'E', or 'W'

    // Convert to decimal degrees
    [[nodiscard]] double toDecimalDegrees() const noexcept {
        double value = degrees + minutes / 60.0 + seconds / 3600.0;
        return (direction == 'S' || direction == 'W') ? -value : value;
    }

    // Create from decimal degrees
    [[nodiscard]] static GpsCoordinate fromDecimalDegrees(
        double decimal, bool isLatitude) noexcept;

    // Convert to string representation
    [[nodiscard]] std::string toString() const;
};

/**
 * @struct ExifData
 * @brief Structure to hold EXIF data for an image.
 */
struct alignas(128) ExifData {
    std::string cameraMake;    ///< The make of the camera.
    std::string cameraModel;   ///< The model of the camera.
    std::string dateTime;      ///< The date and time when the photo was taken.
    std::string exposureTime;  ///< The exposure time of the photo.
    std::string fNumber;       ///< The f-number (aperture) of the photo.
    std::string isoSpeed;      ///< The ISO speed of the photo.
    std::string focalLength;   ///< The focal length of the lens.
    std::optional<GpsCoordinate>
        gpsLatitude;  ///< The GPS latitude where the photo was taken.
    std::optional<GpsCoordinate>
        gpsLongitude;  ///< The GPS longitude where the photo was taken.

    // Additional fields
    std::string orientation;    ///< Image orientation
    std::string compression;    ///< Compression method
    std::string imageWidth;     ///< Image width
    std::string imageHeight;    ///< Image height
    std::string colorSpace;     ///< Color space
    std::string software;       ///< Processing software
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

    // Virtual destructor
    virtual ~ExifParser() = default;

    // Optimization method
    virtual void optimize();

    // Validate data integrity
    [[nodiscard]] bool validateData() const;

    // Clone interface
    [[nodiscard]] virtual std::unique_ptr<ExifParser> clone() const;

    // Serialization interface
    [[nodiscard]] virtual std::string serialize() const;
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
    auto parseGPSCoordinate(const std::byte* data,
                            bool isLittleEndian) -> std::string;

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

    // Validate buffer bounds
    virtual bool validateBufferBounds(const std::byte* ptr, size_t size) const;

    // Clear EXIF data
    virtual void clearExifData();

private:
    std::string m_filename;  ///< The name of the file to parse.
    ExifData m_exifData;     ///< The structure to hold the parsed EXIF data.

    // Private helper methods
    auto parseColorSpace(const std::byte* data, bool isLittleEndian) -> std::string;
    auto parseOrientation(const std::byte* data, bool isLittleEndian) -> std::string;
};

}  // namespace atom::image

#endif  // ATOM_IMAGE_EXIF_HPP
