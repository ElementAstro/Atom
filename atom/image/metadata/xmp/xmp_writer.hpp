#ifndef ATOM_IMAGE_METADATA_XMP_WRITER_HPP
#define ATOM_IMAGE_METADATA_XMP_WRITER_HPP

#include <filesystem>
#include <string>
#include <vector>

#include "../types/xmp_types.hpp"

namespace atom::image::metadata {

/**
 * @brief XMP metadata writer
 */
class XmpWriter {
public:
    XmpWriter() = default;
    explicit XmpWriter(const XmpData& data);

    void setXmpData(const XmpData& data) { xmpData_ = data; }
    [[nodiscard]] const XmpData& getXmpData() const noexcept {
        return xmpData_;
    }
    [[nodiscard]] XmpData& getXmpData() noexcept { return xmpData_; }

    bool writeToFile(const std::filesystem::path& inputFile,
                     const std::filesystem::path& outputFile);
    bool writeToFile(const std::filesystem::path& file);

    [[nodiscard]] std::vector<uint8_t> embedInJpeg(
        const std::vector<uint8_t>& imageData);
    [[nodiscard]] std::string generateXmpPacket();
    [[nodiscard]] std::vector<uint8_t> generateApp1Segment();
    [[nodiscard]] static std::vector<uint8_t> stripXmp(
        const std::vector<uint8_t>& imageData);

    [[nodiscard]] const std::string& lastError() const noexcept {
        return lastError_;
    }

    // Convenience setters
    void setTitle(const std::string& title,
                  const std::string& lang = "x-default");
    void setDescription(const std::string& desc,
                        const std::string& lang = "x-default");
    void setCreator(const std::string& creator);
    void addKeyword(const std::string& keyword);
    void setRating(int rating);
    void setCopyright(const std::string& copyright,
                      const std::string& lang = "x-default");

private:
    XmpData xmpData_;
    std::string lastError_;

    void writeXmlHeader(std::ostringstream& oss);
    void writeRdfOpen(std::ostringstream& oss);
    void writeRdfClose(std::ostringstream& oss);
    void writeDublinCore(std::ostringstream& oss);
    void writeXmpBasic(std::ostringstream& oss);
    void writePhotoshop(std::ostringstream& oss);
    void writeLangAlt(std::ostringstream& oss, const std::string& tag,
                      const std::vector<XmpLangAlt>& values);
    void writeSeq(std::ostringstream& oss, const std::string& tag,
                  const std::vector<std::string>& values);
    void writeBag(std::ostringstream& oss, const std::string& tag,
                  const std::vector<std::string>& values);
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_XMP_WRITER_HPP
