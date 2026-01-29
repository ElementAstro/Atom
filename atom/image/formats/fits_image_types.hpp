/**
 * @file fits_image_types.hpp
 * @brief FITS image type classification and detection system
 *
 * This file provides comprehensive classification of FITS image types
 * based on header keywords, following astronomical conventions and
 * cfitsio standards.
 *
 * @copyright Copyright (C) 2023-2025
 */

#ifndef ATOM_IMAGE_FITS_IMAGE_TYPES_HPP
#define ATOM_IMAGE_FITS_IMAGE_TYPES_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace atom::image::fits {

/**
 * @enum ImageType
 * @brief Primary classification of FITS image types
 */
enum class ImageType {
    PRIMARY_IMAGE,     ///< Standard 2D image in primary HDU
    IMAGE_EXTENSION,   ///< Image in extension HDU
    DATA_CUBE,         ///< 3D or higher dimensional data
    SPECTRAL_CUBE,     ///< Spectral data cube (wavelength axis)
    TEMPORAL_CUBE,     ///< Time-series data cube
    COMPRESSED_IMAGE,  ///< Tile-compressed image
    MOSAIC,            ///< Multi-chip mosaic image
    MULTI_EXTENSION,   ///< Multi-extension FITS file
    BINARY_TABLE,      ///< Binary table extension
    ASCII_TABLE,       ///< ASCII table extension
    RANDOM_GROUPS,     ///< Random groups (radio interferometry)
    UNKNOWN            ///< Unknown or unrecognized type
};

/**
 * @enum CalibrationFrameType
 * @brief Types of calibration frames used in astronomical imaging
 */
enum class CalibrationFrameType {
    NONE,           ///< Not a calibration frame
    BIAS,           ///< Bias/zero frame (zero exposure)
    DARK,           ///< Dark frame (thermal noise)
    FLAT,           ///< Flat field (illumination correction)
    FLAT_DARK,      ///< Dark for flat field
    SKY_FLAT,       ///< Sky flat field
    DOME_FLAT,      ///< Dome flat field
    TWILIGHT_FLAT,  ///< Twilight flat field
    MASTER_BIAS,    ///< Combined master bias
    MASTER_DARK,    ///< Combined master dark
    MASTER_FLAT,    ///< Combined master flat
    BAD_PIXEL_MAP,  ///< Bad pixel mask
    LIGHT,          ///< Science/light frame
    FOCUS,          ///< Focus test frame
    POINTING,       ///< Pointing test frame
    ACQUISITION     ///< Target acquisition frame
};

/**
 * @enum CompressionType
 * @brief FITS tile compression algorithms
 */
enum class CompressionType {
    NONE,       ///< No compression
    RICE,       ///< Rice compression (lossless, integers)
    GZIP,       ///< GZIP compression (general purpose)
    GZIP2,      ///< GZIP with shuffled bytes
    HCOMPRESS,  ///< H-compress (astronomical images)
    PLIO,       ///< Pixel List I/O (masks)
    NOCOMPRESS  ///< Uncompressed tiles
};

/**
 * @enum DataOrganization
 * @brief How data is organized in the FITS file
 */
enum class DataOrganization {
    SINGLE_IMAGE,  ///< Single image
    IMAGE_ARRAY,   ///< Array of images
    TILED_IMAGE,   ///< Tile-compressed image
    ROW_MAJOR,     ///< Row-major ordering
    COLUMN_MAJOR,  ///< Column-major ordering (FITS standard)
    INTERLEAVED,   ///< Interleaved data
    PLANAR         ///< Planar (separate planes)
};

/**
 * @enum ColorType
 * @brief Color interpretation of the image
 */
enum class ColorType {
    GRAYSCALE,       ///< Single channel grayscale
    RGB,             ///< Red-Green-Blue
    RGBA,            ///< RGB with alpha channel
    BGR,             ///< Blue-Green-Red
    BAYER_RGGB,      ///< Bayer pattern RGGB
    BAYER_BGGR,      ///< Bayer pattern BGGR
    BAYER_GRBG,      ///< Bayer pattern GRBG
    BAYER_GBRG,      ///< Bayer pattern GBRG
    MULTI_SPECTRAL,  ///< Multiple spectral bands
    UNKNOWN_COLOR    ///< Unknown color type
};

/**
 * @struct ImageDimensions
 * @brief Complete dimensional information for a FITS image
 */
struct ImageDimensions {
    int naxis = 0;              ///< Number of axes
    std::vector<int64_t> axes;  ///< Size of each axis
    int64_t totalPixels = 0;    ///< Total number of pixels
    int64_t dataSizeBytes = 0;  ///< Total data size in bytes
    int bitpix = 0;             ///< Bits per pixel (BITPIX)

