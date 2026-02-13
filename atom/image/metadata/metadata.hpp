#ifndef ATOM_IMAGE_METADATA_HPP
#define ATOM_IMAGE_METADATA_HPP

/**
 * @file metadata.hpp
 * @brief Unified metadata module header
 *
 * This header provides a single include for all metadata functionality.
 */

#include <fstream>

// Core types
#include "types/exif_types.hpp"
#include "types/gps_types.hpp"
#include "types/iptc_types.hpp"
#include "types/metadata_value.hpp"
#include "types/xmp_types.hpp"

// Utilities
#include "utils/byte_reader.hpp"
#include "utils/byte_writer.hpp"
#include "utils/datetime_utils.hpp"

// EXIF support
#include "exif/exif_reader.hpp"
#include "exif/exif_tags.hpp"
#include "exif/exif_writer.hpp"
#include "exif/gps_parser.hpp"
#include "exif/ifd_parser.hpp"

// IPTC support
#include "iptc/iptc_reader.hpp"
#include "iptc/iptc_writer.hpp"

// XMP support
#include "xmp/xmp_parser.hpp"
#include "xmp/xmp_writer.hpp"

// Tools
#include "tools/batch_processor.hpp"
#include "tools/metadata_validator.hpp"
#include "tools/privacy_sanitizer.hpp"

namespace atom::image::metadata {

/**
 * @brief Read all metadata from a file
 */
struct AllMetadata {
    std::optional<ExifData> exif;
    std::optional<IptcData> iptc;
    std::optional<XmpData> xmp;

    [[nodiscard]] bool hasAny() const noexcept {
        return (exif && exif->hasData()) || (iptc && iptc->hasData()) ||
               (xmp && xmp->hasData());
    }
};

/**
 * @brief Read all metadata from a file
 */
inline AllMetadata readAllMetadata(const std::filesystem::path& file) {
    AllMetadata result;
    result.exif = ExifReader::fromFile(file);
    result.iptc = IptcReader::fromFile(file);
    result.xmp = XmpParser::fromFile(file);
    return result;
}

/**
 * @brief Quick GPS coordinate extraction
 */
inline std::optional<std::pair<double, double>> getGpsCoordinates(
    const std::filesystem::path& file) {
    auto exif = ExifReader::fromFile(file);
    if (exif && exif->gpsData && exif->gpsData->hasPosition()) {
        auto lat = exif->gpsData->getLatitudeDecimal();
        auto lon = exif->gpsData->getLongitudeDecimal();
        if (lat && lon) {
            return std::make_pair(*lat, *lon);
        }
    }
    return std::nullopt;
}

/**
 * @brief Quick date/time extraction
 */
inline std::optional<std::chrono::system_clock::time_point> getDateTime(
    const std::filesystem::path& file) {
    auto exif = ExifReader::fromFile(file);
    if (exif) {
        if (exif->dateTimeOriginal)
            return exif->dateTimeOriginal;
        if (exif->dateTime)
            return exif->dateTime;
    }
    return std::nullopt;
}

/**
 * @brief Strip all metadata from JPEG
 */
inline bool stripAllMetadata(const std::filesystem::path& inputFile,
                             const std::filesystem::path& outputFile) {
    std::ifstream in(inputFile, std::ios::binary);
    if (!in)
        return false;

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    in.close();

    auto stripped = PrivacySanitizer::stripAllMetadata(data);

    std::ofstream out(outputFile, std::ios::binary);
    if (!out)
        return false;

    out.write(reinterpret_cast<const char*>(stripped.data()),
              static_cast<std::streamsize>(stripped.size()));
    return true;
}

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_HPP
