#ifndef ATOM_IMAGE_METADATA_XMP_PARSER_HPP
#define ATOM_IMAGE_METADATA_XMP_PARSER_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "../types/xmp_types.hpp"

namespace atom::image::metadata {

/**
 * @brief XMP metadata parser
 *
 * Parses XMP (Extensible Metadata Platform) data from image files.
 * XMP is typically stored as XML in JPEG APP1 segments.
 */
class XmpParser {
public:
    XmpParser() = default;
    explicit XmpParser(const std::filesystem::path& path);

    [[nodiscard]] static std::optional<XmpData> fromFile(
        const std::filesystem::path& path);
    [[nodiscard]] static std::optional<XmpData> fromMemory(const void* data,
                                                           size_t size);
    [[nodiscard]] static std::optional<XmpData> fromXmlString(
        const std::string& xml);

    bool parse();
    bool parseFromMemory(const void* data, size_t size);

    [[nodiscard]] const XmpData& getXmpData() const noexcept {
        return xmpData_;
    }
    [[nodiscard]] XmpData& getXmpData() noexcept { return xmpData_; }
    [[nodiscard]] bool hasXmpData() const noexcept {
        return xmpData_.hasData();
    }
    [[nodiscard]] const std::string& lastError() const noexcept {
        return lastError_;
    }

    [[nodiscard]] static std::optional<std::pair<size_t, size_t>> findXmpInJpeg(
        const uint8_t* data, size_t size);

private:
    std::filesystem::path filepath_;
    std::vector<uint8_t> fileData_;
    XmpData xmpData_;
    std::string lastError_;

    bool loadFile();
    bool parseJpeg();
    bool parseXml(const std::string& xml);
    void parseRdfDescription(const std::string& xml, size_t start, size_t end);
    void parseDublinCore(const std::string& xml);
    void parseXmpBasic(const std::string& xml);
    void parsePhotoshop(const std::string& xml);

    [[nodiscard]] static std::string extractTagContent(const std::string& xml,
                                                       const std::string& tag);
    [[nodiscard]] static std::vector<std::string> extractArrayItems(
        const std::string& xml, const std::string& tag);
    [[nodiscard]] static std::vector<XmpLangAlt> extractLangAlt(
        const std::string& xml, const std::string& tag);
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_XMP_PARSER_HPP