    [[nodiscard]] int64_t width() const noexcept {
        return axes.empty() ? 0 : axes[0];
    }
    [[nodiscard]] int64_t height() const noexcept {
        return axes.size() < 2 ? 1 : axes[1];
    }
    [[nodiscard]] int64_t depth() const noexcept {
        return axes.size() < 3 ? 1 : axes[2];
    }
    [[nodiscard]] bool is2D() const noexcept { return naxis == 2; }
    [[nodiscard]] bool is3D() const noexcept { return naxis == 3; }
    [[nodiscard]] bool isMultiDimensional() const noexcept { return naxis > 3; }
};

/**
 * @struct SpectralInfo
 * @brief Spectral axis information for data cubes
 */
struct SpectralInfo {
    bool hasSpectralAxis = false;      ///< Whether spectral axis exists
    int spectralAxisIndex = -1;        ///< Index of spectral axis (0-based)
    double referenceWavelength = 0.0;  ///< Reference wavelength (CRVAL)
    double wavelengthIncrement = 0.0;  ///< Wavelength per pixel (CDELT)
    double referencePixel = 0.0;       ///< Reference pixel (CRPIX)
    std::string wavelengthUnit;        ///< Unit of wavelength (CUNIT)
    std::string spectralType;          ///< Type of spectral axis (CTYPE)
};

/**
 * @struct TemporalInfo
 * @brief Temporal axis information for time-series data
 */
struct TemporalInfo {
    bool hasTemporalAxis = false;  ///< Whether temporal axis exists
    int temporalAxisIndex = -1;    ///< Index of temporal axis
    double referenceTime = 0.0;    ///< Reference time (MJD or JD)
    double timeIncrement = 0.0;    ///< Time per frame
    std::string timeUnit;          ///< Unit of time
    std::string timeSystem;        ///< Time system (UTC, TAI, etc.)
};

/**
 * @struct CompressionInfo
 * @brief Information about tile compression
 */
struct CompressionInfo {
    CompressionType algorithm = CompressionType::NONE;
    std::vector<int64_t> tileSize;  ///< Size of each tile
    int quantizationLevel = 0;      ///< Quantization level for lossy
    double noiseScale = 0.0;        ///< Noise scaling factor
    std::string zcmptype;           ///< ZCMPTYPE keyword value
    int zbitpix = 0;                ///< Original BITPIX before compression
    std::vector<int64_t> znaxis;    ///< Original dimensions
};

/**
 * @struct ObservationInfo
 * @brief Observation metadata extracted from FITS headers
 */
struct ObservationInfo {
    std::string object;      ///< Object name (OBJECT)
    std::string observer;    ///< Observer name (OBSERVER)
    std::string telescope;   ///< Telescope name (TELESCOP)
    std::string instrument;  ///< Instrument name (INSTRUME)
    std::string filter;      ///< Filter name (FILTER)
    std::string dateObs;     ///< Observation date (DATE-OBS)
    double expTime = 0.0;    ///< Exposure time in seconds (EXPTIME)
    double ra = 0.0;         ///< Right Ascension in degrees
    double dec = 0.0;        ///< Declination in degrees
    double airmass = 0.0;    ///< Airmass (AIRMASS)
    double gain = 0.0;       ///< Detector gain (GAIN)
    double readNoise = 0.0;  ///< Read noise (RDNOISE)
    double ccdTemp = 0.0;    ///< CCD temperature (CCD-TEMP)
    int binX = 1;            ///< X binning factor
    int binY = 1;            ///< Y binning factor
};

/**
 * @struct FITSImageInfo
 * @brief Complete information about a FITS image
 */
struct FITSImageInfo {
    ImageType imageType = ImageType::UNKNOWN;
    CalibrationFrameType calibrationType = CalibrationFrameType::NONE;
    ColorType colorType = ColorType::GRAYSCALE;
    DataOrganization organization = DataOrganization::SINGLE_IMAGE;

    ImageDimensions dimensions;
    SpectralInfo spectral;
    TemporalInfo temporal;
    CompressionInfo compression;
    ObservationInfo observation;

    int hduIndex = 0;     ///< HDU index in file
    std::string hduName;  ///< HDU name (EXTNAME)
    int hduVersion = 0;   ///< HDU version (EXTVER)

    bool hasWCS = false;       ///< Whether WCS is present
    bool hasChecksum = false;  ///< Whether checksum is present
    bool isValid = false;      ///< Whether info is valid

    std::string errorMessage;  ///< Error message if invalid
};

