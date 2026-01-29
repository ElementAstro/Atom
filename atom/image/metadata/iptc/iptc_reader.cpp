#include "iptc_reader.hpp"

#include <cstring>
#include <fstream>

namespace atom::image::metadata {

namespace {

constexpr uint8_t IPTC_MARKER = 0x1C;
constexpr size_t IPTC_HEADER_SIZE = 5;
constexpr size_t MAX_FILE_SIZE = 100 * 1024 * 1024;

// Photoshop 3.0 resource header
constexpr char PHOTOSHOP_HEADER[] = "Photoshop 3.0";
constexpr char IPTC_RESOURCE_ID[] = "8BIM";
constexpr uint16_t IPTC_RESOURCE_TYPE = 0x0404;

}  // namespace

IptcReader::IptcReader(const std::filesystem::path& path) : filepath_(path) {}

std::optional<IptcData> IptcReader::fromFile(
    const std::filesystem::path& path) {
    IptcReader reader(path);
    if (reader.parse()) {
        return reader.getIptcData();
    }
    return std::nullopt;
}

std::optional<IptcData> IptcReader::fromMemory(const void* data, size_t size) {
    IptcReader reader;
    if (reader.parseFromMemory(data, size)) {
        return reader.getIptcData();
    }
    return std::nullopt;
}

bool IptcReader::parse() {
    if (filepath_.empty()) {
        lastError_ = "No file path specified";
        return false;
    }

    if (!loadFile()) {
        return false;
    }

    return parseJpeg();
}

bool IptcReader::parseFromMemory(const void* data, size_t size) {
    if (data == nullptr || size == 0) {
        lastError_ = "Invalid data";
        return false;
    }

    fileData_.resize(size);
    std::memcpy(fileData_.data(), data, size);

    return parseJpeg();
}

bool IptcReader::loadFile() {
    std::ifstream file(filepath_, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        lastError_ = "Cannot open file: " + filepath_.string();
        return false;
    }

    auto fileSize = file.tellg();
    if (fileSize <= 0 || static_cast<size_t>(fileSize) > MAX_FILE_SIZE) {
        lastError_ = "Invalid file size";
        return false;
    }

    file.seekg(0, std::ios::beg);
    fileData_.resize(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(fileData_.data()), fileSize)) {
        lastError_ = "Failed to read file";
        return false;
    }

    return true;
}

bool IptcReader::parseJpeg() {
    iptcData_ = IptcData{};

    auto iptcLocation = findIptcInJpeg(fileData_.data(), fileData_.size());
    if (!iptcLocation) {
        // No IPTC data - not an error
        return true;
    }

    auto [offset, size] = *iptcLocation;
    auto result = parseIptcIim(fileData_.data() + offset, size);
    if (result) {
        iptcData_ = std::move(*result);
    }

    return true;
}

std::optional<std::pair<size_t, size_t>> IptcReader::findIptcInJpeg(
    const uint8_t* data, size_t size) {
    if (size < 4 || data[0] != 0xFF || data[1] != 0xD8) {
        return std::nullopt;  // Not a JPEG
    }

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

        uint16_t segmentLength =
            (static_cast<uint16_t>(data[pos + 2]) << 8) | data[pos + 3];

        // APP13 marker (IPTC/Photoshop)
        if (marker == 0xED) {
            // Check for Photoshop 3.0 header
            if (pos + 4 + sizeof(PHOTOSHOP_HEADER) - 1 < size &&
                std::memcmp(data + pos + 4, PHOTOSHOP_HEADER,
                            sizeof(PHOTOSHOP_HEADER) - 1) == 0) {
                // Find IPTC resource (8BIM with type 0x0404)
                size_t resPos = pos + 4 + sizeof(PHOTOSHOP_HEADER);

                while (resPos + 8 < pos + 2 + segmentLength) {
                    // Check for 8BIM
                    if (std::memcmp(data + resPos, IPTC_RESOURCE_ID, 4) != 0) {
                        break;
                    }

                    uint16_t resType =
                        (static_cast<uint16_t>(data[resPos + 4]) << 8) |
                        data[resPos + 5];

                    // Skip pascal string (resource name)
                    size_t nameLen = data[resPos + 6];
                    if (nameLen % 2 == 0)
                        nameLen++;  // Pad to even
                    size_t dataOffset = resPos + 7 + nameLen;

                    if (dataOffset + 4 > size)
                        break;

                    uint32_t dataSize =
                        (static_cast<uint32_t>(data[dataOffset]) << 24) |
                        (static_cast<uint32_t>(data[dataOffset + 1]) << 16) |
                        (static_cast<uint32_t>(data[dataOffset + 2]) << 8) |
                        data[dataOffset + 3];

                    if (resType == IPTC_RESOURCE_TYPE) {
                        return std::make_pair(dataOffset + 4, dataSize);
                    }

                    // Move to next resource (with padding to even boundary)
                    size_t nextRes = dataOffset + 4 + dataSize;
                    if (dataSize % 2 != 0)
                        nextRes++;
                    resPos = nextRes;
                }
            }
        }

        // Stop at SOS
        if (marker == 0xDA)
            break;

        pos += 2 + segmentLength;
    }

    return std::nullopt;
}

