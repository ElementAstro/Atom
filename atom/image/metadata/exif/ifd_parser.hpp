#ifndef ATOM_IMAGE_METADATA_IFD_PARSER_HPP
#define ATOM_IMAGE_METADATA_IFD_PARSER_HPP

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "../types/exif_types.hpp"
#include "../utils/byte_reader.hpp"
#include "exif_tags.hpp"

namespace atom::image::metadata {

/**
 * @brief Parsed IFD entry with raw and converted values
 */
struct ParsedIfdEntry {
    uint16_t tag = 0;
    ExifDataType type = ExifDataType::UNDEFINED;
    uint32_t count = 0;
    uint32_t valueOffset = 0;
    std::vector<std::byte> rawData;

    // Converted values (based on type)
    std::optional<std::string> stringValue;
    std::optional<double> doubleValue;
    std::optional<int64_t> intValue;
    std::optional<Rational> rationalValue;
    std::vector<Rational> rationalArray;
    std::vector<uint8_t> byteArray;
    std::vector<uint16_t> shortArray;
    std::vector<uint32_t> longArray;

    /**
     * @brief Get value as string (with conversion)
     */
    [[nodiscard]] std::string asString() const;

    /**
     * @brief Get value as double (for numeric types)
     */
    [[nodiscard]] std::optional<double> asDouble() const;

    /**
     * @brief Get value as integer (for numeric types)
     */
    [[nodiscard]] std::optional<int64_t> asInt() const;

    /**
     * @brief Check if entry has valid data
     */
    [[nodiscard]] bool isValid() const noexcept {
        return tag != 0 && count > 0;
    }
};

/**
 * @brief Result of parsing an IFD
 */
struct IfdParseResult {
    IfdType type = IfdType::IFD0;
    std::vector<ParsedIfdEntry> entries;
    uint32_t nextIfdOffset = 0;

    // Sub-IFD offsets found
    std::optional<uint32_t> exifIfdOffset;
    std::optional<uint32_t> gpsIfdOffset;
    std::optional<uint32_t> interopIfdOffset;

    // Thumbnail information (for IFD1)
    std::optional<uint32_t> thumbnailOffset;
    std::optional<uint32_t> thumbnailLength;

    /**
     * @brief Find entry by tag
     */
    [[nodiscard]] const ParsedIfdEntry* findEntry(uint16_t tag) const;

    /**
     * @brief Get entry value as string
     */
    [[nodiscard]] std::optional<std::string> getString(uint16_t tag) const;

    /**
     * @brief Get entry value as double
     */
    [[nodiscard]] std::optional<double> getDouble(uint16_t tag) const;

    /**
     * @brief Get entry value as integer
     */
    [[nodiscard]] std::optional<int64_t> getInt(uint16_t tag) const;

    /**
     * @brief Get entry value as rational
     */
    [[nodiscard]] std::optional<Rational> getRational(uint16_t tag) const;

    /**
     * @brief Get entry value as rational array
     */
    [[nodiscard]] std::vector<Rational> getRationalArray(uint16_t tag) const;

    /**
     * @brief Check if tag exists
     */
    [[nodiscard]] bool hasTag(uint16_t tag) const;
};

/**
 * @brief IFD (Image File Directory) parser
 *
 * Parses TIFF/EXIF IFD structures from binary data.
 */
class IfdParser {
public:
    /**
     * @brief Construct parser with TIFF data
     * @param tiffStart Pointer to start of TIFF header
     * @param tiffSize Size of TIFF data
     * @param byteOrder Byte order (from TIFF header)
     */
    IfdParser(const std::byte* tiffStart, size_t tiffSize, ByteOrder byteOrder);

    /**
     * @brief Construct from ByteReader
     */
    explicit IfdParser(ByteReader reader);

