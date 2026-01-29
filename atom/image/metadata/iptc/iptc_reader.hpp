#ifndef ATOM_IMAGE_METADATA_IPTC_READER_HPP
#define ATOM_IMAGE_METADATA_IPTC_READER_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "../types/iptc_types.hpp"

namespace atom::image::metadata {

/**
 * @brief IPTC metadata reader
 *
 * Reads IPTC-IIM (Information Interchange Model) metadata from image files.
 * IPTC data is typically stored in JPEG APP13 segments.
 */
class IptcReader {
public:
    /**
     * @brief Default constructor
     */
    IptcReader() = default;

    /**
     * @brief Construct with file path
     */
    explicit IptcReader(const std::filesystem::path& path);

    /**
     * @brief Read IPTC data from file
     */
    [[nodiscard]] static std::optional<IptcData> fromFile(
        const std::filesystem::path& path);

    /**
     * @brief Read IPTC data from memory buffer
     */
    [[nodiscard]] static std::optional<IptcData> fromMemory(const void* data,
                                                            size_t size);

    /**
     * @brief Parse IPTC data
     */
    bool parse();

    /**
     * @brief Parse from memory buffer
     */
    bool parseFromMemory(const void* data, size_t size);

    /**
     * @brief Get parsed IPTC data
     */
    [[nodiscard]] const IptcData& getIptcData() const noexcept {
        return iptcData_;
    }

    /**
     * @brief Get mutable IPTC data
     */
    [[nodiscard]] IptcData& getIptcData() noexcept { return iptcData_; }

    /**
     * @brief Check if IPTC data was found
     */
    [[nodiscard]] bool hasIptcData() const noexcept {
        return iptcData_.hasData();
    }

    /**
     * @brief Get last error message
     */
    [[nodiscard]] const std::string& lastError() const noexcept {
        return lastError_;
    }

    /**
     * @brief Find IPTC data in JPEG APP13 segment
     * @return Offset and size of IPTC data, or nullopt if not found
     */
    [[nodiscard]] static std::optional<std::pair<size_t, size_t>>
    findIptcInJpeg(const uint8_t* data, size_t size);

    /**
     * @brief Parse raw IPTC-IIM data
     */
    [[nodiscard]] static std::optional<IptcData> parseIptcIim(
        const uint8_t* data, size_t size);

private:
    std::filesystem::path filepath_;
    std::vector<uint8_t> fileData_;
    IptcData iptcData_;
    std::string lastError_;

    /**
     * @brief Load file into memory
     */
    bool loadFile();

    /**
     * @brief Parse IPTC from JPEG data
     */
    bool parseJpeg();

    /**
     * @brief Parse single IPTC dataset
     */
    bool parseDataset(const uint8_t* data, size_t size, size_t& pos);

    /**
     * @brief Convert dataset to typed value
     */
    void processDataset(const IptcDataset& dataset);

    /**
     * @brief Parse IPTC date string
     */
    [[nodiscard]] static std::optional<std::chrono::system_clock::time_point>
    parseIptcDate(const std::string& dateStr, const std::string& timeStr = "");
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_IPTC_READER_HPP
