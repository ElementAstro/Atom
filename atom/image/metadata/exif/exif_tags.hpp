#ifndef ATOM_IMAGE_METADATA_EXIF_TAGS_HPP
#define ATOM_IMAGE_METADATA_EXIF_TAGS_HPP

#include <cstdint>
#include <string>
#include <unordered_map>

namespace atom::image::metadata {

/**
 * @brief EXIF tag IDs for IFD0 (main image)
 */
namespace ExifTag {

// Image structure
constexpr uint16_t IMAGE_WIDTH = 0x0100;
constexpr uint16_t IMAGE_HEIGHT = 0x0101;
constexpr uint16_t BITS_PER_SAMPLE = 0x0102;
constexpr uint16_t COMPRESSION = 0x0103;
constexpr uint16_t PHOTOMETRIC_INTERPRETATION = 0x0106;
constexpr uint16_t IMAGE_DESCRIPTION = 0x010E;
constexpr uint16_t MAKE = 0x010F;
constexpr uint16_t MODEL = 0x0110;
constexpr uint16_t STRIP_OFFSETS = 0x0111;
constexpr uint16_t ORIENTATION = 0x0112;
constexpr uint16_t SAMPLES_PER_PIXEL = 0x0115;
constexpr uint16_t ROWS_PER_STRIP = 0x0116;
constexpr uint16_t STRIP_BYTE_COUNTS = 0x0117;
constexpr uint16_t X_RESOLUTION = 0x011A;
constexpr uint16_t Y_RESOLUTION = 0x011B;
constexpr uint16_t PLANAR_CONFIGURATION = 0x011C;
constexpr uint16_t RESOLUTION_UNIT = 0x0128;
constexpr uint16_t TRANSFER_FUNCTION = 0x012D;
constexpr uint16_t SOFTWARE = 0x0131;
constexpr uint16_t DATE_TIME = 0x0132;
constexpr uint16_t ARTIST = 0x013B;
constexpr uint16_t WHITE_POINT = 0x013E;
constexpr uint16_t PRIMARY_CHROMATICITIES = 0x013F;
constexpr uint16_t JPEG_INTERCHANGE_FORMAT = 0x0201;
constexpr uint16_t JPEG_INTERCHANGE_FORMAT_LENGTH = 0x0202;
constexpr uint16_t YCBCR_COEFFICIENTS = 0x0211;
constexpr uint16_t YCBCR_SUB_SAMPLING = 0x0212;
constexpr uint16_t YCBCR_POSITIONING = 0x0213;
constexpr uint16_t REFERENCE_BLACK_WHITE = 0x0214;
constexpr uint16_t COPYRIGHT = 0x8298;

// EXIF IFD pointer
constexpr uint16_t EXIF_IFD_POINTER = 0x8769;
constexpr uint16_t GPS_IFD_POINTER = 0x8825;
constexpr uint16_t INTEROP_IFD_POINTER = 0xA005;

// EXIF specific tags
constexpr uint16_t EXPOSURE_TIME = 0x829A;
constexpr uint16_t F_NUMBER = 0x829D;
constexpr uint16_t EXPOSURE_PROGRAM = 0x8822;
constexpr uint16_t SPECTRAL_SENSITIVITY = 0x8824;
constexpr uint16_t ISO_SPEED_RATINGS = 0x8827;
constexpr uint16_t OECF = 0x8828;
constexpr uint16_t SENSITIVITY_TYPE = 0x8830;
constexpr uint16_t STANDARD_OUTPUT_SENSITIVITY = 0x8831;
constexpr uint16_t RECOMMENDED_EXPOSURE_INDEX = 0x8832;
constexpr uint16_t ISO_SPEED = 0x8833;
constexpr uint16_t ISO_SPEED_LATITUDE_YYY = 0x8834;
constexpr uint16_t ISO_SPEED_LATITUDE_ZZZ = 0x8835;
constexpr uint16_t EXIF_VERSION = 0x9000;
constexpr uint16_t DATE_TIME_ORIGINAL = 0x9003;
constexpr uint16_t DATE_TIME_DIGITIZED = 0x9004;
constexpr uint16_t OFFSET_TIME = 0x9010;
constexpr uint16_t OFFSET_TIME_ORIGINAL = 0x9011;
constexpr uint16_t OFFSET_TIME_DIGITIZED = 0x9012;
constexpr uint16_t COMPONENTS_CONFIGURATION = 0x9101;
constexpr uint16_t COMPRESSED_BITS_PER_PIXEL = 0x9102;
constexpr uint16_t SHUTTER_SPEED_VALUE = 0x9201;
constexpr uint16_t APERTURE_VALUE = 0x9202;
constexpr uint16_t BRIGHTNESS_VALUE = 0x9203;
constexpr uint16_t EXPOSURE_BIAS_VALUE = 0x9204;
constexpr uint16_t MAX_APERTURE_VALUE = 0x9205;
constexpr uint16_t SUBJECT_DISTANCE = 0x9206;
constexpr uint16_t METERING_MODE = 0x9207;
constexpr uint16_t LIGHT_SOURCE = 0x9208;
constexpr uint16_t FLASH = 0x9209;
constexpr uint16_t FOCAL_LENGTH = 0x920A;
constexpr uint16_t SUBJECT_AREA = 0x9214;
constexpr uint16_t MAKER_NOTE = 0x927C;
constexpr uint16_t USER_COMMENT = 0x9286;
constexpr uint16_t SUB_SEC_TIME = 0x9290;
constexpr uint16_t SUB_SEC_TIME_ORIGINAL = 0x9291;
constexpr uint16_t SUB_SEC_TIME_DIGITIZED = 0x9292;
constexpr uint16_t FLASHPIX_VERSION = 0xA000;
constexpr uint16_t COLOR_SPACE = 0xA001;
constexpr uint16_t PIXEL_X_DIMENSION = 0xA002;
constexpr uint16_t PIXEL_Y_DIMENSION = 0xA003;
constexpr uint16_t RELATED_SOUND_FILE = 0xA004;
constexpr uint16_t FLASH_ENERGY = 0xA20B;
constexpr uint16_t SPATIAL_FREQUENCY_RESPONSE = 0xA20C;
constexpr uint16_t FOCAL_PLANE_X_RESOLUTION = 0xA20E;
constexpr uint16_t FOCAL_PLANE_Y_RESOLUTION = 0xA20F;
constexpr uint16_t FOCAL_PLANE_RESOLUTION_UNIT = 0xA210;
constexpr uint16_t SUBJECT_LOCATION = 0xA214;
constexpr uint16_t EXPOSURE_INDEX = 0xA215;
constexpr uint16_t SENSING_METHOD = 0xA217;
constexpr uint16_t FILE_SOURCE = 0xA300;
constexpr uint16_t SCENE_TYPE = 0xA301;
constexpr uint16_t CFA_PATTERN = 0xA302;
constexpr uint16_t CUSTOM_RENDERED = 0xA401;
constexpr uint16_t EXPOSURE_MODE = 0xA402;
constexpr uint16_t WHITE_BALANCE = 0xA403;
constexpr uint16_t DIGITAL_ZOOM_RATIO = 0xA404;
constexpr uint16_t FOCAL_LENGTH_IN_35MM_FILM = 0xA405;
constexpr uint16_t SCENE_CAPTURE_TYPE = 0xA406;
constexpr uint16_t GAIN_CONTROL = 0xA407;
constexpr uint16_t CONTRAST = 0xA408;
constexpr uint16_t SATURATION = 0xA409;
constexpr uint16_t SHARPNESS = 0xA40A;
constexpr uint16_t DEVICE_SETTING_DESCRIPTION = 0xA40B;
constexpr uint16_t SUBJECT_DISTANCE_RANGE = 0xA40C;
constexpr uint16_t IMAGE_UNIQUE_ID = 0xA420;
constexpr uint16_t CAMERA_OWNER_NAME = 0xA430;
constexpr uint16_t BODY_SERIAL_NUMBER = 0xA431;
constexpr uint16_t LENS_SPECIFICATION = 0xA432;
constexpr uint16_t LENS_MAKE = 0xA433;
constexpr uint16_t LENS_MODEL = 0xA434;
constexpr uint16_t LENS_SERIAL_NUMBER = 0xA435;
constexpr uint16_t GAMMA = 0xA500;

}  // namespace ExifTag

/**
 * @brief GPS tag IDs
 */
namespace GpsTag {

constexpr uint16_t VERSION_ID = 0x0000;
constexpr uint16_t LATITUDE_REF = 0x0001;
constexpr uint16_t LATITUDE = 0x0002;
constexpr uint16_t LONGITUDE_REF = 0x0003;
constexpr uint16_t LONGITUDE = 0x0004;
constexpr uint16_t ALTITUDE_REF = 0x0005;
constexpr uint16_t ALTITUDE = 0x0006;
constexpr uint16_t TIME_STAMP = 0x0007;
constexpr uint16_t SATELLITES = 0x0008;
constexpr uint16_t STATUS = 0x0009;
constexpr uint16_t MEASURE_MODE = 0x000A;
constexpr uint16_t DOP = 0x000B;
constexpr uint16_t SPEED_REF = 0x000C;
constexpr uint16_t SPEED = 0x000D;
constexpr uint16_t TRACK_REF = 0x000E;
constexpr uint16_t TRACK = 0x000F;
constexpr uint16_t IMG_DIRECTION_REF = 0x0010;
constexpr uint16_t IMG_DIRECTION = 0x0011;
constexpr uint16_t MAP_DATUM = 0x0012;
constexpr uint16_t DEST_LATITUDE_REF = 0x0013;
constexpr uint16_t DEST_LATITUDE = 0x0014;
constexpr uint16_t DEST_LONGITUDE_REF = 0x0015;
constexpr uint16_t DEST_LONGITUDE = 0x0016;
constexpr uint16_t DEST_BEARING_REF = 0x0017;
constexpr uint16_t DEST_BEARING = 0x0018;
constexpr uint16_t DEST_DISTANCE_REF = 0x0019;
constexpr uint16_t DEST_DISTANCE = 0x001A;
constexpr uint16_t PROCESSING_METHOD = 0x001B;
constexpr uint16_t AREA_INFORMATION = 0x001C;
constexpr uint16_t DATE_STAMP = 0x001D;
constexpr uint16_t DIFFERENTIAL = 0x001E;
constexpr uint16_t H_POSITIONING_ERROR = 0x001F;

}  // namespace GpsTag

/**
 * @brief Interoperability tag IDs
 */
namespace InteropTag {

constexpr uint16_t INTEROPERABILITY_INDEX = 0x0001;
constexpr uint16_t INTEROPERABILITY_VERSION = 0x0002;
constexpr uint16_t RELATED_IMAGE_FILE_FORMAT = 0x1000;
constexpr uint16_t RELATED_IMAGE_WIDTH = 0x1001;
constexpr uint16_t RELATED_IMAGE_HEIGHT = 0x1002;

}  // namespace InteropTag

/**
 * @brief IFD types
 */
enum class IfdType {
    IFD0,        ///< Main image IFD
    IFD1,        ///< Thumbnail IFD
    EXIF_IFD,    ///< EXIF sub-IFD
    GPS_IFD,     ///< GPS sub-IFD
    INTEROP_IFD  ///< Interoperability sub-IFD
};

/**
 * @brief Tag information structure
 */
struct TagInfo {
    uint16_t id;
    const char* name;
    const char* description;
    IfdType ifdType;
};

/**
 * @brief Get tag name by ID
 */
[[nodiscard]] std::string getTagName(uint16_t tag,
                                     IfdType ifdType = IfdType::EXIF_IFD);

/**
 * @brief Get tag description by ID
 */
[[nodiscard]] std::string getTagDescription(
    uint16_t tag, IfdType ifdType = IfdType::EXIF_IFD);

/**
 * @brief Check if tag is an IFD pointer
 */
[[nodiscard]] inline bool isIfdPointer(uint16_t tag) noexcept {
    return tag == ExifTag::EXIF_IFD_POINTER ||
           tag == ExifTag::GPS_IFD_POINTER ||
           tag == ExifTag::INTEROP_IFD_POINTER;
}

/**
 * @brief Get orientation description
 */
[[nodiscard]] std::string getOrientationDescription(int orientation);

/**
 * @brief Get metering mode description
 */
[[nodiscard]] std::string getMeteringModeDescription(int mode);

/**
 * @brief Get exposure program description
 */
[[nodiscard]] std::string getExposureProgramDescription(int program);

/**
 * @brief Get flash description
 */
[[nodiscard]] std::string getFlashDescription(int flash);

/**
 * @brief Get color space description
 */
[[nodiscard]] std::string getColorSpaceDescription(int colorSpace);

/**
 * @brief Get white balance description
 */
[[nodiscard]] std::string getWhiteBalanceDescription(int whiteBalance);

/**
 * @brief Get light source description
 */
[[nodiscard]] std::string getLightSourceDescription(int lightSource);

/**
 * @brief Get compression description
 */
[[nodiscard]] std::string getCompressionDescription(int compression);

/**
 * @brief Get resolution unit description
 */
[[nodiscard]] std::string getResolutionUnitDescription(int unit);

/**
 * @brief Format exposure time as fraction string
 */
[[nodiscard]] std::string formatExposureTime(double seconds);

/**
 * @brief Format focal length with unit
 */
[[nodiscard]] std::string formatFocalLength(double mm);

/**
 * @brief Format f-number
 */
[[nodiscard]] std::string formatFNumber(double f);

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_EXIF_TAGS_HPP
