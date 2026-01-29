#ifndef ATOM_IMAGE_METADATA_IPTC_WRITER_HPP
#define ATOM_IMAGE_METADATA_IPTC_WRITER_HPP

#include <filesystem>
#include <string>
#include <vector>

#include "../types/iptc_types.hpp"

namespace atom::image::metadata {

/**
 * @brief IPTC metadata writer
 */
class IptcWriter {
public:
    IptcWriter() = default;
    explicit IptcWriter(const IptcData& data);

    void setIptcData(const IptcData& data) { iptcData_ = data; }
    [[nodiscard]] const IptcData& getIptcData() const noexcept {
        return iptcData_;
    }
    [[nodiscard]] IptcData& getIptcData() noexcept { return iptcData_; }

    bool writeToFile(const std::filesystem::path& inputFile,
                     const std::filesystem::path& outputFile);
    bool writeToFile(const std::filesystem::path& file);

    [[nodiscard]] std::vector<uint8_t> embedInJpeg(
        const std::vector<uint8_t>& imageData);
    [[nodiscard]] std::vector<uint8_t> generateIptcSegment();
    [[nodiscard]] static std::vector<uint8_t> stripIptc(
        const std::vector<uint8_t>& imageData);

    [[nodiscard]] const std::string& lastError() const noexcept {
        return lastError_;
    }

    // Convenience setters
    void setTitle(const std::string& title) { iptcData_.objectName = title; }
    void setHeadline(const std::string& headline) {
        iptcData_.headline = headline;
    }
    void setCaption(const std::string& caption) { iptcData_.caption = caption; }
    void setByline(const std::string& byline) { iptcData_.byline = byline; }
    void setCopyright(const std::string& copyright) {
        iptcData_.copyrightNotice = copyright;
    }
    void addKeyword(const std::string& keyword) {
        iptcData_.addKeyword(keyword);
    }
    void setCity(const std::string& city) { iptcData_.location.city = city; }
    void setCountry(const std::string& country) {
        iptcData_.location.country = country;
    }

private:
    IptcData iptcData_;
    std::string lastError_;

    [[nodiscard]] std::vector<uint8_t> buildIptcIim();
    void writeDataset(std::vector<uint8_t>& buffer, IptcRecord record,
                      uint8_t tag, const std::string& value);
    void writeDataset(std::vector<uint8_t>& buffer, IptcRecord record,
                      uint8_t tag, const std::vector<uint8_t>& value);
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_IPTC_WRITER_HPP