std::optional<IptcData> IptcReader::parseIptcIim(const uint8_t* data,
                                                 size_t size) {
    IptcData result;
    size_t pos = 0;

    std::string dateCreated, timeCreated;
    std::string digitalDateCreated, digitalTimeCreated;

    while (pos + IPTC_HEADER_SIZE <= size) {
        if (data[pos] != IPTC_MARKER) {
            ++pos;
            continue;
        }

        uint8_t record = data[pos + 1];
        uint8_t tag = data[pos + 2];
        uint16_t dataSize =
            (static_cast<uint16_t>(data[pos + 3]) << 8) | data[pos + 4];

        // Extended size indicator
        if (dataSize & 0x8000) {
            // Size is in following bytes - skip for now
            ++pos;
            continue;
        }

        pos += IPTC_HEADER_SIZE;

        if (pos + dataSize > size)
            break;

        // Store raw dataset
        IptcDataset dataset;
        dataset.record = static_cast<IptcRecord>(record);
        dataset.tag = tag;
        dataset.data.assign(data + pos, data + pos + dataSize);
        result.rawDatasets.push_back(dataset);

        // Process Application Record (2:xxx)
        if (record == static_cast<uint8_t>(IptcRecord::APPLICATION)) {
            std::string value(reinterpret_cast<const char*>(data + pos),
                              dataSize);

            switch (static_cast<IptcTag>(tag)) {
                case IptcTag::RECORD_VERSION:
                    if (dataSize >= 2) {
                        result.recordVersion =
                            (static_cast<uint16_t>(data[pos]) << 8) |
                            data[pos + 1];
                    }
                    break;

                case IptcTag::OBJECT_NAME:
                    result.objectName = value;
                    break;

                case IptcTag::HEADLINE:
                    result.headline = value;
                    break;

                case IptcTag::CAPTION:
                    result.caption = value;
                    break;

                case IptcTag::KEYWORDS:
                    result.addKeyword(value);
                    break;

                case IptcTag::CATEGORY:
                    result.category = value;
                    break;

                case IptcTag::SUPPLEMENTAL_CATEGORY:
                    result.supplementalCategories.push_back(value);
                    break;

                case IptcTag::URGENCY:
                    if (!value.empty() && value[0] >= '1' && value[0] <= '9') {
                        result.urgency =
                            static_cast<IptcUrgency>(value[0] - '0');
                    }
                    break;

                case IptcTag::SPECIAL_INSTRUCTIONS:
                    result.specialInstructions = value;
                    break;

                case IptcTag::BYLINE:
                    result.byline = value;
                    break;

                case IptcTag::BYLINE_TITLE:
                    result.bylineTitle = value;
                    break;

                case IptcTag::WRITER_EDITOR:
                    result.writerEditor = value;
                    break;

                case IptcTag::CREDIT:
                    result.credit = value;
                    break;

                case IptcTag::SOURCE:
                    result.source = value;
                    break;

                case IptcTag::COPYRIGHT_NOTICE:
                    result.copyrightNotice = value;
                    break;

                case IptcTag::CITY:
                    result.location.city = value;
                    break;

                case IptcTag::SUB_LOCATION:
                    result.location.subLocation = value;
                    break;

                case IptcTag::STATE:
                    result.location.state = value;
                    break;

                case IptcTag::COUNTRY:
                    result.location.country = value;
                    break;

                case IptcTag::COUNTRY_CODE:
                    result.location.countryCode = value;
                    break;

                case IptcTag::ORIGINATING_PROGRAM:
                    result.originatingProgram = value;
                    break;

                case IptcTag::PROGRAM_VERSION:
                    result.programVersion = value;
                    break;

                case IptcTag::DATE_CREATED:
                    dateCreated = value;
                    break;

                case IptcTag::TIME_CREATED:
                    timeCreated = value;
                    break;

                case IptcTag::DIGITAL_DATE_CREATED:
                    digitalDateCreated = value;
                    break;

                case IptcTag::DIGITAL_TIME_CREATED:
                    digitalTimeCreated = value;
                    break;

                case IptcTag::ORIGINAL_TRANSMISSION_REF:
                    result.originalTransmissionRef = value;
                    break;

                case IptcTag::FIXTURE_ID:
                    result.fixtureId = value;
                    break;

                case IptcTag::EDIT_STATUS:
                    result.editStatus = value;
                    break;

                case IptcTag::IMAGE_ORIENTATION:
                    if (!value.empty()) {
                        result.imageOrientation =
                            static_cast<IptcImageOrientation>(value[0]);
                    }
                    break;

                case IptcTag::LANGUAGE_ID:
                    result.languageId = value;
                    break;

                case IptcTag::SUBJECT_REFERENCE:
                    result.subjectReferences.push_back(value);
                    break;

                default:
                    break;
            }
        }

        pos += dataSize;
    }

    // Parse dates
    if (!dateCreated.empty()) {
        result.dateCreated = parseIptcDate(dateCreated, timeCreated);
    }
    if (!digitalDateCreated.empty()) {
        result.digitalDateCreated =
            parseIptcDate(digitalDateCreated, digitalTimeCreated);
    }

    return result;
}