/**
 * @class FITSImageClassifier
 * @brief Classifies FITS images based on header keywords
 *
 * This class analyzes FITS headers to determine the type, organization,
 * and characteristics of FITS images following cfitsio conventions.
 */
class FITSImageClassifier {
public:
    using KeywordGetter =
        std::function<std::optional<std::string>(const std::string&)>;

    /**
     * @brief Default constructor
     */
    FITSImageClassifier() = default;

    /**
     * @brief Classify a FITS HDU based on header keywords
     * @param getKeyword Function to retrieve keyword values
     * @param hduIndex Index of the HDU being classified
     * @return Complete classification information
     */
    [[nodiscard]] FITSImageInfo classify(const KeywordGetter& getKeyword,
                                         int hduIndex = 0) const;

    /**
     * @brief Detect image type from header
     * @param getKeyword Function to retrieve keyword values
     * @param isPrimary Whether this is the primary HDU
     * @return Detected image type
     */
    [[nodiscard]] ImageType detectImageType(const KeywordGetter& getKeyword,
                                            bool isPrimary = true) const;

    /**
     * @brief Detect calibration frame type
     * @param getKeyword Function to retrieve keyword values
     * @return Detected calibration frame type
     */
    [[nodiscard]] CalibrationFrameType detectCalibrationFrame(
        const KeywordGetter& getKeyword) const;

    /**
     * @brief Detect compression type
     * @param getKeyword Function to retrieve keyword values
     * @return Compression information
     */
    [[nodiscard]] CompressionInfo detectCompression(
        const KeywordGetter& getKeyword) const;

    /**
     * @brief Extract image dimensions
     * @param getKeyword Function to retrieve keyword values
     * @return Image dimensions
     */
    [[nodiscard]] ImageDimensions extractDimensions(
        const KeywordGetter& getKeyword) const;

    /**
     * @brief Extract spectral axis information
     * @param getKeyword Function to retrieve keyword values
     * @param dims Image dimensions
     * @return Spectral axis info
     */
    [[nodiscard]] SpectralInfo extractSpectralInfo(
        const KeywordGetter& getKeyword, const ImageDimensions& dims) const;

    /**
     * @brief Extract temporal axis information
     * @param getKeyword Function to retrieve keyword values
     * @param dims Image dimensions
     * @return Temporal axis info
     */
    [[nodiscard]] TemporalInfo extractTemporalInfo(
        const KeywordGetter& getKeyword, const ImageDimensions& dims) const;

    /**
     * @brief Extract observation metadata
     * @param getKeyword Function to retrieve keyword values
     * @return Observation info
     */
    [[nodiscard]] ObservationInfo extractObservationInfo(
        const KeywordGetter& getKeyword) const;

    /**
     * @brief Detect color type from header
     * @param getKeyword Function to retrieve keyword values
     * @param dims Image dimensions
     * @return Detected color type
     */
    [[nodiscard]] ColorType detectColorType(const KeywordGetter& getKeyword,
                                            const ImageDimensions& dims) const;

    /**
     * @brief Check if WCS keywords are present
     * @param getKeyword Function to retrieve keyword values
     * @return True if WCS is present
     */
    [[nodiscard]] bool hasWCS(const KeywordGetter& getKeyword) const;

    /**
     * @brief Check if checksum is present and valid
     * @param getKeyword Function to retrieve keyword values
     * @return True if checksum is present
     */
    [[nodiscard]] bool hasChecksum(const KeywordGetter& getKeyword) const;

private:
    /**
     * @brief Parse integer from keyword value
     */
    [[nodiscard]] std::optional<int64_t> parseInteger(
        const std::optional<std::string>& value) const;

    /**
     * @brief Parse floating point from keyword value
     */
    [[nodiscard]] std::optional<double> parseDouble(
        const std::optional<std::string>& value) const;

    /**
     * @brief Trim whitespace and quotes from string
     */
    [[nodiscard]] std::string trimString(const std::string& str) const;

    /**
     * @brief Convert string to uppercase
     */
    [[nodiscard]] std::string toUpper(const std::string& str) const;
};

/**
 * @brief Convert ImageType to string
 */
[[nodiscard]] std::string imageTypeToString(ImageType type);

/**
 * @brief Convert CalibrationFrameType to string
 */
[[nodiscard]] std::string calibrationTypeToString(CalibrationFrameType type);

/**
 * @brief Convert CompressionType to string
 */
[[nodiscard]] std::string compressionTypeToString(CompressionType type);

/**
 * @brief Convert ColorType to string
 */
[[nodiscard]] std::string colorTypeToString(ColorType type);

}  // namespace atom::image::fits

#endif  // ATOM_IMAGE_FITS_IMAGE_TYPES_HPP
