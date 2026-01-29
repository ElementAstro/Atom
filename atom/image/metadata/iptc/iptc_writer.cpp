#include "iptc_writer.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace atom::image::metadata {

namespace {
constexpr uint8_t IPTC_MARKER = 0x1C;
constexpr char PHOTOSHOP_HEADER[] = "Photoshop 3.0\x00";
constexpr char IPTC_RESOURCE_ID[] = "8BIM";
constexpr uint16_t IPTC_RESOURCE_TYPE = 0x0404;
}  // namespace

IptcWriter::IptcWriter(const IptcData& data) : iptcData_(data) {}

bool IptcWriter::writeToFile(const std::filesystem::path& inputFile,
                             const std::filesystem::path& outputFile) {
    std::ifstream inFile(inputFile, std::ios::binary);
    if (!inFile.is_open()) {
        lastError_ = "Cannot open input file";
        return false;
    }

    std::vector<uint8_t> imageData((std::istreambuf_iterator<char>(inFile)),
                                   std::istreambuf_iterator<char>());
    inFile.close();

    auto modifiedData = embedInJpeg(imageData);
    if (modifiedData.empty())
        return false;

    std::ofstream outFile(outputFile, std::ios::binary);
    if (!outFile.is_open()) {
        lastError_ = "Cannot open output file";
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(modifiedData.data()),
                  static_cast<std::streamsize>(modifiedData.size()));
    return true;
}

bool IptcWriter::writeToFile(const std::filesystem::path& file) {
    auto tempFile = file;
    tempFile += ".tmp";
    if (!writeToFile(file, tempFile))
        return false;

    std::error_code ec;
    std::filesystem::remove(file, ec);
    std::filesystem::rename(tempFile, file, ec);
    return !ec;
}

std::vector<uint8_t> IptcWriter::embedInJpeg(
    const std::vector<uint8_t>& imageData) {
    if (imageData.size() < 4 || imageData[0] != 0xFF || imageData[1] != 0xD8) {
        lastError_ = "Not a valid JPEG";
        return {};
    }

    auto iptcSegment = generateIptcSegment();
    if (iptcSegment.empty())
        return {};

    // Find insertion point and skip existing APP13
    size_t insertPos = 2;
    size_t skipBytes = 0;
    size_t pos = 2;

    while (pos + 4 < imageData.size()) {
        if (imageData[pos] != 0xFF) {
            ++pos;
            continue;
        }
        uint8_t marker = imageData[pos + 1];
        if (marker == 0xFF) {
            ++pos;
            continue;
        }

        uint16_t segLen = (static_cast<uint16_t>(imageData[pos + 2]) << 8) |
                          imageData[pos + 3];

        if (marker == 0xED) {  // APP13
            insertPos = pos;
            skipBytes = 2 + segLen;
            break;
        }
        if (marker == 0xDA) {
            insertPos = pos;
            break;
        }
        if (marker == 0xE0 || marker == 0xE1) {
            insertPos = pos + 2 + segLen;
        }
        pos += 2 + segLen;
    }

    std::vector<uint8_t> result;
    result.reserve(imageData.size() + iptcSegment.size());
    result.insert(result.end(), imageData.begin(),
                  imageData.begin() + static_cast<ptrdiff_t>(insertPos));
    result.insert(result.end(), iptcSegment.begin(), iptcSegment.end());
    result.insert(
        result.end(),
        imageData.begin() + static_cast<ptrdiff_t>(insertPos + skipBytes),
        imageData.end());
    return result;
}