std::optional<std::chrono::system_clock::time_point> IptcReader::parseIptcDate(
    const std::string& dateStr, const std::string& timeStr) {
    // IPTC date format: YYYYMMDD or YYYY-MM-DD
    // IPTC time format: HHMMSS or HH:MM:SS with optional timezone

    if (dateStr.size() < 8)
        return std::nullopt;

    int year, month, day;
    try {
        if (dateStr[4] == '-' || dateStr[4] == ':') {
            year = std::stoi(dateStr.substr(0, 4));
            month = std::stoi(dateStr.substr(5, 2));
            day = std::stoi(dateStr.substr(8, 2));
        } else {
            year = std::stoi(dateStr.substr(0, 4));
            month = std::stoi(dateStr.substr(4, 2));
            day = std::stoi(dateStr.substr(6, 2));
        }
    } catch (...) {
        return std::nullopt;
    }

    int hour = 0, minute = 0, second = 0;
    if (timeStr.size() >= 6) {
        try {
            if (timeStr[2] == ':') {
                hour = std::stoi(timeStr.substr(0, 2));
                minute = std::stoi(timeStr.substr(3, 2));
                second = std::stoi(timeStr.substr(6, 2));
            } else {
                hour = std::stoi(timeStr.substr(0, 2));
                minute = std::stoi(timeStr.substr(2, 2));
                second = std::stoi(timeStr.substr(4, 2));
            }
        } catch (...) {
            // Ignore time parsing errors
        }
    }

    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;

#if defined(_WIN32)
    std::time_t time = _mkgmtime(&tm);
#else
    std::time_t time = timegm(&tm);
#endif

    if (time == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }

    return std::chrono::system_clock::from_time_t(time);
}

bool IptcReader::parseDataset(const uint8_t* /*data*/, size_t /*size*/,
                              size_t& /*pos*/) {
    // Implementation handled in parseIptcIim
    return true;
}

void IptcReader::processDataset(const IptcDataset& /*dataset*/) {
    // Implementation handled in parseIptcIim
}

}  // namespace atom::image::metadata