    /**
     * @brief Parse IFD at specific offset
     * @param offset Offset from TIFF start
     * @param ifdType Type of IFD being parsed
     * @return Parse result or nullopt on error
     */
    [[nodiscard]] std::optional<IfdParseResult> parseIfd(
        uint32_t offset, IfdType ifdType = IfdType::IFD0);

    /**
     * @brief Parse all IFDs in chain (IFD0 -> IFD1 -> ...)
     * @param firstIfdOffset Offset of first IFD
     * @return Vector of parse results
     */
    [[nodiscard]] std::vector<IfdParseResult> parseIfdChain(
        uint32_t firstIfdOffset);

    /**
     * @brief Parse EXIF sub-IFD
     * @param offset Offset from TIFF start
     * @return Parse result or nullopt on error
     */
    [[nodiscard]] std::optional<IfdParseResult> parseExifIfd(uint32_t offset);

    /**
     * @brief Parse GPS sub-IFD
     * @param offset Offset from TIFF start
     * @return Parse result or nullopt on error
     */
    [[nodiscard]] std::optional<IfdParseResult> parseGpsIfd(uint32_t offset);

    /**
     * @brief Parse Interoperability sub-IFD
     * @param offset Offset from TIFF start
     * @return Parse result or nullopt on error
     */
    [[nodiscard]] std::optional<IfdParseResult> parseInteropIfd(
        uint32_t offset);

    /**
     * @brief Get last error message
     */
    [[nodiscard]] const std::string& lastError() const noexcept {
        return lastError_;
    }

    /**
     * @brief Check if byte order is little endian
     */
    [[nodiscard]] bool isLittleEndian() const noexcept {
        return reader_.isLittleEndian();
    }

    /**
     * @brief Get byte reader for direct access
     */
    [[nodiscard]] const ByteReader& reader() const noexcept { return reader_; }

    /**
     * @brief Set maximum number of entries to parse (security limit)
     */
    void setMaxEntries(size_t max) noexcept { maxEntries_ = max; }

    /**
     * @brief Set maximum recursion depth for sub-IFDs
     */
    void setMaxDepth(int depth) noexcept { maxDepth_ = depth; }

private:
    ByteReader reader_;
    std::string lastError_;
    size_t maxEntries_ = 1000;
    int maxDepth_ = 10;
    int currentDepth_ = 0;

    /**
     * @brief Parse single IFD entry
     */
    [[nodiscard]] std::optional<ParsedIfdEntry> parseEntry(size_t entryOffset);

    /**
     * @brief Read entry value based on type and count
     */
    void readEntryValue(ParsedIfdEntry& entry, uint32_t valueOffset);

    /**
     * @brief Convert entry raw data to appropriate type
     */
    void convertEntryValue(ParsedIfdEntry& entry);

    /**
     * @brief Calculate total size of entry value
     */
    [[nodiscard]] size_t getValueSize(ExifDataType type, uint32_t count) const;

    /**
     * @brief Check if value fits in 4 bytes (inline in IFD entry)
     */
    [[nodiscard]] bool isValueInline(ExifDataType type, uint32_t count) const {
        return getValueSize(type, count) <= 4;
    }

    /**
     * @brief Validate offset is within bounds
     */
    [[nodiscard]] bool isValidOffset(uint32_t offset, size_t size = 1) const {
        return offset + size <= reader_.size();
    }
};

/**
 * @brief Parse TIFF header and determine byte order
 * @param data Pointer to TIFF data (at least 8 bytes)
 * @param size Size of data
 * @return Byte order and first IFD offset, or nullopt on error
 */
[[nodiscard]] std::optional<std::pair<ByteOrder, uint32_t>> parseTiffHeader(
    const std::byte* data, size_t size);

/**
 * @brief Detect EXIF marker in JPEG data
 * @param data JPEG file data
 * @param size Data size
 * @return Offset and size of EXIF data, or nullopt if not found
 */
[[nodiscard]] std::optional<std::pair<size_t, size_t>> findExifInJpeg(
    const std::byte* data, size_t size);

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_IFD_PARSER_HPP