std::vector<uint8_t> IptcWriter::generateIptcSegment() {
    auto iptcIim = buildIptcIim();
    if (iptcIim.empty())
        return {};

    std::vector<uint8_t> result;

    // APP13 marker
    result.push_back(0xFF);
    result.push_back(0xED);

    // Build Photoshop resource block
    std::vector<uint8_t> resourceBlock;

    // Photoshop header
    for (size_t i = 0; i < sizeof(PHOTOSHOP_HEADER) - 1; ++i) {
        resourceBlock.push_back(static_cast<uint8_t>(PHOTOSHOP_HEADER[i]));
    }

    // 8BIM resource
    for (int i = 0; i < 4; ++i) {
        resourceBlock.push_back(static_cast<uint8_t>(IPTC_RESOURCE_ID[i]));
    }

    // Resource type (0x0404 = IPTC-NAA)
    resourceBlock.push_back(
        static_cast<uint8_t>((IPTC_RESOURCE_TYPE >> 8) & 0xFF));
    resourceBlock.push_back(static_cast<uint8_t>(IPTC_RESOURCE_TYPE & 0xFF));

    // Pascal string (empty name)
    resourceBlock.push_back(0x00);
    resourceBlock.push_back(0x00);  // Pad to even

    // Data size (big-endian 32-bit)
    uint32_t dataSize = static_cast<uint32_t>(iptcIim.size());
    resourceBlock.push_back(static_cast<uint8_t>((dataSize >> 24) & 0xFF));
    resourceBlock.push_back(static_cast<uint8_t>((dataSize >> 16) & 0xFF));
    resourceBlock.push_back(static_cast<uint8_t>((dataSize >> 8) & 0xFF));
    resourceBlock.push_back(static_cast<uint8_t>(dataSize & 0xFF));

    // IPTC data
    resourceBlock.insert(resourceBlock.end(), iptcIim.begin(), iptcIim.end());

    // Pad to even length
    if (resourceBlock.size() % 2 != 0) {
        resourceBlock.push_back(0x00);
    }

    // Segment length
    uint16_t segLen = static_cast<uint16_t>(resourceBlock.size() + 2);
    result.push_back(static_cast<uint8_t>((segLen >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(segLen & 0xFF));

    result.insert(result.end(), resourceBlock.begin(), resourceBlock.end());
    return result;
}

std::vector<uint8_t> IptcWriter::stripIptc(
    const std::vector<uint8_t>& imageData) {
    if (imageData.size() < 4 || imageData[0] != 0xFF || imageData[1] != 0xD8) {
        return imageData;
    }

    std::vector<uint8_t> result;
    result.reserve(imageData.size());
    result.push_back(imageData[0]);
    result.push_back(imageData[1]);

    size_t pos = 2;
    while (pos + 4 < imageData.size()) {
        if (imageData[pos] != 0xFF) {
            result.push_back(imageData[pos++]);
            continue;
        }

        uint8_t marker = imageData[pos + 1];
        if (marker == 0xFF) {
            result.push_back(imageData[pos++]);
            continue;
        }

        uint16_t segLen = (static_cast<uint16_t>(imageData[pos + 2]) << 8) |
                          imageData[pos + 3];

        if (marker == 0xED) {  // Skip APP13
            pos += 2 + segLen;
            continue;
        }

        for (size_t i = 0; i < 2 + segLen && pos + i < imageData.size(); ++i) {
            result.push_back(imageData[pos + i]);
        }

        if (marker == 0xDA) {
            pos += 2 + segLen;
            while (pos < imageData.size())
                result.push_back(imageData[pos++]);
            break;
        }
        pos += 2 + segLen;
    }

    return result;
}

std::vector<uint8_t> IptcWriter::buildIptcIim() {
    std::vector<uint8_t> buffer;

    // Record version (always first)
    std::vector<uint8_t> version = {0x00, 0x04};
    writeDataset(buffer, IptcRecord::APPLICATION,
                 static_cast<uint8_t>(IptcTag::RECORD_VERSION), version);

    if (iptcData_.objectName) {
        writeDataset(buffer, IptcRecord::APPLICATION,
                     static_cast<uint8_t>(IptcTag::OBJECT_NAME),
                     *iptcData_.objectName);
    }
    if (iptcData_.headline) {
        writeDataset(buffer, IptcRecord::APPLICATION,
                     static_cast<uint8_t>(IptcTag::HEADLINE),
                     *iptcData_.headline);
    }
    if (iptcData_.caption) {
        writeDataset(buffer, IptcRecord::APPLICATION,
                     static_cast<uint8_t>(IptcTag::CAPTION),
                     *iptcData_.caption);
    }
    for (const auto& keyword : iptcData_.keywords) {
        writeDataset(buffer, IptcRecord::APPLICATION,
                     static_cast<uint8_t>(IptcTag::KEYWORDS), keyword);
    }
    if (iptcData_.byline) {
        writeDataset(buffer, IptcRecord::APPLICATION,
                     static_cast<uint8_t>(IptcTag::BYLINE), *iptcData_.byline);
    }
    if (iptcData_.bylineTitle) {
        writeDataset(buffer, IptcRecord::APPLICATION,
                     static_cast<uint8_t>(IptcTag::BYLINE_TITLE),
                     *iptcData_.bylineTitle);
    }
    if (iptcData_.credit) {
        writeDataset(buffer, IptcRecord::APPLICATION,
                     static_cast<uint8_t>(IptcTag::CREDIT), *iptcData_.credit);
    }
    if (iptcData_.source) {
        writeDataset(buffer, IptcRecord::APPLICATION,
                     static_cast<uint8_t>(IptcTag::SOURCE), *iptcData_.source);
    }
    if (iptcData_.copyrightNotice) {
        writeDataset(buffer, IptcRecord::APPLICATION,
                     static_cast<uint8_t>(IptcTag::COPYRIGHT_NOTICE),
                     *iptcData_.copyrightNotice);
    }
    if (iptcData_.location.city) {
        writeDataset(buffer, IptcRecord::APPLICATION,
                     static_cast<uint8_t>(IptcTag::CITY),
                     *iptcData_.location.city);
    }
    if (iptcData_.location.state) {
        writeDataset(buffer, IptcRecord::APPLICATION,
                     static_cast<uint8_t>(IptcTag::STATE),
                     *iptcData_.location.state);
    }
    if (iptcData_.location.country) {
        writeDataset(buffer, IptcRecord::APPLICATION,
                     static_cast<uint8_t>(IptcTag::COUNTRY),
                     *iptcData_.location.country);
    }
    if (iptcData_.location.countryCode) {
        writeDataset(buffer, IptcRecord::APPLICATION,
                     static_cast<uint8_t>(IptcTag::COUNTRY_CODE),
                     *iptcData_.location.countryCode);
    }

    return buffer;
}

void IptcWriter::writeDataset(std::vector<uint8_t>& buffer, IptcRecord record,
                              uint8_t tag, const std::string& value) {
    writeDataset(buffer, record, tag,
                 std::vector<uint8_t>(value.begin(), value.end()));
}

void IptcWriter::writeDataset(std::vector<uint8_t>& buffer, IptcRecord record,
                              uint8_t tag, const std::vector<uint8_t>& value) {
    buffer.push_back(IPTC_MARKER);
    buffer.push_back(static_cast<uint8_t>(record));
    buffer.push_back(tag);
    buffer.push_back(static_cast<uint8_t>((value.size() >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value.size() & 0xFF));
    buffer.insert(buffer.end(), value.begin(), value.end());
}

}  // namespace atom::image::metadata
