#include "xmp_parser.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <regex>

#include "../utils/datetime_utils.hpp"

namespace atom::image::metadata {

namespace {
constexpr size_t MAX_FILE_SIZE = 100 * 1024 * 1024;
constexpr char XMP_HEADER[] = "http://ns.adobe.com/xap/1.0/";
}  // namespace

XmpParser::XmpParser(const std::filesystem::path& path) : filepath_(path) {}

std::optional<XmpData> XmpParser::fromFile(const std::filesystem::path& path) {
    XmpParser parser(path);
    if (parser.parse())
        return parser.getXmpData();
    return std::nullopt;
}

std::optional<XmpData> XmpParser::fromMemory(const void* data, size_t size) {
    XmpParser parser;
    if (parser.parseFromMemory(data, size))
        return parser.getXmpData();
    return std::nullopt;
}

std::optional<XmpData> XmpParser::fromXmlString(const std::string& xml) {
    XmpParser parser;
    if (parser.parseXml(xml))
        return parser.getXmpData();
    return std::nullopt;
}

bool XmpParser::parse() {
    if (filepath_.empty()) {
        lastError_ = "No file path specified";
        return false;
    }
    if (!loadFile())
        return false;
    return parseJpeg();
}

bool XmpParser::parseFromMemory(const void* data, size_t size) {
    if (!data || size == 0) {
        lastError_ = "Invalid data";
        return false;
    }
    fileData_.resize(size);
    std::memcpy(fileData_.data(), data, size);
    return parseJpeg();
}

bool XmpParser::loadFile() {
    std::ifstream file(filepath_, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        lastError_ = "Cannot open file";
        return false;
    }
    auto fileSize = file.tellg();
    if (fileSize <= 0 || static_cast<size_t>(fileSize) > MAX_FILE_SIZE) {
        lastError_ = "Invalid file size";
        return false;
    }
    file.seekg(0);
    fileData_.resize(static_cast<size_t>(fileSize));
    file.read(reinterpret_cast<char*>(fileData_.data()), fileSize);
    return true;
}

bool XmpParser::parseJpeg() {
    xmpData_ = XmpData{};
    auto xmpLocation = findXmpInJpeg(fileData_.data(), fileData_.size());
    if (!xmpLocation)
        return true;  // No XMP, not an error

    auto [offset, size] = *xmpLocation;
    std::string xml(reinterpret_cast<const char*>(fileData_.data() + offset),
                    size);
    xmpData_.rawPacket = xml;
    return parseXml(xml);
}

std::optional<std::pair<size_t, size_t>> XmpParser::findXmpInJpeg(
    const uint8_t* data, size_t size) {
    if (size < 4 || data[0] != 0xFF || data[1] != 0xD8)
        return std::nullopt;

    size_t pos = 2;
    while (pos + 4 < size) {
        if (data[pos] != 0xFF) {
            ++pos;
            continue;
        }
        uint8_t marker = data[pos + 1];
        if (marker == 0xFF) {
            ++pos;
            continue;
        }

        uint16_t segLen =
            (static_cast<uint16_t>(data[pos + 2]) << 8) | data[pos + 3];

        if (marker == 0xE1 && pos + 4 + 29 < size) {
            // Check for XMP header
            if (std::memcmp(data + pos + 4, XMP_HEADER,
                            sizeof(XMP_HEADER) - 1) == 0) {
                size_t xmpStart = pos + 4 + sizeof(XMP_HEADER);
                size_t xmpSize = segLen - 2 - sizeof(XMP_HEADER);
                if (xmpStart + xmpSize <= size) {
                    return std::make_pair(xmpStart, xmpSize);
                }
            }
        }
        if (marker == 0xDA)
            break;
        pos += 2 + segLen;
    }
    return std::nullopt;
}

bool XmpParser::parseXml(const std::string& xml) {
    // Simple XML parsing - look for common XMP elements
    parseDublinCore(xml);
    parseXmpBasic(xml);
    parsePhotoshop(xml);

    // Extract rating
    std::string ratingStr = extractTagContent(xml, "xmp:Rating");
    if (!ratingStr.empty()) {
        try {
            xmpData_.xmpBasic.rating = std::stoi(ratingStr);
        } catch (...) {
        }
    }

    // Extract creator tool
    xmpData_.xmpBasic.creatorTool = extractTagContent(xml, "xmp:CreatorTool");
    if (xmpData_.xmpBasic.creatorTool->empty()) {
        xmpData_.xmpBasic.creatorTool.reset();
    }

    // Store raw properties from rdf:Description
    size_t descStart = xml.find("<rdf:Description");
    while (descStart != std::string::npos) {
        size_t descEnd = xml.find("</rdf:Description>", descStart);
        if (descEnd == std::string::npos) {
            descEnd = xml.find("/>", descStart);
            if (descEnd != std::string::npos)
                descEnd += 2;
        } else {
            descEnd += 18;
        }
        if (descEnd != std::string::npos) {
            parseRdfDescription(xml, descStart, descEnd);
        }
        descStart = xml.find("<rdf:Description", descEnd);
    }

    return true;
}

void XmpParser::parseRdfDescription(const std::string& xml, size_t start,
                                    size_t end) {
    std::string desc = xml.substr(start, end - start);

    // Extract namespace declarations and simple attributes
    std::regex attrRegex(R"((\w+):(\w+)=\"([^\"]*)\")");
    std::sregex_iterator it(desc.begin(), desc.end(), attrRegex);
    std::sregex_iterator itEnd;

    for (; it != itEnd; ++it) {
        std::smatch match = *it;
        if (match.size() == 4) {
            XmpProperty prop;
            prop.prefix = match[1].str();
            prop.name = match[2].str();
            prop.type = XmpValueType::SIMPLE;
            prop.value = match[3].str();
            xmpData_.properties.push_back(prop);
        }
    }
}

void XmpParser::parseDublinCore(const std::string& xml) {
    // Title
    xmpData_.dc.title = extractLangAlt(xml, "dc:title");

    // Description
    xmpData_.dc.description = extractLangAlt(xml, "dc:description");

    // Rights
    xmpData_.dc.rights = extractLangAlt(xml, "dc:rights");

    // Creator
    xmpData_.dc.creator = extractArrayItems(xml, "dc:creator");

    // Subject (keywords)
    xmpData_.dc.subject = extractArrayItems(xml, "dc:subject");

    // Publisher
    xmpData_.dc.publisher = extractArrayItems(xml, "dc:publisher");

    // Format
    std::string format = extractTagContent(xml, "dc:format");
    if (!format.empty())
        xmpData_.dc.format = format;

    // Identifier
    std::string identifier = extractTagContent(xml, "dc:identifier");
    if (!identifier.empty())
        xmpData_.dc.identifier = identifier;
}

void XmpParser::parseXmpBasic(const std::string& xml) {
    // CreateDate
    std::string createDate = extractTagContent(xml, "xmp:CreateDate");
    if (!createDate.empty()) {
        xmpData_.xmpBasic.createDate = DateTimeUtils::parseIso8601(createDate);
    }

    // ModifyDate
    std::string modifyDate = extractTagContent(xml, "xmp:ModifyDate");
    if (!modifyDate.empty()) {
        xmpData_.xmpBasic.modifyDate = DateTimeUtils::parseIso8601(modifyDate);
    }

    // MetadataDate
    std::string metadataDate = extractTagContent(xml, "xmp:MetadataDate");
    if (!metadataDate.empty()) {
        xmpData_.xmpBasic.metadataDate =
            DateTimeUtils::parseIso8601(metadataDate);
    }

    // Label
    std::string label = extractTagContent(xml, "xmp:Label");
    if (!label.empty())
        xmpData_.xmpBasic.label = label;

    // Nickname
    std::string nickname = extractTagContent(xml, "xmp:Nickname");
    if (!nickname.empty())
        xmpData_.xmpBasic.nickname = nickname;
}

void XmpParser::parsePhotoshop(const std::string& xml) {
    // City
    std::string city = extractTagContent(xml, "photoshop:City");
    if (!city.empty())
        xmpData_.photoshop.city = city;

    // State
    std::string state = extractTagContent(xml, "photoshop:State");
    if (!state.empty())
        xmpData_.photoshop.state = state;

    // Country
    std::string country = extractTagContent(xml, "photoshop:Country");
    if (!country.empty())
        xmpData_.photoshop.country = country;

    // Headline
    std::string headline = extractTagContent(xml, "photoshop:Headline");
    if (!headline.empty())
        xmpData_.photoshop.headline = headline;

    // Credit
    std::string credit = extractTagContent(xml, "photoshop:Credit");
    if (!credit.empty())
        xmpData_.photoshop.credit = credit;

    // Source
    std::string source = extractTagContent(xml, "photoshop:Source");
    if (!source.empty())
        xmpData_.photoshop.source = source;

    // Instructions
    std::string instructions = extractTagContent(xml, "photoshop:Instructions");
    if (!instructions.empty())
        xmpData_.photoshop.instructions = instructions;

    // DateCreated
    std::string dateCreated = extractTagContent(xml, "photoshop:DateCreated");
    if (!dateCreated.empty()) {
        xmpData_.photoshop.dateCreated =
            DateTimeUtils::parseIso8601(dateCreated);
    }
}

std::string XmpParser::extractTagContent(const std::string& xml,
                                         const std::string& tag) {
    std::string openTag = "<" + tag + ">";
    std::string closeTag = "</" + tag + ">";

    size_t start = xml.find(openTag);
    if (start == std::string::npos) {
        // Try attribute form
        std::string attrPattern = tag + "=\"";
        start = xml.find(attrPattern);
        if (start != std::string::npos) {
            start += attrPattern.length();
            size_t end = xml.find('"', start);
            if (end != std::string::npos) {
                return xml.substr(start, end - start);
            }
        }
        return "";
    }

    start += openTag.length();
    size_t end = xml.find(closeTag, start);
    if (end == std::string::npos)
        return "";

    return xml.substr(start, end - start);
}

std::vector<std::string> XmpParser::extractArrayItems(const std::string& xml,
                                                      const std::string& tag) {
    std::vector<std::string> result;

    std::string openTag = "<" + tag + ">";
    std::string closeTag = "</" + tag + ">";

    size_t tagStart = xml.find(openTag);
    if (tagStart == std::string::npos)
        return result;

    size_t tagEnd = xml.find(closeTag, tagStart);
    if (tagEnd == std::string::npos)
        return result;

    std::string content = xml.substr(tagStart + openTag.length(),
                                     tagEnd - tagStart - openTag.length());

    // Look for rdf:li items
    std::string liOpen = "<rdf:li>";
    std::string liClose = "</rdf:li>";

    size_t pos = 0;
    while ((pos = content.find(liOpen, pos)) != std::string::npos) {
        pos += liOpen.length();
        size_t end = content.find(liClose, pos);
        if (end == std::string::npos)
            break;
        result.push_back(content.substr(pos, end - pos));
        pos = end + liClose.length();
    }

    // Also try xml:lang form
    std::regex liRegex(R"(<rdf:li[^>]*>([^<]*)</rdf:li>)");
    std::sregex_iterator it(content.begin(), content.end(), liRegex);
    std::sregex_iterator itEnd;

    if (result.empty()) {
        for (; it != itEnd; ++it) {
            if ((*it).size() > 1) {
                result.push_back((*it)[1].str());
            }
        }
    }

    return result;
}

std::vector<XmpLangAlt> XmpParser::extractLangAlt(const std::string& xml,
                                                  const std::string& tag) {
    std::vector<XmpLangAlt> result;

    std::string openTag = "<" + tag + ">";
    std::string closeTag = "</" + tag + ">";

    size_t tagStart = xml.find(openTag);
    if (tagStart == std::string::npos)
        return result;

    size_t tagEnd = xml.find(closeTag, tagStart);
    if (tagEnd == std::string::npos)
        return result;

    std::string content = xml.substr(tagStart + openTag.length(),
                                     tagEnd - tagStart - openTag.length());

    // Look for rdf:li items with xml:lang attribute
    std::regex liRegex(R"(<rdf:li\s+xml:lang=\"([^\"]*)\">([^<]*)</rdf:li>)");
    std::sregex_iterator it(content.begin(), content.end(), liRegex);
    std::sregex_iterator itEnd;

    for (; it != itEnd; ++it) {
        if ((*it).size() > 2) {
            XmpLangAlt item;
            item.language = (*it)[1].str();
            item.value = (*it)[2].str();
            result.push_back(item);
        }
    }

    // If no lang items found, try simple content
    if (result.empty()) {
        auto items = extractArrayItems(xml, tag);
        for (const auto& item : items) {
            XmpLangAlt alt;
            alt.language = "x-default";
            alt.value = item;
            result.push_back(alt);
        }
    }

    return result;
}

}  // namespace atom::image::metadata
