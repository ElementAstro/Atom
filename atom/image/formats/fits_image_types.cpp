/**
 * @file fits_image_types.cpp
 * @brief Implementation of FITS image type classification system
 *
 * @copyright Copyright (C) 2023-2025
 */

#include "fits_image_types.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <stdexcept>

namespace atom::image::fits {

namespace {

// Common calibration frame keywords and their values
const std::unordered_map<std::string, CalibrationFrameType> IMAGETYP_MAP = {
    {"BIAS", CalibrationFrameType::BIAS},
    {"ZERO", CalibrationFrameType::BIAS},
    {"DARK", CalibrationFrameType::DARK},
    {"FLAT", CalibrationFrameType::FLAT},
    {"FLAT FIELD", CalibrationFrameType::FLAT},
    {"FLATFIELD", CalibrationFrameType::FLAT},
    {"DOME FLAT", CalibrationFrameType::DOME_FLAT},
    {"DOMEFLAT", CalibrationFrameType::DOME_FLAT},
    {"SKY FLAT", CalibrationFrameType::SKY_FLAT},
    {"SKYFLAT", CalibrationFrameType::SKY_FLAT},
    {"TWILIGHT FLAT", CalibrationFrameType::TWILIGHT_FLAT},
    {"TWILIGHT", CalibrationFrameType::TWILIGHT_FLAT},
    {"LIGHT", CalibrationFrameType::LIGHT},
    {"OBJECT", CalibrationFrameType::LIGHT},
    {"SCIENCE", CalibrationFrameType::LIGHT},
    {"FOCUS", CalibrationFrameType::FOCUS},
    {"POINTING", CalibrationFrameType::POINTING},
    {"ACQUISITION", CalibrationFrameType::ACQUISITION},
    {"ACQ", CalibrationFrameType::ACQUISITION}};

// Spectral axis CTYPE values
const std::vector<std::string> SPECTRAL_CTYPES = {
    "FREQ", "ENER", "WAVN", "VRAD", "VOPT",   "ZOPT",
    "AWAV", "VELO", "BETA", "WAVE", "LAMBDA", "WAVELENG"};

// Temporal axis CTYPE values
const std::vector<std::string> TEMPORAL_CTYPES = {
    "TIME", "TAI", "TT",  "TDT", "ET",  "IAT", "UT1",
    "UTC",  "GMT", "GPS", "TCG", "TCB", "TDB", "LOCAL"};

// Compression algorithm names
const std::unordered_map<std::string, CompressionType> COMPRESSION_MAP = {
    {"RICE_1", CompressionType::RICE},
    {"RICE_ONE", CompressionType::RICE},
    {"GZIP_1", CompressionType::GZIP},
    {"GZIP_2", CompressionType::GZIP2},
    {"HCOMPRESS_1", CompressionType::HCOMPRESS},
    {"PLIO_1", CompressionType::PLIO},
    {"NOCOMPRESS", CompressionType::NOCOMPRESS}};

}  // anonymous namespace

FITSImageInfo FITSImageClassifier::classify(const KeywordGetter& getKeyword,
                                            int hduIndex) const {
    FITSImageInfo info;
    info.hduIndex = hduIndex;

    try {
        // Determine if primary HDU
        bool isPrimary = (hduIndex == 0);
        auto simple = getKeyword("SIMPLE");
        auto xtension = getKeyword("XTENSION");

        if (!isPrimary && xtension) {
            isPrimary = false;
        }

        // Extract dimensions first
        info.dimensions = extractDimensions(getKeyword);

        // Detect image type
        info.imageType = detectImageType(getKeyword, isPrimary);

        // Detect calibration type
        info.calibrationType = detectCalibrationFrame(getKeyword);

        // Detect color type
        info.colorType = detectColorType(getKeyword, info.dimensions);

        // Detect compression
        info.compression = detectCompression(getKeyword);
        if (info.compression.algorithm != CompressionType::NONE) {
            info.imageType = ImageType::COMPRESSED_IMAGE;
        }

        // Extract spectral info if 3D+
        if (info.dimensions.naxis >= 3) {
            info.spectral = extractSpectralInfo(getKeyword, info.dimensions);
            if (info.spectral.hasSpectralAxis) {
                info.imageType = ImageType::SPECTRAL_CUBE;
            }

            info.temporal = extractTemporalInfo(getKeyword, info.dimensions);
            if (info.temporal.hasTemporalAxis &&
                info.imageType != ImageType::SPECTRAL_CUBE) {
                info.imageType = ImageType::TEMPORAL_CUBE;
            }

            if (!info.spectral.hasSpectralAxis &&
                !info.temporal.hasTemporalAxis && info.dimensions.naxis == 3) {
                info.imageType = ImageType::DATA_CUBE;
            }
        }

        // Extract observation info
        info.observation = extractObservationInfo(getKeyword);

        // Extract HDU name
        auto extname = getKeyword("EXTNAME");
        if (extname) {
            info.hduName = trimString(*extname);
        }

        auto extver = getKeyword("EXTVER");
        if (extver) {
            auto ver = parseInteger(extver);
            if (ver) {
                info.hduVersion = static_cast<int>(*ver);
            }
        }

        // Check for WCS and checksum
        info.hasWCS = hasWCS(getKeyword);
        info.hasChecksum = hasChecksum(getKeyword);

        // Determine data organization
        if (info.compression.algorithm != CompressionType::NONE) {
            info.organization = DataOrganization::TILED_IMAGE;
        } else if (info.dimensions.naxis > 2) {
            info.organization = DataOrganization::IMAGE_ARRAY;
        } else {
            info.organization = DataOrganization::SINGLE_IMAGE;
        }

        info.isValid = true;

    } catch (const std::exception& e) {
        info.isValid = false;
        info.errorMessage = e.what();
    }

    return info;
}

ImageType FITSImageClassifier::detectImageType(const KeywordGetter& getKeyword,
                                               bool isPrimary) const {
    // Check XTENSION keyword for extension type
    auto xtension = getKeyword("XTENSION");
    if (xtension) {
        std::string ext = toUpper(trimString(*xtension));

        if (ext == "IMAGE" || ext == "'IMAGE   '") {
            return ImageType::IMAGE_EXTENSION;
        }
        if (ext == "BINTABLE" || ext == "'BINTABLE'") {
            return ImageType::BINARY_TABLE;
        }
        if (ext == "TABLE" || ext == "'TABLE   '") {
            return ImageType::ASCII_TABLE;
        }
        if (ext == "COMPRESSED_IMAGE" ||
            ext.find("COMPRESS") != std::string::npos) {
            return ImageType::COMPRESSED_IMAGE;
        }
    }

    // Check for random groups
    auto groups = getKeyword("GROUPS");
    if (groups) {
        std::string g = toUpper(trimString(*groups));
        if (g == "T" || g == "TRUE") {
            return ImageType::RANDOM_GROUPS;
        }
    }

    // Check NAXIS for dimensions
    auto naxisStr = getKeyword("NAXIS");
    if (naxisStr) {
        auto naxis = parseInteger(naxisStr);
        if (naxis) {
            if (*naxis == 0) {
                // Could be a header-only HDU or table
                return isPrimary ? ImageType::PRIMARY_IMAGE
                                 : ImageType::IMAGE_EXTENSION;
            }
            if (*naxis == 2) {
                return isPrimary ? ImageType::PRIMARY_IMAGE
                                 : ImageType::IMAGE_EXTENSION;
            }
            if (*naxis >= 3) {
                return ImageType::DATA_CUBE;
            }
        }
    }

    return isPrimary ? ImageType::PRIMARY_IMAGE : ImageType::UNKNOWN;
}

CalibrationFrameType FITSImageClassifier::detectCalibrationFrame(
    const KeywordGetter& getKeyword) const {
    // Check IMAGETYP keyword (most common)
    auto imagetyp = getKeyword("IMAGETYP");
    if (imagetyp) {
        std::string type = toUpper(trimString(*imagetyp));
        auto it = IMAGETYP_MAP.find(type);
        if (it != IMAGETYP_MAP.end()) {
            return it->second;
        }
    }

    // Check OBSTYPE keyword
    auto obstype = getKeyword("OBSTYPE");
    if (obstype) {
        std::string type = toUpper(trimString(*obstype));
        auto it = IMAGETYP_MAP.find(type);
        if (it != IMAGETYP_MAP.end()) {
            return it->second;
        }
    }

    // Check FRAME keyword
    auto frame = getKeyword("FRAME");
    if (frame) {
        std::string type = toUpper(trimString(*frame));
        auto it = IMAGETYP_MAP.find(type);
        if (it != IMAGETYP_MAP.end()) {
            return it->second;
        }
    }

    // Check for master frames
    auto object = getKeyword("OBJECT");
    if (object) {
        std::string obj = toUpper(trimString(*object));
        if (obj.find("MASTER") != std::string::npos) {
            if (obj.find("BIAS") != std::string::npos) {
                return CalibrationFrameType::MASTER_BIAS;
            }
            if (obj.find("DARK") != std::string::npos) {
                return CalibrationFrameType::MASTER_DARK;
            }
            if (obj.find("FLAT") != std::string::npos) {
                return CalibrationFrameType::MASTER_FLAT;
            }
        }
    }

    // Check exposure time for bias detection
    auto exptime = getKeyword("EXPTIME");
    if (!exptime) {
        exptime = getKeyword("EXPOSURE");
    }
    if (exptime) {
        auto exp = parseDouble(exptime);
        if (exp && *exp == 0.0) {
            // Zero exposure suggests bias frame
            // But only if no IMAGETYP was specified
            return CalibrationFrameType::BIAS;
        }
    }

    return CalibrationFrameType::NONE;
}

CompressionInfo FITSImageClassifier::detectCompression(
    const KeywordGetter& getKeyword) const {
    CompressionInfo info;

    // Check for tile compression (ZCMPTYPE)
    auto zcmptype = getKeyword("ZCMPTYPE");
    if (zcmptype) {
        info.zcmptype = trimString(*zcmptype);
        std::string algo = toUpper(info.zcmptype);

        auto it = COMPRESSION_MAP.find(algo);
        if (it != COMPRESSION_MAP.end()) {
            info.algorithm = it->second;
        } else {
            info.algorithm = CompressionType::GZIP;  // Default assumption
        }

        // Get original BITPIX
        auto zbitpix = getKeyword("ZBITPIX");
        if (zbitpix) {
            auto val = parseInteger(zbitpix);
            if (val) {
                info.zbitpix = static_cast<int>(*val);
            }
        }

        // Get original dimensions
        auto znaxis = getKeyword("ZNAXIS");
        if (znaxis) {
            auto naxis = parseInteger(znaxis);
            if (naxis) {
                for (int i = 1; i <= *naxis; ++i) {
                    auto znaxisN = getKeyword("ZNAXIS" + std::to_string(i));
                    if (znaxisN) {
                        auto val = parseInteger(znaxisN);
                        if (val) {
                            info.znaxis.push_back(*val);
                        }
                    }
                }
            }
        }

        // Get tile size
        auto ztile1 = getKeyword("ZTILE1");
        if (ztile1) {
            auto val = parseInteger(ztile1);
            if (val) {
                info.tileSize.push_back(*val);
            }
        }
        auto ztile2 = getKeyword("ZTILE2");
        if (ztile2) {
            auto val = parseInteger(ztile2);
            if (val) {
                info.tileSize.push_back(*val);
            }
        }

        // Get quantization level
        auto zquantiz = getKeyword("ZQUANTIZ");
        auto zval1 = getKeyword("ZVAL1");
        if (zval1) {
            auto val = parseInteger(zval1);
            if (val) {
                info.quantizationLevel = static_cast<int>(*val);
            }
        }
    }

    return info;
}

ImageDimensions FITSImageClassifier::extractDimensions(
    const KeywordGetter& getKeyword) const {
    ImageDimensions dims;

    // Get BITPIX
    auto bitpix = getKeyword("BITPIX");
    if (bitpix) {
        auto val = parseInteger(bitpix);
        if (val) {
            dims.bitpix = static_cast<int>(*val);
        }
    }

    // Get NAXIS
    auto naxisStr = getKeyword("NAXIS");
    if (naxisStr) {
        auto naxis = parseInteger(naxisStr);
        if (naxis) {
            dims.naxis = static_cast<int>(*naxis);
        }
    }

    // Get each axis dimension
    dims.totalPixels = 1;
    for (int i = 1; i <= dims.naxis; ++i) {
        auto naxisN = getKeyword("NAXIS" + std::to_string(i));
        if (naxisN) {
            auto val = parseInteger(naxisN);
            if (val) {
                dims.axes.push_back(*val);
                dims.totalPixels *= *val;
            }
        }
    }

    // Calculate data size
    if (dims.bitpix != 0 && dims.totalPixels > 0) {
        dims.dataSizeBytes = dims.totalPixels * std::abs(dims.bitpix) / 8;
    }

    return dims;
}

SpectralInfo FITSImageClassifier::extractSpectralInfo(
    const KeywordGetter& getKeyword, const ImageDimensions& dims) const {
    SpectralInfo info;

    // Check each axis for spectral type
    for (int i = 1; i <= dims.naxis; ++i) {
        auto ctype = getKeyword("CTYPE" + std::to_string(i));
        if (ctype) {
            std::string type = toUpper(trimString(*ctype));

            // Check if this is a spectral axis
            for (const auto& spectralType : SPECTRAL_CTYPES) {
                if (type.find(spectralType) != std::string::npos) {
                    info.hasSpectralAxis = true;
                    info.spectralAxisIndex = i - 1;  // 0-based
                    info.spectralType = trimString(*ctype);

                    // Get reference values
                    auto crval = getKeyword("CRVAL" + std::to_string(i));
                    if (crval) {
                        auto val = parseDouble(crval);
                        if (val) {
                            info.referenceWavelength = *val;
                        }
                    }

                    auto cdelt = getKeyword("CDELT" + std::to_string(i));
                    if (cdelt) {
                        auto val = parseDouble(cdelt);
                        if (val) {
                            info.wavelengthIncrement = *val;
                        }
                    }

                    auto crpix = getKeyword("CRPIX" + std::to_string(i));
                    if (crpix) {
                        auto val = parseDouble(crpix);
                        if (val) {
                            info.referencePixel = *val;
                        }
                    }

                    auto cunit = getKeyword("CUNIT" + std::to_string(i));
                    if (cunit) {
                        info.wavelengthUnit = trimString(*cunit);
                    }

                    break;
                }
            }

            if (info.hasSpectralAxis)
                break;
        }
    }

    return info;
}

TemporalInfo FITSImageClassifier::extractTemporalInfo(
    const KeywordGetter& getKeyword, const ImageDimensions& dims) const {
    TemporalInfo info;

    // Check each axis for temporal type
    for (int i = 1; i <= dims.naxis; ++i) {
        auto ctype = getKeyword("CTYPE" + std::to_string(i));
        if (ctype) {
            std::string type = toUpper(trimString(*ctype));

            for (const auto& temporalType : TEMPORAL_CTYPES) {
                if (type.find(temporalType) != std::string::npos) {
                    info.hasTemporalAxis = true;
                    info.temporalAxisIndex = i - 1;
                    info.timeSystem = trimString(*ctype);

                    auto crval = getKeyword("CRVAL" + std::to_string(i));
                    if (crval) {
                        auto val = parseDouble(crval);
                        if (val) {
                            info.referenceTime = *val;
                        }
                    }

                    auto cdelt = getKeyword("CDELT" + std::to_string(i));
                    if (cdelt) {
                        auto val = parseDouble(cdelt);
                        if (val) {
                            info.timeIncrement = *val;
                        }
                    }

                    auto cunit = getKeyword("CUNIT" + std::to_string(i));
                    if (cunit) {
                        info.timeUnit = trimString(*cunit);
                    }

                    break;
                }
            }

            if (info.hasTemporalAxis)
                break;
        }
    }

    return info;
}

ObservationInfo FITSImageClassifier::extractObservationInfo(
    const KeywordGetter& getKeyword) const {
    ObservationInfo info;

    // Basic keywords
    auto object = getKeyword("OBJECT");
    if (object)
        info.object = trimString(*object);

    auto observer = getKeyword("OBSERVER");
    if (observer)
        info.observer = trimString(*observer);

    auto telescop = getKeyword("TELESCOP");
    if (telescop)
        info.telescope = trimString(*telescop);

    auto instrume = getKeyword("INSTRUME");
    if (instrume)
        info.instrument = trimString(*instrume);

    auto filter = getKeyword("FILTER");
    if (filter)
        info.filter = trimString(*filter);

    auto dateobs = getKeyword("DATE-OBS");
    if (dateobs)
        info.dateObs = trimString(*dateobs);

    // Numeric keywords
    auto exptime = getKeyword("EXPTIME");
    if (!exptime)
        exptime = getKeyword("EXPOSURE");
    if (exptime) {
        auto val = parseDouble(exptime);
        if (val)
            info.expTime = *val;
    }

    auto ra = getKeyword("RA");
    if (!ra)
        ra = getKeyword("OBJCTRA");
    if (ra) {
        auto val = parseDouble(ra);
        if (val)
            info.ra = *val;
    }

    auto dec = getKeyword("DEC");
    if (!dec)
        dec = getKeyword("OBJCTDEC");
    if (dec) {
        auto val = parseDouble(dec);
        if (val)
            info.dec = *val;
    }

    auto airmass = getKeyword("AIRMASS");
    if (airmass) {
        auto val = parseDouble(airmass);
        if (val)
            info.airmass = *val;
    }

    auto gain = getKeyword("GAIN");
    if (gain) {
        auto val = parseDouble(gain);
        if (val)
            info.gain = *val;
    }

    auto rdnoise = getKeyword("RDNOISE");
    if (!rdnoise)
        rdnoise = getKeyword("READNOIS");
    if (rdnoise) {
        auto val = parseDouble(rdnoise);
        if (val)
            info.readNoise = *val;
    }

    auto ccdtemp = getKeyword("CCD-TEMP");
    if (!ccdtemp)
        ccdtemp = getKeyword("CCDTEMP");
    if (ccdtemp) {
        auto val = parseDouble(ccdtemp);
        if (val)
            info.ccdTemp = *val;
    }

    // Binning
    auto xbinning = getKeyword("XBINNING");
    if (!xbinning)
        xbinning = getKeyword("BINX");
    if (xbinning) {
        auto val = parseInteger(xbinning);
        if (val)
            info.binX = static_cast<int>(*val);
    }

    auto ybinning = getKeyword("YBINNING");
    if (!ybinning)
        ybinning = getKeyword("BINY");
    if (ybinning) {
        auto val = parseInteger(ybinning);
        if (val)
            info.binY = static_cast<int>(*val);
    }

    return info;
}

ColorType FITSImageClassifier::detectColorType(
    const KeywordGetter& getKeyword, const ImageDimensions& dims) const {
    // Check COLORTYP keyword
    auto colortyp = getKeyword("COLORTYP");
    if (colortyp) {
        std::string type = toUpper(trimString(*colortyp));
        if (type == "RGB")
            return ColorType::RGB;
        if (type == "RGBA")
            return ColorType::RGBA;
        if (type == "BGR")
            return ColorType::BGR;
    }

    // Check BAYERPAT keyword for Bayer pattern
    auto bayerpat = getKeyword("BAYERPAT");
    if (bayerpat) {
        std::string pattern = toUpper(trimString(*bayerpat));
        if (pattern == "RGGB")
            return ColorType::BAYER_RGGB;
        if (pattern == "BGGR")
            return ColorType::BAYER_BGGR;
        if (pattern == "GRBG")
            return ColorType::BAYER_GRBG;
        if (pattern == "GBRG")
            return ColorType::BAYER_GBRG;
    }

    // Check NAXIS3 for color channels
    if (dims.naxis >= 3 && dims.axes.size() >= 3) {
        int64_t depth = dims.axes[2];
        if (depth == 3) {
            // Could be RGB
            auto ctype3 = getKeyword("CTYPE3");
            if (ctype3) {
                std::string type = toUpper(trimString(*ctype3));
                if (type.find("RGB") != std::string::npos) {
                    return ColorType::RGB;
                }
            }
            return ColorType::RGB;  // Assume RGB for 3-channel
        }
        if (depth == 4) {
            return ColorType::RGBA;
        }
        if (depth > 4) {
            return ColorType::MULTI_SPECTRAL;
        }
    }

    return ColorType::GRAYSCALE;
}

bool FITSImageClassifier::hasWCS(const KeywordGetter& getKeyword) const {
    // Check for basic WCS keywords
    auto ctype1 = getKeyword("CTYPE1");
    auto crpix1 = getKeyword("CRPIX1");
    auto crval1 = getKeyword("CRVAL1");

    // Must have at least CTYPE1, CRPIX1, and CRVAL1
    if (ctype1 && crpix1 && crval1) {
        return true;
    }

    // Check for CD matrix
    auto cd1_1 = getKeyword("CD1_1");
    if (cd1_1) {
        return true;
    }

    // Check for CDELT
    auto cdelt1 = getKeyword("CDELT1");
    if (cdelt1 && crpix1 && crval1) {
        return true;
    }

    return false;
}

bool FITSImageClassifier::hasChecksum(const KeywordGetter& getKeyword) const {
    auto checksum = getKeyword("CHECKSUM");
    auto datasum = getKeyword("DATASUM");

    return checksum.has_value() || datasum.has_value();
}

std::optional<int64_t> FITSImageClassifier::parseInteger(
    const std::optional<std::string>& value) const {
    if (!value || value->empty()) {
        return std::nullopt;
    }

    std::string str = trimString(*value);

    // Remove any trailing comments
    auto commentPos = str.find('/');
    if (commentPos != std::string::npos) {
        str = str.substr(0, commentPos);
        str = trimString(str);
    }

    try {
        int64_t result;
        auto [ptr, ec] =
            std::from_chars(str.data(), str.data() + str.size(), result);
        if (ec == std::errc{}) {
            return result;
        }
    } catch (...) {
    }

    // Fallback to stoll
    try {
        return std::stoll(str);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> FITSImageClassifier::parseDouble(
    const std::optional<std::string>& value) const {
    if (!value || value->empty()) {
        return std::nullopt;
    }

    std::string str = trimString(*value);

    // Remove any trailing comments
    auto commentPos = str.find('/');
    if (commentPos != std::string::npos) {
        str = str.substr(0, commentPos);
        str = trimString(str);
    }

    // Handle FITS 'D' exponent notation
    std::replace(str.begin(), str.end(), 'D', 'E');
    std::replace(str.begin(), str.end(), 'd', 'e');

    try {
        return std::stod(str);
    } catch (...) {
        return std::nullopt;
    }
}

std::string FITSImageClassifier::trimString(const std::string& str) const {
    if (str.empty())
        return str;

    size_t start = 0;
    size_t end = str.size();

    // Trim leading whitespace and quotes
    while (start < end &&
           (std::isspace(static_cast<unsigned char>(str[start])) ||
            str[start] == '\'' || str[start] == '"')) {
        ++start;
    }

    // Trim trailing whitespace and quotes
    while (end > start &&
           (std::isspace(static_cast<unsigned char>(str[end - 1])) ||
            str[end - 1] == '\'' || str[end - 1] == '"')) {
        --end;
    }

    return str.substr(start, end - start);
}

std::string FITSImageClassifier::toUpper(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

// String conversion functions

std::string imageTypeToString(ImageType type) {
    switch (type) {
        case ImageType::PRIMARY_IMAGE:
            return "Primary Image";
        case ImageType::IMAGE_EXTENSION:
            return "Image Extension";
        case ImageType::DATA_CUBE:
            return "Data Cube";
        case ImageType::SPECTRAL_CUBE:
            return "Spectral Cube";
        case ImageType::TEMPORAL_CUBE:
            return "Temporal Cube";
        case ImageType::COMPRESSED_IMAGE:
            return "Compressed Image";
        case ImageType::MOSAIC:
            return "Mosaic";
        case ImageType::MULTI_EXTENSION:
            return "Multi-Extension";
        case ImageType::BINARY_TABLE:
            return "Binary Table";
        case ImageType::ASCII_TABLE:
            return "ASCII Table";
        case ImageType::RANDOM_GROUPS:
            return "Random Groups";
        case ImageType::UNKNOWN:
            return "Unknown";
    }
    return "Unknown";
}

std::string calibrationTypeToString(CalibrationFrameType type) {
    switch (type) {
        case CalibrationFrameType::NONE:
            return "None";
        case CalibrationFrameType::BIAS:
            return "Bias";
        case CalibrationFrameType::DARK:
            return "Dark";
        case CalibrationFrameType::FLAT:
            return "Flat";
        case CalibrationFrameType::FLAT_DARK:
            return "Flat Dark";
        case CalibrationFrameType::SKY_FLAT:
            return "Sky Flat";
        case CalibrationFrameType::DOME_FLAT:
            return "Dome Flat";
        case CalibrationFrameType::TWILIGHT_FLAT:
            return "Twilight Flat";
        case CalibrationFrameType::MASTER_BIAS:
            return "Master Bias";
        case CalibrationFrameType::MASTER_DARK:
            return "Master Dark";
        case CalibrationFrameType::MASTER_FLAT:
            return "Master Flat";
        case CalibrationFrameType::BAD_PIXEL_MAP:
            return "Bad Pixel Map";
        case CalibrationFrameType::LIGHT:
            return "Light";
        case CalibrationFrameType::FOCUS:
            return "Focus";
        case CalibrationFrameType::POINTING:
            return "Pointing";
        case CalibrationFrameType::ACQUISITION:
            return "Acquisition";
    }
    return "None";
}

std::string compressionTypeToString(CompressionType type) {
    switch (type) {
        case CompressionType::NONE:
            return "None";
        case CompressionType::RICE:
            return "Rice";
        case CompressionType::GZIP:
            return "GZIP";
        case CompressionType::GZIP2:
            return "GZIP2";
        case CompressionType::HCOMPRESS:
            return "HCompress";
        case CompressionType::PLIO:
            return "PLIO";
        case CompressionType::NOCOMPRESS:
            return "No Compress";
    }
    return "None";
}

std::string colorTypeToString(ColorType type) {
    switch (type) {
        case ColorType::GRAYSCALE:
            return "Grayscale";
        case ColorType::RGB:
            return "RGB";
        case ColorType::RGBA:
            return "RGBA";
        case ColorType::BGR:
            return "BGR";
        case ColorType::BAYER_RGGB:
            return "Bayer RGGB";
        case ColorType::BAYER_BGGR:
            return "Bayer BGGR";
        case ColorType::BAYER_GRBG:
            return "Bayer GRBG";
        case ColorType::BAYER_GBRG:
            return "Bayer GBRG";
        case ColorType::MULTI_SPECTRAL:
            return "Multi-Spectral";
        case ColorType::UNKNOWN_COLOR:
            return "Unknown";
    }
    return "Unknown";
}

}  // namespace atom::image::fits
