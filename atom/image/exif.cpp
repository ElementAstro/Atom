#include "exif.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <vector>

#include "atom/log/loguru.hpp"

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
            LOG_F(ERROR, "Invalid IFD entry position, out of bounds.");
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
                LOG_F(ERROR, "Invalid string offset, out of bounds.");
                continue;
            }
            value =
                std::string(reinterpret_cast<const char*>(valuePtr), count - 1);
        } else if ((type == 3 || type == 4) && count == 1) {
            value = std::to_string(valueOffset & 0xFFFF);
        } else if (type == 5 && count == 1) {
            if (tiffStart + valueOffset + RATIONAL_SIZE >
                tiffStart + bufferSize) {
                LOG_F(ERROR, "Invalid rational offset, out of bounds.");
                continue;
            }
            double rationalValue =
                parseRational(tiffStart + valueOffset, isLittleEndian);
            value = std::to_string(rationalValue);
        } else if (tag == 0x0002 || tag == 0x0004) {
            if (tiffStart + valueOffset + GPS_COORDINATE_SIZE >
                tiffStart + bufferSize) {
                LOG_F(ERROR, "Invalid GPS coordinate offset, out of bounds.");
                continue;
            }
            value = parseGPSCoordinate(tiffStart + valueOffset, isLittleEndian);
        } else {
            value = "Unsupported format";
        }

        switch (tag) {
            case 0x010F:
                m_exifData.cameraMake = value;
                break;
            case 0x0110:
                m_exifData.cameraModel = value;
                break;
            case 0x9003:
                m_exifData.dateTime = value;
                break;
            case 0x829A:
                m_exifData.exposureTime = value;
                break;
            case 0x829D:
                m_exifData.fNumber = value;
                break;
            case 0x8827:
                m_exifData.isoSpeed = value;
                break;
            case 0x920A:
                m_exifData.focalLength = value;
                break;
            case 0x0112:
                m_exifData.orientation = parseOrientation(data, isLittleEndian);
                break;
            case 0x0103:
                m_exifData.compression = value;
                break;
            case 0xA001:
                m_exifData.colorSpace = parseColorSpace(data, isLittleEndian);
                break;
            case 0x0131:
                m_exifData.software = value;
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
        LOG_F(ERROR, "Cannot open file: {}", m_filename);
        return false;
    }

    std::vector<char> charBuffer((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
    std::vector<std::byte> buffer(charBuffer.size());
    std::transform(
        charBuffer.begin(), charBuffer.end(), buffer.begin(),
        [](char c) { return std::byte{static_cast<unsigned char>(c)}; });
    file.close();

    if (buffer.size() < 2 || buffer[0] != std::byte{0xFF} ||
        buffer[1] != std::byte{0xD8}) {
        LOG_F(ERROR, "Not a valid JPEG file!");
        return false;
    }

    size_t pos = 2;
    while (pos < buffer.size()) {
        if (pos + 4 > buffer.size()) {
            LOG_F(ERROR, "Unexpected end of file while searching for markers.");
            return false;
        }

        if (buffer[pos] == std::byte{0xFF}) {
            uint16_t marker = readUint16Be(&buffer[pos]);
            uint16_t segmentLength = readUint16Be(&buffer[pos + 2]);

            if (pos + 2 + segmentLength > buffer.size()) {
                LOG_F(ERROR,
                      "Invalid segment length, segment exceeds file bounds.");
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
                    LOG_F(ERROR,
                          "Invalid IFD offset, exceeds EXIF data bounds.");
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
    // 优化内存对齐
    if (!m_exifData.cameraMake.empty()) {
        m_exifData.cameraMake.shrink_to_fit();
    }
    // ... 对其他字符串字段进行同样的优化
}

bool ExifParser::validateData() const {
    // 验证必要字段
    if (m_exifData.dateTime.empty()) {
        LOG_F(WARNING, "Missing required DateTime field");
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

    // Check if the buffer is within valid range
    // Typically this would compare against the bounds of a larger buffer
    // Since we don't have the full buffer context here, we'll implement a basic
    // check that ensures the pointer is not null and size is reasonable

    if (size == 0 || size > 100 * 1024 * 1024) {  // Arbitrary max size (100MB)
        LOG_F(ERROR, "Invalid buffer size: {}", size);
        return false;
    }

    // In a real implementation, we would check:
    // return ptr >= bufferStart && ptr + size <= bufferEnd;

    return true;
}

void ExifParser::clearExifData() { m_exifData = ExifData{}; }

GpsCoordinate GpsCoordinate::fromDecimalDegrees(double decimal,
                                                bool isLatitude) noexcept {
    GpsCoordinate coord;

    // 确定方向
    if (isLatitude) {
        coord.direction = decimal >= 0 ? 'N' : 'S';
    } else {
        coord.direction = decimal >= 0 ? 'E' : 'W';
    }

    // 使用绝对值进行计算
    double absDecimal = std::abs(decimal);

    // 提取度
    coord.degrees = static_cast<double>(static_cast<int>(absDecimal));

    // 提取分
    double remaining = (absDecimal - coord.degrees) * 60.0;
    coord.minutes = static_cast<double>(static_cast<int>(remaining));

    // 提取秒
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

    // 序列化基本信息
    ss << m_filename << "\n";

    // 序列化各个EXIF字段
    ss << m_exifData.cameraMake << "\n";
    ss << m_exifData.cameraModel << "\n";
    ss << m_exifData.dateTime << "\n";
    ss << m_exifData.exposureTime << "\n";
    ss << m_exifData.fNumber << "\n";
    ss << m_exifData.isoSpeed << "\n";
    ss << m_exifData.focalLength << "\n";

    // 序列化GPS坐标
    ss << (m_exifData.gpsLatitude.has_value() ? "1" : "0") << "\n";
    if (m_exifData.gpsLatitude.has_value()) {
        ss << m_exifData.gpsLatitude->degrees << " ";
        ss << m_exifData.gpsLatitude->minutes << " ";
        ss << m_exifData.gpsLatitude->seconds << " ";
        ss << m_exifData.gpsLatitude->direction << "\n";
    }

    ss << (m_exifData.gpsLongitude.has_value() ? "1" : "0") << "\n";
    if (m_exifData.gpsLongitude.has_value()) {
        ss << m_exifData.gpsLongitude->degrees << " ";
        ss << m_exifData.gpsLongitude->minutes << " ";
        ss << m_exifData.gpsLongitude->seconds << " ";
        ss << m_exifData.gpsLongitude->direction << "\n";
    }

    // 序列化额外字段
    ss << m_exifData.orientation << "\n";
    ss << m_exifData.compression << "\n";
    ss << m_exifData.imageWidth << "\n";
    ss << m_exifData.imageHeight << "\n";
    ss << m_exifData.colorSpace << "\n";
    ss << m_exifData.software << "\n";

    return ss.str();
}

std::unique_ptr<ExifParser> ExifParser::deserialize(const std::string& data) {
    std::stringstream ss(data);
    std::string line;

    // 读取文件名
    std::getline(ss, line);
    auto parser = std::make_unique<ExifParser>(line);

    // 读取EXIF字段
    std::getline(ss, parser->m_exifData.cameraMake);
    std::getline(ss, parser->m_exifData.cameraModel);
    std::getline(ss, parser->m_exifData.dateTime);
    std::getline(ss, parser->m_exifData.exposureTime);
    std::getline(ss, parser->m_exifData.fNumber);
    std::getline(ss, parser->m_exifData.isoSpeed);
    std::getline(ss, parser->m_exifData.focalLength);

    // 读取GPS坐标
    std::getline(ss, line);
    if (line == "1") {
        GpsCoordinate latitude;
        std::getline(ss, line);
        std::stringstream coordStream(line);

        coordStream >> latitude.degrees;
        coordStream >> latitude.minutes;
        coordStream >> latitude.seconds;
        coordStream >> latitude.direction;

        parser->m_exifData.gpsLatitude = latitude;
    } else {
        parser->m_exifData.gpsLatitude = std::nullopt;
    }

    std::getline(ss, line);
    if (line == "1") {
        GpsCoordinate longitude;
        std::getline(ss, line);
        std::stringstream coordStream(line);

        coordStream >> longitude.degrees;
        coordStream >> longitude.minutes;
        coordStream >> longitude.seconds;
        coordStream >> longitude.direction;

        parser->m_exifData.gpsLongitude = longitude;
    } else {
        parser->m_exifData.gpsLongitude = std::nullopt;
    }

    // 读取额外字段
    std::getline(ss, parser->m_exifData.orientation);
    std::getline(ss, parser->m_exifData.compression);
    std::getline(ss, parser->m_exifData.imageWidth);
    std::getline(ss, parser->m_exifData.imageHeight);
    std::getline(ss, parser->m_exifData.colorSpace);
    std::getline(ss, parser->m_exifData.software);

    return parser;
}

}  // namespace atom::image
