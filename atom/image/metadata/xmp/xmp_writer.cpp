#include "xmp_writer.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "../utils/datetime_utils.hpp"

namespace atom::image::metadata {

namespace {
constexpr char XMP_HEADER[] = "http://ns.adobe.com/xap/1.0/";
}

XmpWriter::XmpWriter(const XmpData& data) : xmpData_(data) {}

bool XmpWriter::writeToFile(const std::filesystem::path& inputFile,
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

bool XmpWriter::writeToFile(const std::filesystem::path& file) {
    auto tempFile = file;
    tempFile += ".tmp";
    if (!writeToFile(file, tempFile))
        return false;
    std::error_code ec;
    std::filesystem::remove(file, ec);
    std::filesystem::rename(tempFile, file, ec);
    return !ec;
}

std::vector<uint8_t> XmpWriter::embedInJpeg(
    const std::vector<uint8_t>& imageData) {
    if (imageData.size() < 4 || imageData[0] != 0xFF || imageData[1] != 0xD8) {
        lastError_ = "Not a valid JPEG";
        return {};
    }

    auto xmpSegment = generateApp1Segment();
    if (xmpSegment.empty())
        return {};

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

        // Check for existing XMP APP1
        if (marker == 0xE1 && pos + 4 + sizeof(XMP_HEADER) < imageData.size()) {
            if (std::memcmp(imageData.data() + pos + 4, XMP_HEADER,
                            sizeof(XMP_HEADER) - 1) == 0) {
                insertPos = pos;
                skipBytes = 2 + segLen;
                break;
            }
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
    result.reserve(imageData.size() + xmpSegment.size());
    result.insert(result.end(), imageData.begin(),
                  imageData.begin() + static_cast<ptrdiff_t>(insertPos));
    result.insert(result.end(), xmpSegment.begin(), xmpSegment.end());
    result.insert(
        result.end(),
        imageData.begin() + static_cast<ptrdiff_t>(insertPos + skipBytes),
        imageData.end());
    return result;
}

std::string XmpWriter::generateXmpPacket() {
    std::ostringstream oss;

    writeXmlHeader(oss);
    writeRdfOpen(oss);

    // Write namespaces in description
    oss << "  <rdf:Description rdf:about=\"\"\n";
    oss << "    xmlns:dc=\"" << XmpNamespace::DC << "\"\n";
    oss << "    xmlns:xmp=\"" << XmpNamespace::XMP << "\"\n";
    oss << "    xmlns:xmpRights=\"" << XmpNamespace::XMP_RIGHTS << "\"\n";
    oss << "    xmlns:photoshop=\"" << XmpNamespace::PHOTOSHOP << "\">\n";

    writeDublinCore(oss);
    writeXmpBasic(oss);
    writePhotoshop(oss);

    oss << "  </rdf:Description>\n";

    writeRdfClose(oss);

    return oss.str();
}

std::vector<uint8_t> XmpWriter::generateApp1Segment() {
    std::string xmpPacket = generateXmpPacket();
    if (xmpPacket.empty())
        return {};

    std::vector<uint8_t> result;

    // APP1 marker
    result.push_back(0xFF);
    result.push_back(0xE1);

    // Build segment content: XMP header + null + packet
    std::vector<uint8_t> content;
    for (char c : std::string(XMP_HEADER)) {
        content.push_back(static_cast<uint8_t>(c));
    }
    content.push_back(0x00);  // Null terminator
    for (char c : xmpPacket) {
        content.push_back(static_cast<uint8_t>(c));
    }

    // Segment length (big-endian)
    uint16_t segLen = static_cast<uint16_t>(content.size() + 2);
    result.push_back(static_cast<uint8_t>((segLen >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(segLen & 0xFF));

    result.insert(result.end(), content.begin(), content.end());
    return result;
}

std::vector<uint8_t> XmpWriter::stripXmp(
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

        // Skip XMP APP1
        if (marker == 0xE1 && pos + 4 + sizeof(XMP_HEADER) < imageData.size() &&
            std::memcmp(imageData.data() + pos + 4, XMP_HEADER,
                        sizeof(XMP_HEADER) - 1) == 0) {
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

void XmpWriter::setTitle(const std::string& title, const std::string& lang) {
    XmpLangAlt item{lang, title};
    xmpData_.dc.title.clear();
    xmpData_.dc.title.push_back(item);
}

void XmpWriter::setDescription(const std::string& desc,
                               const std::string& lang) {
    XmpLangAlt item{lang, desc};
    xmpData_.dc.description.clear();
    xmpData_.dc.description.push_back(item);
}

void XmpWriter::setCreator(const std::string& creator) {
    xmpData_.dc.creator.clear();
    xmpData_.dc.creator.push_back(creator);
}

void XmpWriter::addKeyword(const std::string& keyword) {
    if (std::find(xmpData_.dc.subject.begin(), xmpData_.dc.subject.end(),
                  keyword) == xmpData_.dc.subject.end()) {
        xmpData_.dc.subject.push_back(keyword);
    }
}

void XmpWriter::setRating(int rating) {
    xmpData_.xmpBasic.rating = std::clamp(rating, -1, 5);
}

void XmpWriter::setCopyright(const std::string& copyright,
                             const std::string& lang) {
    XmpLangAlt item{lang, copyright};
    xmpData_.dc.rights.clear();
    xmpData_.dc.rights.push_back(item);
}

void XmpWriter::writeXmlHeader(std::ostringstream& oss) {
    oss << "<?xpacket begin=\"\xEF\xBB\xBF\" "
           "id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n";
    oss << "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">\n";
}

void XmpWriter::writeRdfOpen(std::ostringstream& oss) {
    oss << "<rdf:RDF xmlns:rdf=\"" << XmpNamespace::RDF << "\">\n";
}

void XmpWriter::writeRdfClose(std::ostringstream& oss) {
    oss << "</rdf:RDF>\n";
    oss << "</x:xmpmeta>\n";
    oss << "<?xpacket end=\"w\"?>";
}

void XmpWriter::writeDublinCore(std::ostringstream& oss) {
    if (!xmpData_.dc.title.empty()) {
        writeLangAlt(oss, "dc:title", xmpData_.dc.title);
    }
    if (!xmpData_.dc.description.empty()) {
        writeLangAlt(oss, "dc:description", xmpData_.dc.description);
    }
    if (!xmpData_.dc.rights.empty()) {
        writeLangAlt(oss, "dc:rights", xmpData_.dc.rights);
    }
    if (!xmpData_.dc.creator.empty()) {
        writeSeq(oss, "dc:creator", xmpData_.dc.creator);
    }
    if (!xmpData_.dc.subject.empty()) {
        writeBag(oss, "dc:subject", xmpData_.dc.subject);
    }
    if (xmpData_.dc.format) {
        oss << "    <dc:format>" << *xmpData_.dc.format << "</dc:format>\n";
    }
}

void XmpWriter::writeXmpBasic(std::ostringstream& oss) {
    if (xmpData_.xmpBasic.rating) {
        oss << "    <xmp:Rating>" << *xmpData_.xmpBasic.rating
            << "</xmp:Rating>\n";
    }
    if (xmpData_.xmpBasic.creatorTool) {
        oss << "    <xmp:CreatorTool>" << *xmpData_.xmpBasic.creatorTool
            << "</xmp:CreatorTool>\n";
    }
    if (xmpData_.xmpBasic.createDate) {
        oss << "    <xmp:CreateDate>"
            << DateTimeUtils::formatIso8601(*xmpData_.xmpBasic.createDate)
            << "</xmp:CreateDate>\n";
    }
    if (xmpData_.xmpBasic.modifyDate) {
        oss << "    <xmp:ModifyDate>"
            << DateTimeUtils::formatIso8601(*xmpData_.xmpBasic.modifyDate)
            << "</xmp:ModifyDate>\n";
    }
    if (xmpData_.xmpBasic.label) {
        oss << "    <xmp:Label>" << *xmpData_.xmpBasic.label
            << "</xmp:Label>\n";
    }
}

void XmpWriter::writePhotoshop(std::ostringstream& oss) {
    if (xmpData_.photoshop.headline) {
        oss << "    <photoshop:Headline>" << *xmpData_.photoshop.headline
            << "</photoshop:Headline>\n";
    }
    if (xmpData_.photoshop.city) {
        oss << "    <photoshop:City>" << *xmpData_.photoshop.city
            << "</photoshop:City>\n";
    }
    if (xmpData_.photoshop.state) {
        oss << "    <photoshop:State>" << *xmpData_.photoshop.state
            << "</photoshop:State>\n";
    }
    if (xmpData_.photoshop.country) {
        oss << "    <photoshop:Country>" << *xmpData_.photoshop.country
            << "</photoshop:Country>\n";
    }
    if (xmpData_.photoshop.credit) {
        oss << "    <photoshop:Credit>" << *xmpData_.photoshop.credit
            << "</photoshop:Credit>\n";
    }
    if (xmpData_.photoshop.source) {
        oss << "    <photoshop:Source>" << *xmpData_.photoshop.source
            << "</photoshop:Source>\n";
    }
}

void XmpWriter::writeLangAlt(std::ostringstream& oss, const std::string& tag,
                             const std::vector<XmpLangAlt>& values) {
    if (values.empty())
        return;
    oss << "    <" << tag << ">\n";
    oss << "      <rdf:Alt>\n";
    for (const auto& item : values) {
        oss << "        <rdf:li xml:lang=\"" << item.language << "\">"
            << item.value << "</rdf:li>\n";
    }
    oss << "      </rdf:Alt>\n";
    oss << "    </" << tag << ">\n";
}

void XmpWriter::writeSeq(std::ostringstream& oss, const std::string& tag,
                         const std::vector<std::string>& values) {
    if (values.empty())
        return;
    oss << "    <" << tag << ">\n";
    oss << "      <rdf:Seq>\n";
    for (const auto& item : values) {
        oss << "        <rdf:li>" << item << "</rdf:li>\n";
    }
    oss << "      </rdf:Seq>\n";
    oss << "    </" << tag << ">\n";
}

void XmpWriter::writeBag(std::ostringstream& oss, const std::string& tag,
                         const std::vector<std::string>& values) {
    if (values.empty())
        return;
    oss << "    <" << tag << ">\n";
    oss << "      <rdf:Bag>\n";
    for (const auto& item : values) {
        oss << "        <rdf:li>" << item << "</rdf:li>\n";
    }
    oss << "      </rdf:Bag>\n";
    oss << "    </" << tag << ">\n";
}

}  // namespace atom::image::metadata
