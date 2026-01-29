#ifndef ATOM_IMAGE_METADATA_VALUE_HPP
#define ATOM_IMAGE_METADATA_VALUE_HPP

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace atom::image::metadata {

/**
 * @brief Generic metadata value type supporting all common metadata types
 */
using MetadataValue =
    std::variant<std::monostate,                        // No value / null
                 bool,                                  // Boolean
                 int32_t,                               // Integer
                 int64_t,                               // Long integer
                 double,                                // Floating point
                 std::string,                           // String
                 std::vector<uint8_t>,                  // Binary data
                 std::vector<std::string>,              // String array
                 std::vector<int32_t>,                  // Integer array
                 std::vector<double>,                   // Double array
                 std::chrono::system_clock::time_point  // Timestamp
                 >;

/**
 * @brief Metadata value type enumeration
 */
enum class MetadataValueType {
    NONE,
    BOOLEAN,
    INTEGER,
    LONG,
    DOUBLE,
    STRING,
    BINARY,
    STRING_ARRAY,
    INTEGER_ARRAY,
    DOUBLE_ARRAY,
    TIMESTAMP
};

/**
 * @brief Get the type of a metadata value
 */
inline MetadataValueType getMetadataValueType(const MetadataValue& value) {
    return static_cast<MetadataValueType>(value.index());
}

/**
 * @brief Metadata entry with key, value, and optional namespace
 */
struct MetadataEntry {
    std::string key;
    MetadataValue value;
    std::optional<std::string> namespaceUri;
    std::optional<std::string> description;

    MetadataEntry() = default;

    MetadataEntry(std::string k, MetadataValue v)
        : key(std::move(k)), value(std::move(v)) {}

    MetadataEntry(std::string k, MetadataValue v, std::string ns)
        : key(std::move(k)), value(std::move(v)), namespaceUri(std::move(ns)) {}

    /**
     * @brief Check if entry has a value
     */
    [[nodiscard]] bool hasValue() const noexcept {
        return !std::holds_alternative<std::monostate>(value);
    }

    /**
     * @brief Get value as string (with conversion)
     */
    [[nodiscard]] std::optional<std::string> asString() const;

    /**
     * @brief Get value as integer (with conversion)
     */
    [[nodiscard]] std::optional<int32_t> asInt() const;

    /**
     * @brief Get value as double (with conversion)
     */
    [[nodiscard]] std::optional<double> asDouble() const;

    /**
     * @brief Get value as boolean (with conversion)
     */
    [[nodiscard]] std::optional<bool> asBool() const;

    /**
     * @brief Get value type
     */
    [[nodiscard]] MetadataValueType getType() const noexcept {
        return getMetadataValueType(value);
    }
};

/**
 * @brief Unified metadata container that can hold EXIF, IPTC, XMP data
 */
class UnifiedMetadata {
public:
    UnifiedMetadata() = default;

    /**
     * @brief Set a metadata value
     */
    void set(const std::string& key, const MetadataValue& value,
             const std::optional<std::string>& ns = std::nullopt);

    /**
     * @brief Get a metadata value
     */
    [[nodiscard]] std::optional<MetadataValue> get(
        const std::string& key,
        const std::optional<std::string>& ns = std::nullopt) const;

    /**
     * @brief Get a metadata value as string
     */
    [[nodiscard]] std::optional<std::string> getString(
        const std::string& key,
        const std::optional<std::string>& ns = std::nullopt) const;

    /**
     * @brief Get a metadata value as integer
     */
    [[nodiscard]] std::optional<int32_t> getInt(
        const std::string& key,
        const std::optional<std::string>& ns = std::nullopt) const;

    /**
     * @brief Get a metadata value as double
     */
    [[nodiscard]] std::optional<double> getDouble(
        const std::string& key,
        const std::optional<std::string>& ns = std::nullopt) const;

    /**
     * @brief Check if a key exists
     */
    [[nodiscard]] bool has(
        const std::string& key,
        const std::optional<std::string>& ns = std::nullopt) const;

    /**
     * @brief Remove a metadata entry
     */
    bool remove(const std::string& key,
                const std::optional<std::string>& ns = std::nullopt);

    /**
     * @brief Get all entries
     */
    [[nodiscard]] const std::vector<MetadataEntry>& entries() const noexcept {
        return entries_;
    }

    /**
     * @brief Get all keys
     */
    [[nodiscard]] std::vector<std::string> keys() const;

    /**
     * @brief Get keys in a specific namespace
     */
    [[nodiscard]] std::vector<std::string> keysInNamespace(
        const std::string& ns) const;

    /**
     * @brief Get entry count
     */
    [[nodiscard]] size_t size() const noexcept { return entries_.size(); }

    /**
     * @brief Check if empty
     */
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

    /**
     * @brief Clear all entries
     */
    void clear() noexcept { entries_.clear(); }

    /**
     * @brief Merge with another metadata container
     */
    void merge(const UnifiedMetadata& other, bool overwrite = false);

private:
    std::vector<MetadataEntry> entries_;

    /**
     * @brief Find entry by key and optional namespace
     */
    [[nodiscard]] std::vector<MetadataEntry>::iterator findEntry(
        const std::string& key, const std::optional<std::string>& ns);

    [[nodiscard]] std::vector<MetadataEntry>::const_iterator findEntry(
        const std::string& key, const std::optional<std::string>& ns) const;
};

/**
 * @brief Standard metadata keys used across different formats
 */
namespace MetadataKeys {
// Basic image information
constexpr const char* WIDTH = "ImageWidth";
constexpr const char* HEIGHT = "ImageHeight";
constexpr const char* BIT_DEPTH = "BitsPerSample";
constexpr const char* COLOR_SPACE = "ColorSpace";
constexpr const char* ORIENTATION = "Orientation";

// Camera information
constexpr const char* CAMERA_MAKE = "Make";
constexpr const char* CAMERA_MODEL = "Model";
constexpr const char* LENS_MAKE = "LensMake";
constexpr const char* LENS_MODEL = "LensModel";

// Shooting parameters
constexpr const char* EXPOSURE_TIME = "ExposureTime";
constexpr const char* F_NUMBER = "FNumber";
constexpr const char* ISO = "ISOSpeedRatings";
constexpr const char* FOCAL_LENGTH = "FocalLength";
constexpr const char* FOCAL_LENGTH_35MM = "FocalLengthIn35mmFilm";
constexpr const char* FLASH = "Flash";
constexpr const char* METERING_MODE = "MeteringMode";
constexpr const char* EXPOSURE_PROGRAM = "ExposureProgram";
constexpr const char* WHITE_BALANCE = "WhiteBalance";

// Date/Time
constexpr const char* DATE_TIME = "DateTime";
constexpr const char* DATE_TIME_ORIGINAL = "DateTimeOriginal";
constexpr const char* DATE_TIME_DIGITIZED = "DateTimeDigitized";

// GPS
constexpr const char* GPS_LATITUDE = "GPSLatitude";
constexpr const char* GPS_LONGITUDE = "GPSLongitude";
constexpr const char* GPS_ALTITUDE = "GPSAltitude";

// Creator/Copyright
constexpr const char* ARTIST = "Artist";
constexpr const char* COPYRIGHT = "Copyright";
constexpr const char* SOFTWARE = "Software";

// Description
constexpr const char* TITLE = "Title";
constexpr const char* DESCRIPTION = "ImageDescription";
constexpr const char* COMMENT = "UserComment";
constexpr const char* KEYWORDS = "Keywords";
constexpr const char* RATING = "Rating";

// Technical
constexpr const char* COMPRESSION = "Compression";
constexpr const char* X_RESOLUTION = "XResolution";
constexpr const char* Y_RESOLUTION = "YResolution";
constexpr const char* RESOLUTION_UNIT = "ResolutionUnit";
}  // namespace MetadataKeys

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_VALUE_HPP
