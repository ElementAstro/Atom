#include "ifd_parser.hpp"

#include <cstring>
#include <sstream>

namespace atom::image::metadata {

namespace {

constexpr size_t IFD_ENTRY_SIZE = 12;
constexpr size_t TIFF_HEADER_SIZE = 8;
constexpr uint16_t TIFF_MAGIC_LE = 0x4949;  // "II" - Intel
constexpr uint16_t TIFF_MAGIC_BE = 0x4D4D;  // "MM" - Motorola
constexpr uint16_t TIFF_MARKER = 0x002A;

}  // namespace

// ParsedIfdEntry implementation

std::string ParsedIfdEntry::asString() const {
    if (stringValue.has_value()) {
        return *stringValue;
    }
    if (doubleValue.has_value()) {
        std::ostringstream oss;
        oss << *doubleValue;
        return oss.str();
    }
    if (intValue.has_value()) {
        return std::to_string(*intValue);
    }
    if (rationalValue.has_value()) {
        std::ostringstream oss;
        oss << rationalValue->numerator << "/" << rationalValue->denominator;
        return oss.str();
    }
    if (!rationalArray.empty()) {
        std::ostringstream oss;
        for (size_t i = 0; i < rationalArray.size(); ++i) {
            if (i > 0)
                oss << ", ";
            oss << rationalArray[i].numerator << "/"
                << rationalArray[i].denominator;
        }
        return oss.str();
    }
    return "";
}

std::optional<double> ParsedIfdEntry::asDouble() const {
    if (doubleValue.has_value()) {
        return *doubleValue;
    }
    if (intValue.has_value()) {
        return static_cast<double>(*intValue);
    }
    if (rationalValue.has_value()) {
        return rationalValue->toDouble();
    }
    if (!shortArray.empty()) {
        return static_cast<double>(shortArray[0]);
    }
    if (!longArray.empty()) {
        return static_cast<double>(longArray[0]);
    }
    return std::nullopt;
}

std::optional<int64_t> ParsedIfdEntry::asInt() const {
    if (intValue.has_value()) {
        return *intValue;
    }
    if (doubleValue.has_value()) {
        return static_cast<int64_t>(*doubleValue);
    }
    if (rationalValue.has_value()) {
        return static_cast<int64_t>(rationalValue->toDouble());
    }
    if (!shortArray.empty()) {
        return static_cast<int64_t>(shortArray[0]);
    }
    if (!longArray.empty()) {
        return static_cast<int64_t>(longArray[0]);
    }
    return std::nullopt;
}

// IfdParseResult implementation

const ParsedIfdEntry* IfdParseResult::findEntry(uint16_t tag) const {
    for (const auto& entry : entries) {
        if (entry.tag == tag) {
            return &entry;
        }
    }
    return nullptr;
}

std::optional<std::string> IfdParseResult::getString(uint16_t tag) const {
    if (const auto* entry = findEntry(tag)) {
        return entry->asString();
    }
    return std::nullopt;
}

std::optional<double> IfdParseResult::getDouble(uint16_t tag) const {
    if (const auto* entry = findEntry(tag)) {
        return entry->asDouble();
    }
    return std::nullopt;
}

std::optional<int64_t> IfdParseResult::getInt(uint16_t tag) const {
    if (const auto* entry = findEntry(tag)) {
        return entry->asInt();
    }
    return std::nullopt;
}

std::optional<Rational> IfdParseResult::getRational(uint16_t tag) const {
    if (const auto* entry = findEntry(tag)) {
        return entry->rationalValue;
    }
    return std::nullopt;
}

std::vector<Rational> IfdParseResult::getRationalArray(uint16_t tag) const {
    if (const auto* entry = findEntry(tag)) {
        return entry->rationalArray;
    }
    return {};
}

bool IfdParseResult::hasTag(uint16_t tag) const {
    return findEntry(tag) != nullptr;
}

// IfdParser implementation

IfdParser::IfdParser(const std::byte* tiffStart, size_t tiffSize,
                     ByteOrder byteOrder)
    : reader_(tiffStart, tiffSize, byteOrder) {}

IfdParser::IfdParser(ByteReader reader) : reader_(std::move(reader)) {}

std::optional<IfdParseResult> IfdParser::parseIfd(uint32_t offset,
                                                  IfdType ifdType) {
    if (currentDepth_ >= maxDepth_) {
        lastError_ = "Maximum IFD recursion depth exceeded";
        return std::nullopt;
    }

    if (!isValidOffset(offset, 2)) {
        lastError_ = "Invalid IFD offset: " + std::to_string(offset);
        return std::nullopt;
    }

    IfdParseResult result;
    result.type = ifdType;

    // Read entry count
    uint16_t entryCount = reader_.peekUint16At(offset);
    if (entryCount == 0 || entryCount > maxEntries_) {
        lastError_ = "Invalid entry count: " + std::to_string(entryCount);
        return std::nullopt;
    }

    size_t entriesSize = static_cast<size_t>(entryCount) * IFD_ENTRY_SIZE;
    if (!isValidOffset(offset + 2, entriesSize + 4)) {
        lastError_ = "IFD entries exceed buffer bounds";
        return std::nullopt;
    }

    // Parse entries
    size_t entryOffset = offset + 2;
    for (uint16_t i = 0; i < entryCount; ++i) {
        auto entry = parseEntry(entryOffset);
        if (entry) {
            // Check for sub-IFD pointers
            if (entry->tag == ExifTag::EXIF_IFD_POINTER) {
                result.exifIfdOffset = static_cast<uint32_t>(
                    entry->intValue.value_or(entry->valueOffset));
            } else if (entry->tag == ExifTag::GPS_IFD_POINTER) {
                result.gpsIfdOffset = static_cast<uint32_t>(
                    entry->intValue.value_or(entry->valueOffset));
            } else if (entry->tag == ExifTag::INTEROP_IFD_POINTER) {
                result.interopIfdOffset = static_cast<uint32_t>(
                    entry->intValue.value_or(entry->valueOffset));
            } else if (entry->tag == ExifTag::JPEG_INTERCHANGE_FORMAT) {
                result.thumbnailOffset =
                    static_cast<uint32_t>(entry->intValue.value_or(0));
            } else if (entry->tag == ExifTag::JPEG_INTERCHANGE_FORMAT_LENGTH) {
                result.thumbnailLength =
                    static_cast<uint32_t>(entry->intValue.value_or(0));
            }

            result.entries.push_back(std::move(*entry));
        }
        entryOffset += IFD_ENTRY_SIZE;
    }

    // Read next IFD offset
    result.nextIfdOffset = reader_.peekUint32At(entryOffset);

    return result;
}

std::vector<IfdParseResult> IfdParser::parseIfdChain(uint32_t firstIfdOffset) {
    std::vector<IfdParseResult> results;
    uint32_t currentOffset = firstIfdOffset;
    int ifdIndex = 0;

    while (currentOffset != 0 && ifdIndex < 10) {  // Limit chain length
        IfdType type = (ifdIndex == 0) ? IfdType::IFD0 : IfdType::IFD1;
        auto result = parseIfd(currentOffset, type);
        if (!result) {
            break;
        }

        currentOffset = result->nextIfdOffset;
        results.push_back(std::move(*result));
        ++ifdIndex;
    }

    return results;
}

std::optional<IfdParseResult> IfdParser::parseExifIfd(uint32_t offset) {
    ++currentDepth_;
    auto result = parseIfd(offset, IfdType::EXIF_IFD);
    --currentDepth_;
    return result;
}

std::optional<IfdParseResult> IfdParser::parseGpsIfd(uint32_t offset) {
    ++currentDepth_;
    auto result = parseIfd(offset, IfdType::GPS_IFD);
    --currentDepth_;
    return result;
}

std::optional<IfdParseResult> IfdParser::parseInteropIfd(uint32_t offset) {
    ++currentDepth_;
    auto result = parseIfd(offset, IfdType::INTEROP_IFD);
    --currentDepth_;
    return result;
}

std::optional<ParsedIfdEntry> IfdParser::parseEntry(size_t entryOffset) {
    if (!isValidOffset(static_cast<uint32_t>(entryOffset), IFD_ENTRY_SIZE)) {
        return std::nullopt;
    }

    ParsedIfdEntry entry;
    entry.tag = reader_.peekUint16At(entryOffset);
    uint16_t typeVal = reader_.peekUint16At(entryOffset + 2);
    entry.count = reader_.peekUint32At(entryOffset + 4);
    entry.valueOffset = reader_.peekUint32At(entryOffset + 8);

    // Validate type
    if (typeVal < 1 || typeVal > 12) {
        entry.type = ExifDataType::UNDEFINED;
    } else {
        entry.type = static_cast<ExifDataType>(typeVal);
    }

    // Validate count
    if (entry.count == 0 || entry.count > 0x10000000) {
        return std::nullopt;
    }

    // Read value data
    size_t valueSize = getValueSize(entry.type, entry.count);
    uint32_t valueOffset = (valueSize <= 4)
                               ? static_cast<uint32_t>(entryOffset + 8)
                               : entry.valueOffset;

    if (isValidOffset(valueOffset, valueSize)) {
        readEntryValue(entry, valueOffset);
        convertEntryValue(entry);
    }

    return entry;
}

void IfdParser::readEntryValue(ParsedIfdEntry& entry, uint32_t valueOffset) {
    size_t valueSize = getValueSize(entry.type, entry.count);

    if (!isValidOffset(valueOffset, valueSize)) {
        return;
    }

    // Store raw data
    entry.rawData.resize(valueSize);
    std::memcpy(entry.rawData.data(), reader_.at(valueOffset), valueSize);
}

void IfdParser::convertEntryValue(ParsedIfdEntry& entry) {
    if (entry.rawData.empty()) {
        return;
    }

    ByteReader valueReader(entry.rawData.data(), entry.rawData.size(),
                           reader_.getByteOrder());

    switch (entry.type) {
        case ExifDataType::BYTE:
        case ExifDataType::UNDEFINED:
            entry.byteArray.reserve(entry.count);
            for (uint32_t i = 0; i < entry.count && !valueReader.eof(); ++i) {
                entry.byteArray.push_back(valueReader.readUint8());
            }
            if (entry.count == 1) {
                entry.intValue = entry.byteArray[0];
            }
            break;

        case ExifDataType::ASCII:
            entry.stringValue =
                std::string(reinterpret_cast<const char*>(entry.rawData.data()),
                            entry.rawData.size());
            // Remove trailing null
            if (!entry.stringValue->empty() &&
                entry.stringValue->back() == '\0') {
                entry.stringValue->pop_back();
            }
            // Trim trailing whitespace
            while (!entry.stringValue->empty() &&
                   (entry.stringValue->back() == ' ' ||
                    entry.stringValue->back() == '\0')) {
                entry.stringValue->pop_back();
            }
            break;

        case ExifDataType::SHORT:
            entry.shortArray.reserve(entry.count);
            for (uint32_t i = 0;
                 i < entry.count && valueReader.remaining() >= 2; ++i) {
                entry.shortArray.push_back(valueReader.readUint16());
            }
            if (entry.count == 1 && !entry.shortArray.empty()) {
                entry.intValue = entry.shortArray[0];
            }
            break;

        case ExifDataType::LONG:
            entry.longArray.reserve(entry.count);
            for (uint32_t i = 0;
                 i < entry.count && valueReader.remaining() >= 4; ++i) {
                entry.longArray.push_back(valueReader.readUint32());
            }
            if (entry.count == 1 && !entry.longArray.empty()) {
                entry.intValue = entry.longArray[0];
            }
            break;

        case ExifDataType::RATIONAL:
            entry.rationalArray.reserve(entry.count);
            for (uint32_t i = 0;
                 i < entry.count && valueReader.remaining() >= 8; ++i) {
                entry.rationalArray.push_back(valueReader.readRational());
            }
            if (entry.count == 1 && !entry.rationalArray.empty()) {
                entry.rationalValue = entry.rationalArray[0];
                entry.doubleValue = entry.rationalArray[0].toDouble();
            }
            break;

        case ExifDataType::SBYTE:
            entry.byteArray.reserve(entry.count);
            for (uint32_t i = 0; i < entry.count && !valueReader.eof(); ++i) {
                entry.byteArray.push_back(
                    static_cast<uint8_t>(valueReader.readInt8()));
            }
            if (entry.count == 1) {
                entry.intValue = static_cast<int8_t>(entry.byteArray[0]);
            }
            break;

        case ExifDataType::SSHORT:
            for (uint32_t i = 0;
                 i < entry.count && valueReader.remaining() >= 2; ++i) {
                int16_t val = valueReader.readInt16();
                entry.shortArray.push_back(static_cast<uint16_t>(val));
            }
            if (entry.count == 1 && !entry.shortArray.empty()) {
                entry.intValue = static_cast<int16_t>(entry.shortArray[0]);
            }
            break;

        case ExifDataType::SLONG:
            for (uint32_t i = 0;
                 i < entry.count && valueReader.remaining() >= 4; ++i) {
                int32_t val = valueReader.readInt32();
                entry.longArray.push_back(static_cast<uint32_t>(val));
            }
            if (entry.count == 1 && !entry.longArray.empty()) {
                entry.intValue = static_cast<int32_t>(entry.longArray[0]);
            }
            break;

        case ExifDataType::SRATIONAL:
            for (uint32_t i = 0;
                 i < entry.count && valueReader.remaining() >= 8; ++i) {
                SRational sr = valueReader.readSRational();
                Rational r;
                r.numerator = static_cast<uint32_t>(std::abs(sr.numerator));
                r.denominator = static_cast<uint32_t>(std::abs(sr.denominator));
                entry.rationalArray.push_back(r);
            }
            if (entry.count == 1 && !entry.rationalArray.empty()) {
                entry.rationalValue = entry.rationalArray[0];
                entry.doubleValue = entry.rationalArray[0].toDouble();
            }
            break;

        case ExifDataType::FLOAT:
            if (entry.count >= 1 && valueReader.remaining() >= 4) {
                entry.doubleValue =
                    static_cast<double>(valueReader.readFloat());
            }
            break;

        case ExifDataType::DOUBLE:
            if (entry.count >= 1 && valueReader.remaining() >= 8) {
                entry.doubleValue = valueReader.readDouble();
            }
            break;

        default:
            break;
    }
}

size_t IfdParser::getValueSize(ExifDataType type, uint32_t count) const {
    return getExifDataTypeSize(type) * count;
}

// Free functions

std::optional<std::pair<ByteOrder, uint32_t>> parseTiffHeader(
    const std::byte* data, size_t size) {
    if (size < TIFF_HEADER_SIZE) {
        return std::nullopt;
    }

    // Check byte order marker
    uint16_t byteOrderMarker = ByteReader::readUint16Be(data);
    ByteOrder byteOrder;

    if (byteOrderMarker == TIFF_MAGIC_LE) {
        byteOrder = ByteOrder::LITTLE_ENDIAN;
    } else if (byteOrderMarker == TIFF_MAGIC_BE) {
        byteOrder = ByteOrder::BIG_ENDIAN;
    } else {
        return std::nullopt;
    }

    // Check TIFF magic number (42)
    uint16_t magic = (byteOrder == ByteOrder::LITTLE_ENDIAN)
                         ? ByteReader::readUint16Le(data + 2)
                         : ByteReader::readUint16Be(data + 2);

    if (magic != TIFF_MARKER) {
        return std::nullopt;
    }

    // Read first IFD offset
    uint32_t ifdOffset = (byteOrder == ByteOrder::LITTLE_ENDIAN)
                             ? ByteReader::readUint32Le(data + 4)
                             : ByteReader::readUint32Be(data + 4);

    return std::make_pair(byteOrder, ifdOffset);
}

std::optional<std::pair<size_t, size_t>> findExifInJpeg(const std::byte* data,
                                                        size_t size) {
    if (size < 4) {
        return std::nullopt;
    }

    // Check JPEG SOI marker
    if (data[0] != std::byte{0xFF} || data[1] != std::byte{0xD8}) {
        return std::nullopt;
    }

    size_t pos = 2;
    while (pos + 4 < size) {
        // Look for marker
        if (data[pos] != std::byte{0xFF}) {
            ++pos;
            continue;
        }

        uint8_t marker = std::to_integer<uint8_t>(data[pos + 1]);

        // Skip padding bytes
        if (marker == 0xFF) {
            ++pos;
            continue;
        }

        // Get segment length (big-endian)
        uint16_t segmentLength = ByteReader::readUint16Be(data + pos + 2);

        // Check for APP1 (EXIF) marker
        if (marker == 0xE1) {
            // Check for "Exif\0\0" header
            if (pos + 10 <= size &&
                std::memcmp(data + pos + 4, "Exif\0\0", 6) == 0) {
                // EXIF data starts after "Exif\0\0"
                size_t exifStart = pos + 10;
                size_t exifSize = segmentLength - 8;  // Subtract header size

                if (exifStart + exifSize <= size) {
                    return std::make_pair(exifStart, exifSize);
                }
            }
        }

        // Check for SOS (Start of Scan) - no more metadata after this
        if (marker == 0xDA) {
            break;
        }

        // Check for EOI (End of Image)
        if (marker == 0xD9) {
            break;
        }

        // Move to next segment
        pos += 2 + segmentLength;
    }

    return std::nullopt;
}

}  // namespace atom::image::metadata
