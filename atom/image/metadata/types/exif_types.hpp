#ifndef ATOM_IMAGE_METADATA_EXIF_TYPES_HPP
#define ATOM_IMAGE_METADATA_EXIF_TYPES_HPP

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace atom::image::metadata {

/**
 * @brief Exception class for EXIF parsing errors
 */
class ExifException : public std::runtime_error {
public:
    explicit ExifException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Exception thrown when EXIF data is not found
 */
class ExifNotFoundException : public ExifException {
public:
    explicit ExifNotFoundException(const std::string& message)
        : ExifException("EXIF not found: " + message) {}
};

/**
 * @brief Exception thrown when EXIF data is corrupted
 */
class ExifCorruptedException : public ExifException {
public:
    explicit ExifCorruptedException(const std::string& message)
        : ExifException("EXIF corrupted: " + message) {}
};

/**
 * @brief EXIF data types as defined in EXIF specification
 */
enum class ExifDataType : uint16_t {
    BYTE = 1,        ///< 8-bit unsigned integer
    ASCII = 2,       ///< 8-bit byte containing ASCII code
    SHORT = 3,       ///< 16-bit unsigned integer
    LONG = 4,        ///< 32-bit unsigned integer
    RATIONAL = 5,    ///< Two LONGs: numerator and denominator
    SBYTE = 6,       ///< 8-bit signed integer
    UNDEFINED = 7,   ///< 8-bit byte that may take any value
    SSHORT = 8,      ///< 16-bit signed integer
    SLONG = 9,       ///< 32-bit signed integer
    SRATIONAL = 10,  ///< Two SLONGs: numerator and denominator
    FLOAT = 11,      ///< Single precision (4-byte) IEEE format
    DOUBLE = 12      ///< Double precision (8-byte) IEEE format
};

/**
 * @brief Get the size in bytes for each EXIF data type
 */
constexpr size_t getExifDataTypeSize(ExifDataType type) noexcept {
    switch (type) {
        case ExifDataType::BYTE:
        case ExifDataType::ASCII:
        case ExifDataType::SBYTE:
        case ExifDataType::UNDEFINED:
            return 1;
        case ExifDataType::SHORT:
        case ExifDataType::SSHORT:
            return 2;
        case ExifDataType::LONG:
        case ExifDataType::SLONG:
        case ExifDataType::FLOAT:
            return 4;
        case ExifDataType::RATIONAL:
        case ExifDataType::SRATIONAL:
        case ExifDataType::DOUBLE:
            return 8;
        default:
            return 0;
    }
}

/**
 * @brief Image orientation values (EXIF tag 0x0112)
 */
enum class ExifOrientation : int {
    NORMAL = 1,           ///< Normal orientation
    FLIP_HORIZONTAL = 2,  ///< Flipped horizontally
    ROTATE_180 = 3,       ///< Rotated 180 degrees
    FLIP_VERTICAL = 4,    ///< Flipped vertically
    TRANSPOSE = 5,        ///< Transposed (rotated 90 CCW and flipped)
    ROTATE_90_CW = 6,     ///< Rotated 90 degrees clockwise
    TRANSVERSE = 7,       ///< Transverse (rotated 90 CW and flipped)
    ROTATE_270_CW = 8     ///< Rotated 270 degrees clockwise (90 CCW)
};

/**
 * @brief Color space values (EXIF tag 0xA001)
 */
enum class ColorSpace : uint16_t {
    SRGB = 1,              ///< sRGB color space
    ADOBE_RGB = 2,         ///< Adobe RGB color space
    WIDE_GAMUT = 0xFFFD,   ///< Wide gamut RGB
    ICC_PROFILE = 0xFFFE,  ///< ICC Profile
    UNCALIBRATED = 0xFFFF  ///< Uncalibrated
};

/**
 * @brief Compression type values (EXIF tag 0x0103)
 */
enum class CompressionType : uint16_t {
    UNCOMPRESSED = 1,
    CCITT_1D = 2,
    T4_GROUP3_FAX = 3,
    T6_GROUP4_FAX = 4,
    LZW = 5,
    JPEG_OLD = 6,
    JPEG = 7,
    ADOBE_DEFLATE = 8,
    JBIG_BW = 9,
    JBIG_COLOR = 10,
    JPEG_2000 = 34712,
    PACKBITS = 32773
};

/**
 * @brief Metering mode values (EXIF tag 0x9207)
 */
enum class MeteringMode : uint16_t {
    UNKNOWN = 0,
    AVERAGE = 1,
    CENTER_WEIGHTED_AVERAGE = 2,
    SPOT = 3,
    MULTI_SPOT = 4,
    PATTERN = 5,
    PARTIAL = 6,
    OTHER = 255
};

/**
 * @brief Exposure program values (EXIF tag 0x8822)
 */
enum class ExposureProgram : uint16_t {
    NOT_DEFINED = 0,
    MANUAL = 1,
    NORMAL_PROGRAM = 2,
    APERTURE_PRIORITY = 3,
    SHUTTER_PRIORITY = 4,
    CREATIVE_PROGRAM = 5,
    ACTION_PROGRAM = 6,
    PORTRAIT_MODE = 7,
    LANDSCAPE_MODE = 8
};

/**
 * @brief Flash mode values (EXIF tag 0x9209)
 */
enum class FlashMode : uint16_t {
    NO_FLASH = 0x0000,
    FIRED = 0x0001,
    FIRED_RETURN_NOT_DETECTED = 0x0005,
    FIRED_RETURN_DETECTED = 0x0007,
    ON_DID_NOT_FIRE = 0x0008,
    ON = 0x0009,
    OFF = 0x0010,
    AUTO_DID_NOT_FIRE = 0x0018,
    AUTO_FIRED = 0x0019,
    NO_FLASH_FUNCTION = 0x0020,
    FIRED_RED_EYE_REDUCTION = 0x0041,
    FIRED_RED_EYE_RETURN_NOT_DETECTED = 0x0045,
    FIRED_RED_EYE_RETURN_DETECTED = 0x0047,
    ON_RED_EYE_REDUCTION = 0x0049,
    AUTO_FIRED_RED_EYE_REDUCTION = 0x0059
};

/**
 * @brief White balance values (EXIF tag 0xA403)
 */
enum class WhiteBalance : uint16_t { AUTO = 0, MANUAL = 1 };

/**
 * @brief Light source values (EXIF tag 0x9208)
 */
enum class LightSource : uint16_t {
    UNKNOWN = 0,
    DAYLIGHT = 1,
    FLUORESCENT = 2,
    TUNGSTEN = 3,
    FLASH = 4,
    FINE_WEATHER = 9,
    CLOUDY_WEATHER = 10,
    SHADE = 11,
    DAYLIGHT_FLUORESCENT = 12,
    DAY_WHITE_FLUORESCENT = 13,
    COOL_WHITE_FLUORESCENT = 14,
    WHITE_FLUORESCENT = 15,
    WARM_WHITE_FLUORESCENT = 16,
    STANDARD_LIGHT_A = 17,
    STANDARD_LIGHT_B = 18,
    STANDARD_LIGHT_C = 19,
    D55 = 20,
    D65 = 21,
    D75 = 22,
    D50 = 23,
    ISO_STUDIO_TUNGSTEN = 24,
    OTHER = 255
};

/**
 * @brief Rational number representation
 */
struct Rational {
    uint32_t numerator = 0;
    uint32_t denominator = 1;

    [[nodiscard]] double toDouble() const noexcept {
        return denominator == 0 ? 0.0
                                : static_cast<double>(numerator) / denominator;
    }

    static Rational fromDouble(double value, uint32_t precision = 10000) {
        Rational r;
        r.numerator = static_cast<uint32_t>(value * precision);
        r.denominator = precision;
        return r;
    }

    bool operator==(const Rational& other) const noexcept {
        return numerator == other.numerator && denominator == other.denominator;
    }
};

/**
 * @brief Signed rational number representation
 */
struct SRational {
    int32_t numerator = 0;
    int32_t denominator = 1;

    [[nodiscard]] double toDouble() const noexcept {
        return denominator == 0 ? 0.0
                                : static_cast<double>(numerator) / denominator;
    }

    static SRational fromDouble(double value, int32_t precision = 10000) {
        SRational r;
        r.numerator = static_cast<int32_t>(value * precision);
        r.denominator = precision;
        return r;
    }

    bool operator==(const SRational& other) const noexcept {
        return numerator == other.numerator && denominator == other.denominator;
    }
};

/**
 * @brief Generic EXIF value type
 */
using ExifValue = std::variant<std::monostate,         // No value
                               uint8_t,                // BYTE
                               std::string,            // ASCII
                               uint16_t,               // SHORT
                               uint32_t,               // LONG
                               Rational,               // RATIONAL
                               int8_t,                 // SBYTE
                               std::vector<uint8_t>,   // UNDEFINED
                               int16_t,                // SSHORT
                               int32_t,                // SLONG
                               SRational,              // SRATIONAL
                               float,                  // FLOAT
                               double,                 // DOUBLE
                               std::vector<uint16_t>,  // Multiple SHORTs
                               std::vector<uint32_t>,  // Multiple LONGs
                               std::vector<Rational>,  // Multiple RATIONALs
                               std::vector<SRational>  // Multiple SRATIONALs
                               >;

/**
 * @brief IFD entry structure
 */
struct IfdEntry {
    uint16_t tag = 0;
    ExifDataType type = ExifDataType::UNDEFINED;
    uint32_t count = 0;
    ExifValue value;
    uint32_t valueOffset = 0;  ///< Original offset in file (for writing)

    [[nodiscard]] bool hasValue() const noexcept {
        return !std::holds_alternative<std::monostate>(value);
    }
};

/**
 * @brief Lens information structure
 */
struct LensInfo {
    std::optional<std::string> make;
    std::optional<std::string> model;
    std::optional<std::string> serialNumber;
    std::optional<double> minFocalLength;
    std::optional<double> maxFocalLength;
    std::optional<double> minFNumberAtMinFocal;
    std::optional<double> minFNumberAtMaxFocal;
};

/**
 * @brief Camera settings structure
 */
struct CameraSettings {
    std::optional<double> exposureTime;     ///< Exposure time in seconds
    std::optional<double> fNumber;          ///< F-number (aperture)
    std::optional<int> isoSpeed;            ///< ISO speed rating
    std::optional<double> focalLength;      ///< Focal length in mm
    std::optional<double> focalLength35mm;  ///< 35mm equivalent focal length
    std::optional<ExposureProgram> exposureProgram;
    std::optional<MeteringMode> meteringMode;
    std::optional<FlashMode> flashMode;
    std::optional<WhiteBalance> whiteBalance;
    std::optional<LightSource> lightSource;
    std::optional<double> exposureBias;  ///< Exposure bias in EV
    std::optional<double> maxAperture;   ///< Max aperture value
    std::optional<double> digitalZoomRatio;
    std::optional<int> contrast;    ///< -2 to +2
    std::optional<int> saturation;  ///< -2 to +2
    std::optional<int> sharpness;   ///< -2 to +2
};

/**
 * @brief Image dimensions and properties
 */
struct ImageProperties {
    std::optional<int> width;
    std::optional<int> height;
    std::optional<int> bitsPerSample;
    std::optional<int> samplesPerPixel;
    std::optional<ColorSpace> colorSpace;
    std::optional<CompressionType> compression;
    std::optional<ExifOrientation> orientation;
    std::optional<double> xResolution;
    std::optional<double> yResolution;
    std::optional<std::string> resolutionUnit;
};

// Forward declaration for GpsCoordinate (defined in gps_types.hpp)
struct GpsCoordinate;
struct GpsData;

/**
 * @brief Complete EXIF data structure
 */
struct alignas(64) ExifData {
    // Camera information
    std::optional<std::string> cameraMake;
    std::optional<std::string> cameraModel;
    std::optional<std::string> software;
    std::optional<std::string> artist;
    std::optional<std::string> copyright;

    // Timestamps
    std::optional<std::chrono::system_clock::time_point> dateTime;
    std::optional<std::chrono::system_clock::time_point> dateTimeOriginal;
    std::optional<std::chrono::system_clock::time_point> dateTimeDigitized;
    std::optional<std::string> subSecTime;
    std::optional<std::string> subSecTimeOriginal;
    std::optional<std::string> subSecTimeDigitized;

    // Camera settings
    CameraSettings cameraSettings;

    // Lens information
    LensInfo lensInfo;

    // Image properties
    ImageProperties imageProperties;

    // GPS data (pointer to avoid circular dependency)
    std::shared_ptr<GpsData> gpsData;

    // Thumbnail
    std::optional<std::vector<uint8_t>> thumbnailData;
    std::optional<int> thumbnailWidth;
    std::optional<int> thumbnailHeight;
    std::optional<uint32_t> thumbnailOffset;
    std::optional<uint32_t> thumbnailLength;

    // User comment and description
    std::optional<std::string> userComment;
    std::optional<std::string> imageDescription;

    // Custom/maker notes
    std::optional<std::vector<uint8_t>> makerNote;
    std::unordered_map<std::string, std::string> customTags;

    // Raw IFD entries for complete preservation
    std::vector<IfdEntry> ifd0Entries;
    std::vector<IfdEntry> exifIfdEntries;
    std::vector<IfdEntry> gpsIfdEntries;
    std::vector<IfdEntry> interopIfdEntries;

    /**
     * @brief Check if EXIF data has any meaningful content
     */
    [[nodiscard]] bool hasData() const noexcept {
        return cameraMake.has_value() || cameraModel.has_value() ||
               dateTime.has_value() || cameraSettings.exposureTime.has_value();
    }

    /**
     * @brief Check if GPS data is present
     */
    [[nodiscard]] bool hasGps() const noexcept;

    /**
     * @brief Clear all EXIF data
     */
    void clear() noexcept;

    /**
     * @brief Set a custom tag value
     */
    void setCustomTag(const std::string& key, const std::string& value) {
        customTags[key] = value;
    }

    /**
     * @brief Get a custom tag value
     */
    [[nodiscard]] std::optional<std::string> getCustomTag(
        const std::string& key) const {
        auto it = customTags.find(key);
        if (it != customTags.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

/**
 * @brief EXIF load mode for lazy loading support
 */
enum class ExifLoadMode {
    FULL,    ///< Load all EXIF data immediately
    LAZY,    ///< Load EXIF data on demand
    MINIMAL  ///< Load only essential fields
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_EXIF_TYPES_HPP
