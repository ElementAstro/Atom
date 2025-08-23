#include "exif.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <vector>

#include <spdlog/spdlog.h>

namespace atom::image {

constexpr int BYTE_SHIFT_8 = 8;
constexpr int BYTE_SHIFT_16 = 16;
constexpr int BYTE_SHIFT_24 = 24;
constexpr int EXIF_HEADER_OFFSET = 10;
constexpr int EXIF_HEADER_SIZE = 6;
constexpr int IFD_ENTRY_SIZE = 12;
constexpr int GPS_COORDINATE_SIZE = 24;
constexpr int RATIONAL_SIZE = 8;
constexpr int EXIF_MARKER = 0xFFE1;
constexpr int TIFF_LITTLE_ENDIAN = 0x4949;
constexpr size_t MAX_BUFFER_SIZE = 100 * 1024 * 1024;

ExifParser::ExifParser(const std::string& filename) : m_filename(filename) {}

auto ExifParser::readUint16Be(const std::byte* data) -> uint16_t {
    return static_cast<uint16_t>(std::to_integer<int>(data[0])
                                 << BYTE_SHIFT_8) |
           static_cast<uint16_t>(std::to_integer<int>(data[1]));
}

auto ExifParser::readUint32Be(const std::byte* data) -> uint32_t {
    return (std::to_integer<uint32_t>(data[0]) << BYTE_SHIFT_24) |
           (std::to_integer<uint32_t>(data[1]) << BYTE_SHIFT_16) |
           (std::to_integer<uint32_t>(data[2]) << BYTE_SHIFT_8) |
           std::to_integer<uint32_t>(data[3]);
}

auto ExifParser::readUint16Le(const std::byte* data) -> uint16_t {
    return static_cast<uint16_t>(std::to_integer<int>(data[0])) |
           static_cast<uint16_t>(std::to_integer<int>(data[1]) << BYTE_SHIFT_8);
}

auto ExifParser::readUint32Le(const std::byte* data) -> uint32_t {
    return std::to_integer<uint32_t>(data[0]) |
           (std::to_integer<uint32_t>(data[1]) << BYTE_SHIFT_8) |
           (std::to_integer<uint32_t>(data[2]) << BYTE_SHIFT_16) |
           (std::to_integer<uint32_t>(data[3]) << BYTE_SHIFT_24);
}

auto ExifParser::parseRational(const std::byte* data, bool isLittleEndian)
    -> double {
    uint32_t numerator =
        isLittleEndian ? readUint32Le(data) : readUint32Be(data);
    uint32_t denominator =
        isLittleEndian ? readUint32Le(data + 4) : readUint32Be(data + 4);
    return denominator == 0 ? 0.0
                            : static_cast<double>(numerator) / denominator;
}

auto ExifParser::parseGPSCoordinate(const std::byte* data, bool isLittleEndian)
    -> std::string {
    double degrees = parseRational(data, isLittleEndian);
    double minutes = parseRational(data + RATIONAL_SIZE, isLittleEndian);
    double seconds = parseRational(data + 2 * RATIONAL_SIZE, isLittleEndian);
    double coordinate = degrees + minutes / 60.0 + seconds / 3600.0;
    return std::to_string(coordinate);
}

auto ExifParser::parseIFD(const std::byte* data, bool isLittleEndian,
                          const std::byte* tiffStart, size_t bufferSize)
    -> bool {
    uint16_t entryCount =
        isLittleEndian ? readUint16Le(data) : readUint16Be(data);
    data += 2;

    for (int i = 0; i < entryCount; ++i) {
        if (data + IFD_ENTRY_SIZE > tiffStart + bufferSize) {
            spdlog::error("Invalid IFD entry position, out of bounds");
            return false;
        }

        uint16_t tag = isLittleEndian ? readUint16Le(data) : readUint16Be(data);
        uint16_t type =
            isLittleEndian ? readUint16Le(data + 2) : readUint16Be(data + 2);
        uint32_t count =
            isLittleEndian ? readUint32Le(data + 4) : readUint32Be(data + 4);
        uint32_t valueOffset =
            isLittleEndian ? readUint32Le(data + 8) : readUint32Be(data + 8);
        data += IFD_ENTRY_SIZE;

        std::string value;
        if (type == 2) {
            const std::byte* valuePtr =
                (count <= 4) ? reinterpret_cast<const std::byte*>(&valueOffset)
                             : (tiffStart + valueOffset);
            if (valuePtr + count - 1 > tiffStart + bufferSize) {
                spdlog::error("Invalid string offset, out of bounds");
                continue;
            }
            value =
                std::string(reinterpret_cast<const char*>(valuePtr), count - 1);
        } else if ((type == 3 || type == 4) && count == 1) {
            value = std::to_string(valueOffset & 0xFFFF);
        } else if (type == 5 && count == 1) {
            if (tiffStart + valueOffset + RATIONAL_SIZE >
                tiffStart + bufferSize) {
                spdlog::error("Invalid rational offset, out of bounds");
                continue;
            }
            double rationalValue =
                parseRational(tiffStart + valueOffset, isLittleEndian);
            value = std::to_string(rationalValue);
        } else if (tag == 0x0002 || tag == 0x0004) {
            if (tiffStart + valueOffset + GPS_COORDINATE_SIZE >
                tiffStart + bufferSize) {
                spdlog::error("Invalid GPS coordinate offset, out of bounds");
                continue;
            }
            value = parseGPSCoordinate(tiffStart + valueOffset, isLittleEndian);
        } else {
            value = "Unsupported format";
        }

        switch (tag) {
            case 0x010F:
                m_exifData.cameraMake = std::move(value);
                break;
            case 0x0110:
                m_exifData.cameraModel = std::move(value);
                break;
            case 0x9003:
                m_exifData.dateTime = std::move(value);
                break;
            case 0x829A:
                m_exifData.exposureTime = std::move(value);
                break;
            case 0x829D:
                m_exifData.fNumber = std::move(value);
                break;
            case 0x8827:
                m_exifData.isoSpeed = std::move(value);
                break;
            case 0x920A:
                m_exifData.focalLength = std::move(value);
                break;
            case 0x0112:
                m_exifData.orientation = parseOrientation(data, isLittleEndian);
                break;
            case 0x0103:
                m_exifData.compression = std::move(value);
                break;
            case 0xA001:
                m_exifData.colorSpace = parseColorSpace(data, isLittleEndian);
                break;
            case 0x0131:
                m_exifData.software = std::move(value);
                break;
            default:
                break;
        }
    }
    return true;
}

auto ExifParser::parseColorSpace(const std::byte* data, bool isLittleEndian)
    -> std::string {
    uint16_t colorSpace =
        isLittleEndian ? readUint16Le(data) : readUint16Be(data);
    switch (colorSpace) {
        case 1:
            return "sRGB";
        case 2:
            return "Adobe RGB";
        default:
            return "Unknown";
    }
}

auto ExifParser::parseOrientation(const std::byte* data, bool isLittleEndian)
    -> std::string {
    uint16_t orientation =
        isLittleEndian ? readUint16Le(data) : readUint16Be(data);
    switch (orientation) {
        case 1:
            return "Normal";
        case 3:
            return "Rotate 180";
        case 6:
            return "Rotate 90 CW";
        case 8:
            return "Rotate 270 CW";
        default:
            return "Unknown";
    }
}

auto ExifParser::parse() -> bool {
    std::ifstream file(m_filename, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Cannot open file: {}", m_filename);
        return false;
    }

    std::vector<char> charBuffer((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
    file.close();

    if (charBuffer.size() > MAX_BUFFER_SIZE) {
        spdlog::error("File too large: {} bytes", charBuffer.size());
        return false;
    }

    std::vector<std::byte> buffer;
    buffer.reserve(charBuffer.size());
    std::transform(
        charBuffer.begin(), charBuffer.end(), std::back_inserter(buffer),
        [](char c) { return std::byte{static_cast<unsigned char>(c)}; });

    if (buffer.size() < 2 || buffer[0] != std::byte{0xFF} ||
        buffer[1] != std::byte{0xD8}) {
        spdlog::error("Not a valid JPEG file: {}", m_filename);
        return false;
    }

    size_t pos = 2;
    while (pos < buffer.size()) {
        if (pos + 4 > buffer.size()) {
            spdlog::error("Unexpected end of file while searching for markers");
            return false;
        }

        if (buffer[pos] == std::byte{0xFF}) {
            uint16_t marker = readUint16Be(&buffer[pos]);
            uint16_t segmentLength = readUint16Be(&buffer[pos + 2]);

            if (pos + 2 + segmentLength > buffer.size()) {
                spdlog::error(
                    "Invalid segment length, segment exceeds file bounds");
                return false;
            }

            if (marker == EXIF_MARKER &&
                std::memcmp(&buffer[pos + 4], "Exif\0\0", EXIF_HEADER_SIZE) ==
                    0) {
                const std::byte* tiffHeader = &buffer[pos + EXIF_HEADER_OFFSET];
                uint16_t byteOrder = readUint16Be(tiffHeader);
                bool isLittleEndian = (byteOrder == TIFF_LITTLE_ENDIAN);

                uint32_t ifdOffset = isLittleEndian
                                         ? readUint32Le(&tiffHeader[4])
                                         : readUint32Be(&tiffHeader[4]);

                if (tiffHeader + ifdOffset > &buffer[pos] + segmentLength) {
                    spdlog::error(
                        "Invalid IFD offset, exceeds EXIF data bounds");
                    return false;
                }

                return parseIFD(tiffHeader + ifdOffset, isLittleEndian,
                                tiffHeader, segmentLength);
            }

            pos += 2 + segmentLength;
        } else {
            ++pos;
        }
    }
    return true;
}

auto ExifParser::getExifData() const -> const ExifData& { return m_exifData; }

void ExifParser::optimize() {
    auto shrinkIfNotEmpty = [](std::string& str) {
        if (!str.empty()) {
            str.shrink_to_fit();
        }
    };

    shrinkIfNotEmpty(m_exifData.cameraMake);
    shrinkIfNotEmpty(m_exifData.cameraModel);
    shrinkIfNotEmpty(m_exifData.dateTime);
    shrinkIfNotEmpty(m_exifData.exposureTime);
    shrinkIfNotEmpty(m_exifData.fNumber);
    shrinkIfNotEmpty(m_exifData.isoSpeed);
    shrinkIfNotEmpty(m_exifData.focalLength);
    shrinkIfNotEmpty(m_exifData.orientation);
    shrinkIfNotEmpty(m_exifData.compression);
    shrinkIfNotEmpty(m_exifData.imageWidth);
    shrinkIfNotEmpty(m_exifData.imageHeight);
    shrinkIfNotEmpty(m_exifData.colorSpace);
    shrinkIfNotEmpty(m_exifData.software);
}

bool ExifParser::validateData() const {
    if (m_exifData.dateTime.empty()) {
        spdlog::warn("Missing required DateTime field");
        return false;
    }
    return true;
}

std::unique_ptr<ExifParser> ExifParser::clone() const {
    auto parser = std::make_unique<ExifParser>(m_filename);
    parser->m_exifData = m_exifData;
    return parser;
}

bool ExifParser::validateBufferBounds(const std::byte* ptr, size_t size) const {
    if (ptr == nullptr) {
        return false;
    }

    if (size == 0 || size > MAX_BUFFER_SIZE) {
        spdlog::error("Invalid buffer size: {}", size);
        return false;
    }

    return true;
}

void ExifParser::clearExifData() { m_exifData = ExifData{}; }

GpsCoordinate GpsCoordinate::fromDecimalDegrees(double decimal,
                                                bool isLatitude) noexcept {
    GpsCoordinate coord;

    coord.direction =
        isLatitude ? (decimal >= 0 ? 'N' : 'S') : (decimal >= 0 ? 'E' : 'W');

    double absDecimal = std::abs(decimal);
    coord.degrees = static_cast<double>(static_cast<int>(absDecimal));

    double remaining = (absDecimal - coord.degrees) * 60.0;
    coord.minutes = static_cast<double>(static_cast<int>(remaining));
    coord.seconds = (remaining - coord.minutes) * 60.0;

    return coord;
}

std::string GpsCoordinate::toString() const {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%d°%d'%.2f\"%c",
                  static_cast<int>(degrees), static_cast<int>(minutes), seconds,
                  direction);
    return std::string(buffer);
}

std::string ExifParser::serialize() const {
    std::stringstream ss;

    ss << m_filename << "\n"
       << m_exifData.cameraMake << "\n"
       << m_exifData.cameraModel << "\n"
       << m_exifData.dateTime << "\n"
       << m_exifData.exposureTime << "\n"
       << m_exifData.fNumber << "\n"
       << m_exifData.isoSpeed << "\n"
       << m_exifData.focalLength << "\n";

    ss << (m_exifData.gpsLatitude.has_value() ? "1" : "0") << "\n";
    if (m_exifData.gpsLatitude.has_value()) {
        const auto& lat = *m_exifData.gpsLatitude;
        ss << lat.degrees << " " << lat.minutes << " " << lat.seconds << " "
           << lat.direction << "\n";
    }

    ss << (m_exifData.gpsLongitude.has_value() ? "1" : "0") << "\n";
    if (m_exifData.gpsLongitude.has_value()) {
        const auto& lon = *m_exifData.gpsLongitude;
        ss << lon.degrees << " " << lon.minutes << " " << lon.seconds << " "
           << lon.direction << "\n";
    }

    ss << m_exifData.orientation << "\n"
       << m_exifData.compression << "\n"
       << m_exifData.imageWidth << "\n"
       << m_exifData.imageHeight << "\n"
       << m_exifData.colorSpace << "\n"
       << m_exifData.software << "\n";

    return ss.str();
}

std::unique_ptr<ExifParser> ExifParser::deserialize(const std::string& data) {
    std::stringstream ss(data);
    std::string line;

    std::getline(ss, line);
    auto parser = std::make_unique<ExifParser>(line);

    std::getline(ss, parser->m_exifData.cameraMake);
    std::getline(ss, parser->m_exifData.cameraModel);
    std::getline(ss, parser->m_exifData.dateTime);
    std::getline(ss, parser->m_exifData.exposureTime);
    std::getline(ss, parser->m_exifData.fNumber);
    std::getline(ss, parser->m_exifData.isoSpeed);
    std::getline(ss, parser->m_exifData.focalLength);

    std::getline(ss, line);
    if (line == "1") {
        GpsCoordinate latitude;
        std::getline(ss, line);
        std::stringstream coordStream(line);
        coordStream >> latitude.degrees >> latitude.minutes >>
            latitude.seconds >> latitude.direction;
        parser->m_exifData.gpsLatitude = latitude;
    }

    std::getline(ss, line);
    if (line == "1") {
        GpsCoordinate longitude;
        std::getline(ss, line);
        std::stringstream coordStream(line);
        coordStream >> longitude.degrees >> longitude.minutes >>
            longitude.seconds >> longitude.direction;
        parser->m_exifData.gpsLongitude = longitude;
    }

    std::getline(ss, parser->m_exifData.orientation);
    std::getline(ss, parser->m_exifData.compression);
    std::getline(ss, parser->m_exifData.imageWidth);
    std::getline(ss, parser->m_exifData.imageHeight);
    std::getline(ss, parser->m_exifData.colorSpace);
    std::getline(ss, parser->m_exifData.software);

    return parser;
}

}  // namespace atom::image
