#include "exif_reader.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

#include "../utils/datetime_utils.hpp"
#include "exif_tags.hpp"
#include "gps_parser.hpp"

namespace atom::image::metadata {

namespace {

constexpr size_t MAX_FILE_SIZE = 100 * 1024 * 1024;  // 100 MB
constexpr uint8_t JPEG_SOI_1 = 0xFF;
constexpr uint8_t JPEG_SOI_2 = 0xD8;

bool isJpeg(const std::byte* data, size_t size) {
    return size >= 2 && data[0] == std::byte{JPEG_SOI_1} &&
           data[1] == std::byte{JPEG_SOI_2};
}

bool isTiff(const std::byte* data, size_t size) {
    if (size < 4)
        return false;
    // Check for "II" (little-endian) or "MM" (big-endian)
    return (data[0] == std::byte{0x49} && data[1] == std::byte{0x49}) ||
           (data[0] == std::byte{0x4D} && data[1] == std::byte{0x4D});
}

}  // namespace

ExifReader::ExifReader(std::string_view filename)
    : filename_(std::string(filename)) {}

ExifReader::ExifReader(const std::filesystem::path& path)
    : filename_(path.string()) {}

std::optional<ExifData> ExifReader::fromFile(
    const std::filesystem::path& filename) {
    ExifReader reader(filename);
    if (reader.parse()) {
        return reader.getExifData();
    }
    return std::nullopt;
}

std::optional<ExifData> ExifReader::fromFile(std::string_view filename) {
    return fromFile(std::filesystem::path(filename));
}

std::optional<ExifData> ExifReader::fromMemory(const void* data, size_t size) {
    ExifReader reader;
    if (reader.parseFromMemory(data, size)) {
        return reader.getExifData();
    }
    return std::nullopt;
}

std::optional<ExifData> ExifReader::fromMemory(
    const std::vector<uint8_t>& data) {
    return fromMemory(data.data(), data.size());
}

bool ExifReader::parse() {
    if (filename_.empty()) {
        lastError_ = "No filename specified";
        return false;
    }

    if (!loadFile()) {
        return false;
    }

    return parseBuffer();
}

bool ExifReader::parseFromMemory(const void* data, size_t size) {
    if (data == nullptr || size == 0) {
        lastError_ = "Invalid data pointer or size";
        return false;
    }

    if (size > MAX_FILE_SIZE) {
        lastError_ = "Data too large";
        return false;
    }

    fileData_.resize(size);
    std::memcpy(fileData_.data(), data, size);

    return parseBuffer();
}

bool ExifReader::loadFile() {
    std::ifstream file(filename_, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        lastError_ = "Cannot open file: " + filename_;
        return false;
    }

    auto fileSize = file.tellg();
    if (fileSize <= 0) {
        lastError_ = "Empty file: " + filename_;
        return false;
    }

    if (static_cast<size_t>(fileSize) > MAX_FILE_SIZE) {
        lastError_ = "File too large: " + filename_;
        return false;
    }

    file.seekg(0, std::ios::beg);
    std::vector<char> charBuffer(static_cast<size_t>(fileSize));
    if (!file.read(charBuffer.data(), fileSize)) {
        lastError_ = "Failed to read file: " + filename_;
        return false;
    }

    fileData_.resize(charBuffer.size());
    std::transform(charBuffer.begin(), charBuffer.end(), fileData_.begin(),
                   [](char c) { return std::byte{static_cast<uint8_t>(c)}; });

    return true;
}

bool ExifReader::parseBuffer() {
    if (fileData_.empty()) {
        lastError_ = "No data to parse";
        return false;
    }

    exifData_ = ExifData{};

    if (isJpeg(fileData_.data(), fileData_.size())) {
        return parseJpegExif();
    } else if (isTiff(fileData_.data(), fileData_.size())) {
        return parseTiffExif();
    }

    lastError_ = "Unsupported file format";
    return false;
}

bool ExifReader::parseJpegExif() {
    auto exifLocation = findExifInJpeg(fileData_.data(), fileData_.size());
    if (!exifLocation) {
        // No EXIF data - not an error, just no metadata
        return true;
    }

    auto [exifOffset, exifSize] = *exifLocation;
    const std::byte* exifData = fileData_.data() + exifOffset;

    // Parse TIFF header within EXIF data
    auto tiffHeader = parseTiffHeader(exifData, exifSize);
    if (!tiffHeader) {
        lastError_ = "Invalid TIFF header in EXIF data";
        return false;
    }

    auto [byteOrder, firstIfdOffset] = *tiffHeader;

    // Create IFD parser
    IfdParser parser(exifData, exifSize, byteOrder);

    // Parse IFD chain (IFD0, IFD1, ...)
    auto ifdChain = parser.parseIfdChain(firstIfdOffset);
    if (ifdChain.empty()) {
        lastError_ = "Failed to parse IFD: " + parser.lastError();
        return false;
    }

    // Extract EXIF data from parsed IFDs
    extractExifData(ifdChain, parser);

    return true;
}

bool ExifReader::parseTiffExif() {
    auto tiffHeader = parseTiffHeader(fileData_.data(), fileData_.size());
    if (!tiffHeader) {
        lastError_ = "Invalid TIFF header";
        return false;
    }

    auto [byteOrder, firstIfdOffset] = *tiffHeader;

    IfdParser parser(fileData_.data(), fileData_.size(), byteOrder);
    auto ifdChain = parser.parseIfdChain(firstIfdOffset);

    if (ifdChain.empty()) {
        lastError_ = "Failed to parse IFD: " + parser.lastError();
        return false;
    }

    extractExifData(ifdChain, parser);
    return true;
}

void ExifReader::extractExifData(const std::vector<IfdParseResult>& ifdChain,
                                 IfdParser& parser) {
    for (const auto& ifd : ifdChain) {
        if (ifd.type == IfdType::IFD0) {
            extractCameraInfo(ifd);
            extractImageProperties(ifd);
            extractTimestamps(ifd);

            // Parse EXIF sub-IFD
            if (ifd.exifIfdOffset) {
                auto exifIfd = parser.parseExifIfd(*ifd.exifIfdOffset);
                if (exifIfd) {
                    extractCameraSettings(*exifIfd);
                    extractLensInfo(*exifIfd);
                    extractTimestamps(*exifIfd);

                    // Store raw entries
                    exifData_.exifIfdEntries.clear();
                    for (const auto& entry : exifIfd->entries) {
                        IfdEntry ie;
                        ie.tag = entry.tag;
                        ie.type = entry.type;
                        ie.count = entry.count;
                        ie.valueOffset = entry.valueOffset;
                        exifData_.exifIfdEntries.push_back(ie);
                    }

                    // Parse Interoperability sub-IFD
                    if (exifIfd->interopIfdOffset) {
                        auto interopIfd =
                            parser.parseInteropIfd(*exifIfd->interopIfdOffset);
                        if (interopIfd) {
                            for (const auto& entry : interopIfd->entries) {
                                IfdEntry ie;
                                ie.tag = entry.tag;
                                ie.type = entry.type;
                                ie.count = entry.count;
                                exifData_.interopIfdEntries.push_back(ie);
                            }
                        }
                    }
                }
            }

            // Parse GPS sub-IFD
            if (ifd.gpsIfdOffset) {
                auto gpsIfd = parser.parseGpsIfd(*ifd.gpsIfdOffset);
                if (gpsIfd) {
                    exifData_.gpsData = GpsParser::parse(*gpsIfd);

                    // Store raw entries
                    exifData_.gpsIfdEntries.clear();
                    for (const auto& entry : gpsIfd->entries) {
                        IfdEntry ie;
                        ie.tag = entry.tag;
                        ie.type = entry.type;
                        ie.count = entry.count;
                        exifData_.gpsIfdEntries.push_back(ie);
                    }
                }
            }

            // Store IFD0 entries
            exifData_.ifd0Entries.clear();
            for (const auto& entry : ifd.entries) {
                IfdEntry ie;
                ie.tag = entry.tag;
                ie.type = entry.type;
                ie.count = entry.count;
                ie.valueOffset = entry.valueOffset;
                exifData_.ifd0Entries.push_back(ie);
            }
        } else if (ifd.type == IfdType::IFD1) {
            // Extract thumbnail
            extractThumbnail(ifd, parser.reader());
        }
    }
}

void ExifReader::extractCameraInfo(const IfdParseResult& ifd) {
    exifData_.cameraMake = ifd.getString(ExifTag::MAKE);
    exifData_.cameraModel = ifd.getString(ExifTag::MODEL);
    exifData_.software = ifd.getString(ExifTag::SOFTWARE);
    exifData_.artist = ifd.getString(ExifTag::ARTIST);
    exifData_.copyright = ifd.getString(ExifTag::COPYRIGHT);
    exifData_.imageDescription = ifd.getString(ExifTag::IMAGE_DESCRIPTION);
}

void ExifReader::extractCameraSettings(const IfdParseResult& ifd) {
    auto& settings = exifData_.cameraSettings;

    if (auto exp = ifd.getDouble(ExifTag::EXPOSURE_TIME)) {
        settings.exposureTime = *exp;
    }
    if (auto fn = ifd.getDouble(ExifTag::F_NUMBER)) {
        settings.fNumber = *fn;
    }
    if (auto iso = ifd.getInt(ExifTag::ISO_SPEED_RATINGS)) {
        settings.isoSpeed = static_cast<int>(*iso);
    }
    if (auto fl = ifd.getDouble(ExifTag::FOCAL_LENGTH)) {
        settings.focalLength = *fl;
    }
    if (auto fl35 = ifd.getInt(ExifTag::FOCAL_LENGTH_IN_35MM_FILM)) {
        settings.focalLength35mm = static_cast<double>(*fl35);
    }
    if (auto ep = ifd.getInt(ExifTag::EXPOSURE_PROGRAM)) {
        settings.exposureProgram = static_cast<ExposureProgram>(*ep);
    }
    if (auto mm = ifd.getInt(ExifTag::METERING_MODE)) {
        settings.meteringMode = static_cast<MeteringMode>(*mm);
    }
    if (auto flash = ifd.getInt(ExifTag::FLASH)) {
        settings.flashMode = static_cast<FlashMode>(*flash);
    }
    if (auto wb = ifd.getInt(ExifTag::WHITE_BALANCE)) {
        settings.whiteBalance = static_cast<WhiteBalance>(*wb);
    }
    if (auto ls = ifd.getInt(ExifTag::LIGHT_SOURCE)) {
        settings.lightSource = static_cast<LightSource>(*ls);
    }
    if (auto eb = ifd.getDouble(ExifTag::EXPOSURE_BIAS_VALUE)) {
        settings.exposureBias = *eb;
    }
    if (auto ma = ifd.getDouble(ExifTag::MAX_APERTURE_VALUE)) {
        settings.maxAperture = *ma;
    }
    if (auto dz = ifd.getDouble(ExifTag::DIGITAL_ZOOM_RATIO)) {
        settings.digitalZoomRatio = *dz;
    }
    if (auto c = ifd.getInt(ExifTag::CONTRAST)) {
        settings.contrast = static_cast<int>(*c);
    }
    if (auto s = ifd.getInt(ExifTag::SATURATION)) {
        settings.saturation = static_cast<int>(*s);
    }
    if (auto sh = ifd.getInt(ExifTag::SHARPNESS)) {
        settings.sharpness = static_cast<int>(*sh);
    }

    // User comment
    if (auto uc = ifd.getString(ExifTag::USER_COMMENT)) {
        // Remove encoding prefix if present (e.g., "ASCII\0\0\0")
        std::string comment = *uc;
        if (comment.size() > 8) {
            // Check for known encodings
            if (comment.substr(0, 5) == "ASCII" ||
                comment.substr(0, 7) == "UNICODE" ||
                comment.substr(0, 3) == "JIS") {
                size_t start = comment.find_first_not_of('\0', 8);
                if (start != std::string::npos) {
                    comment = comment.substr(start);
                }
            }
        }
        exifData_.userComment = comment;
    }

    // Maker note (raw bytes)
    if (const auto* entry = ifd.findEntry(ExifTag::MAKER_NOTE)) {
        if (!entry->byteArray.empty()) {
            exifData_.makerNote = entry->byteArray;
        }
    }
}

void ExifReader::extractImageProperties(const IfdParseResult& ifd) {
    auto& props = exifData_.imageProperties;

    if (auto w = ifd.getInt(ExifTag::IMAGE_WIDTH)) {
        props.width = static_cast<int>(*w);
    }
    if (auto h = ifd.getInt(ExifTag::IMAGE_HEIGHT)) {
        props.height = static_cast<int>(*h);
    }
    if (auto bps = ifd.getInt(ExifTag::BITS_PER_SAMPLE)) {
        props.bitsPerSample = static_cast<int>(*bps);
    }
    if (auto spp = ifd.getInt(ExifTag::SAMPLES_PER_PIXEL)) {
        props.samplesPerPixel = static_cast<int>(*spp);
    }
    if (auto cs = ifd.getInt(ExifTag::COLOR_SPACE)) {
        props.colorSpace = static_cast<ColorSpace>(*cs);
    }
    if (auto comp = ifd.getInt(ExifTag::COMPRESSION)) {
        props.compression = static_cast<CompressionType>(*comp);
    }
    if (auto orient = ifd.getInt(ExifTag::ORIENTATION)) {
        props.orientation = static_cast<ExifOrientation>(*orient);
    }
    if (auto xres = ifd.getDouble(ExifTag::X_RESOLUTION)) {
        props.xResolution = *xres;
    }
    if (auto yres = ifd.getDouble(ExifTag::Y_RESOLUTION)) {
        props.yResolution = *yres;
    }
    if (auto ru = ifd.getInt(ExifTag::RESOLUTION_UNIT)) {
        props.resolutionUnit =
            getResolutionUnitDescription(static_cast<int>(*ru));
    }

    // Also check EXIF-specific dimension tags
    if (auto pw = ifd.getInt(ExifTag::PIXEL_X_DIMENSION)) {
        if (!props.width.has_value()) {
            props.width = static_cast<int>(*pw);
        }
    }
    if (auto ph = ifd.getInt(ExifTag::PIXEL_Y_DIMENSION)) {
        if (!props.height.has_value()) {
            props.height = static_cast<int>(*ph);
        }
    }
}

void ExifReader::extractLensInfo(const IfdParseResult& ifd) {
    auto& lens = exifData_.lensInfo;

    lens.make = ifd.getString(ExifTag::LENS_MAKE);
    lens.model = ifd.getString(ExifTag::LENS_MODEL);
    lens.serialNumber = ifd.getString(ExifTag::LENS_SERIAL_NUMBER);

    // Lens specification (4 rationals: min/max focal length, min f-number at
    // min/max focal)
    auto lensSpec = ifd.getRationalArray(ExifTag::LENS_SPECIFICATION);
    if (lensSpec.size() >= 4) {
        lens.minFocalLength = lensSpec[0].toDouble();
        lens.maxFocalLength = lensSpec[1].toDouble();
        lens.minFNumberAtMinFocal = lensSpec[2].toDouble();
        lens.minFNumberAtMaxFocal = lensSpec[3].toDouble();
    }
}

void ExifReader::extractTimestamps(const IfdParseResult& ifd) {
    // DateTime (modification time)
    if (auto dt = ifd.getString(ExifTag::DATE_TIME)) {
        exifData_.dateTime = DateTimeUtils::parseExifDateTime(*dt);
    }

    // DateTimeOriginal (when photo was taken)
    if (auto dto = ifd.getString(ExifTag::DATE_TIME_ORIGINAL)) {
        exifData_.dateTimeOriginal = DateTimeUtils::parseExifDateTime(*dto);
    }

    // DateTimeDigitized
    if (auto dtd = ifd.getString(ExifTag::DATE_TIME_DIGITIZED)) {
        exifData_.dateTimeDigitized = DateTimeUtils::parseExifDateTime(*dtd);
    }

    // Subseconds
    exifData_.subSecTime = ifd.getString(ExifTag::SUB_SEC_TIME);
    exifData_.subSecTimeOriginal =
        ifd.getString(ExifTag::SUB_SEC_TIME_ORIGINAL);
    exifData_.subSecTimeDigitized =
        ifd.getString(ExifTag::SUB_SEC_TIME_DIGITIZED);

    // Add subseconds to timestamps
    if (exifData_.dateTime && exifData_.subSecTime) {
        exifData_.dateTime = DateTimeUtils::addSubseconds(
            *exifData_.dateTime, *exifData_.subSecTime);
    }
    if (exifData_.dateTimeOriginal && exifData_.subSecTimeOriginal) {
        exifData_.dateTimeOriginal = DateTimeUtils::addSubseconds(
            *exifData_.dateTimeOriginal, *exifData_.subSecTimeOriginal);
    }
    if (exifData_.dateTimeDigitized && exifData_.subSecTimeDigitized) {
        exifData_.dateTimeDigitized = DateTimeUtils::addSubseconds(
            *exifData_.dateTimeDigitized, *exifData_.subSecTimeDigitized);
    }
}

void ExifReader::extractThumbnail(const IfdParseResult& ifd1,
                                  const ByteReader& reader) {
    if (!ifd1.thumbnailOffset || !ifd1.thumbnailLength) {
        return;
    }

    uint32_t offset = *ifd1.thumbnailOffset;
    uint32_t length = *ifd1.thumbnailLength;

    if (offset + length > reader.size()) {
        return;  // Invalid thumbnail location
    }

    exifData_.thumbnailOffset = offset;
    exifData_.thumbnailLength = length;

    // Extract thumbnail data
    std::vector<uint8_t> thumbData(length);
    std::memcpy(thumbData.data(), reader.at(offset), length);
    exifData_.thumbnailData = std::move(thumbData);

    // Get thumbnail dimensions if available
    if (auto tw = ifd1.getInt(ExifTag::IMAGE_WIDTH)) {
        exifData_.thumbnailWidth = static_cast<int>(*tw);
    }
    if (auto th = ifd1.getInt(ExifTag::IMAGE_HEIGHT)) {
        exifData_.thumbnailHeight = static_cast<int>(*th);
    }
}

std::vector<uint8_t> ExifReader::extractThumbnail() const {
    if (exifData_.thumbnailData) {
        return *exifData_.thumbnailData;
    }
    return {};
}

bool ExifReader::validateData() const {
    // Basic validation
    if (!exifData_.hasData()) {
        return true;  // Empty data is valid
    }

    // Check timestamp validity
    if (exifData_.dateTimeOriginal) {
        auto now = std::chrono::system_clock::now();
        if (*exifData_.dateTimeOriginal > now) {
            return false;  // Future date
        }
    }

    // Check ISO range
    if (exifData_.cameraSettings.isoSpeed) {
        int iso = *exifData_.cameraSettings.isoSpeed;
        if (iso < 1 || iso > 10000000) {
            return false;
        }
    }

    // Check f-number range
    if (exifData_.cameraSettings.fNumber) {
        double f = *exifData_.cameraSettings.fNumber;
        if (f < 0.5 || f > 100) {
            return false;
        }
    }

    return true;
}

void ExifReader::optimize() {
    // Shrink string fields
    auto shrinkString = [](std::optional<std::string>& opt) {
        if (opt && !opt->empty()) {
            opt->shrink_to_fit();
        }
    };

    shrinkString(exifData_.cameraMake);
    shrinkString(exifData_.cameraModel);
    shrinkString(exifData_.software);
    shrinkString(exifData_.artist);
    shrinkString(exifData_.copyright);
    shrinkString(exifData_.userComment);
    shrinkString(exifData_.imageDescription);
    shrinkString(exifData_.lensInfo.make);
    shrinkString(exifData_.lensInfo.model);
    shrinkString(exifData_.lensInfo.serialNumber);

    // Shrink vectors
    exifData_.ifd0Entries.shrink_to_fit();
    exifData_.exifIfdEntries.shrink_to_fit();
    exifData_.gpsIfdEntries.shrink_to_fit();
    exifData_.interopIfdEntries.shrink_to_fit();

    if (exifData_.thumbnailData) {
        exifData_.thumbnailData->shrink_to_fit();
    }
}

void ExifReader::clear() {
    filename_.clear();
    fileData_.clear();
    exifData_ = ExifData{};
    lastError_.clear();
}

std::unique_ptr<ExifReader> ExifReader::clone() const {
    auto cloned = std::make_unique<ExifReader>();
    cloned->filename_ = filename_;
    cloned->exifData_ = exifData_;
    cloned->loadMode_ = loadMode_;
    return cloned;
}

std::string ExifReader::toJson() const {
    std::ostringstream oss;
    oss << "{\n";

    auto addString = [&oss](const char* name,
                            const std::optional<std::string>& val,
                            bool& first) {
        if (val) {
            if (!first)
                oss << ",\n";
            first = false;
            oss << "  \"" << name << "\": \"" << *val << "\"";
        }
    };

    auto addInt = [&oss](const char* name, const std::optional<int>& val,
                         bool& first) {
        if (val) {
            if (!first)
                oss << ",\n";
            first = false;
            oss << "  \"" << name << "\": " << *val;
        }
    };

    auto addDouble = [&oss](const char* name, const std::optional<double>& val,
                            bool& first) {
        if (val) {
            if (!first)
                oss << ",\n";
            first = false;
            oss << "  \"" << name << "\": " << *val;
        }
    };

    bool first = true;

    addString("make", exifData_.cameraMake, first);
    addString("model", exifData_.cameraModel, first);
    addString("software", exifData_.software, first);
    addString("artist", exifData_.artist, first);
    addString("copyright", exifData_.copyright, first);

    if (exifData_.dateTimeOriginal) {
        if (!first)
            oss << ",\n";
        first = false;
        oss << "  \"dateTimeOriginal\": \""
            << DateTimeUtils::formatIso8601(*exifData_.dateTimeOriginal)
            << "\"";
    }

    addDouble("exposureTime", exifData_.cameraSettings.exposureTime, first);
    addDouble("fNumber", exifData_.cameraSettings.fNumber, first);
    addInt("iso", exifData_.cameraSettings.isoSpeed, first);
    addDouble("focalLength", exifData_.cameraSettings.focalLength, first);

    if (exifData_.gpsData && exifData_.gpsData->hasPosition()) {
        if (!first)
            oss << ",\n";
        first = false;
        oss << "  \"gps\": {\n";
        oss << "    \"latitude\": "
            << exifData_.gpsData->getLatitudeDecimal().value_or(0) << ",\n";
        oss << "    \"longitude\": "
            << exifData_.gpsData->getLongitudeDecimal().value_or(0);
        if (exifData_.gpsData->altitude) {
            oss << ",\n    \"altitude\": " << *exifData_.gpsData->altitude;
        }
        oss << "\n  }";
    }

    addInt("width", exifData_.imageProperties.width, first);
    addInt("height", exifData_.imageProperties.height, first);

    oss << "\n}";
    return oss.str();
}

std::string ExifReader::serialize() const {
    std::ostringstream oss;

    oss << filename_ << "\n";
    oss << exifData_.cameraMake.value_or("") << "\n";
    oss << exifData_.cameraModel.value_or("") << "\n";

    if (exifData_.dateTimeOriginal) {
        oss << DateTimeUtils::formatExifDateTime(*exifData_.dateTimeOriginal);
    }
    oss << "\n";

    oss << (exifData_.cameraSettings.exposureTime
                ? std::to_string(*exifData_.cameraSettings.exposureTime)
                : "")
        << "\n";
    oss << (exifData_.cameraSettings.fNumber
                ? std::to_string(*exifData_.cameraSettings.fNumber)
                : "")
        << "\n";
    oss << (exifData_.cameraSettings.isoSpeed
                ? std::to_string(*exifData_.cameraSettings.isoSpeed)
                : "")
        << "\n";
    oss << (exifData_.cameraSettings.focalLength
                ? std::to_string(*exifData_.cameraSettings.focalLength)
                : "")
        << "\n";

    // GPS data
    if (exifData_.gpsData && exifData_.gpsData->latitude) {
        oss << "1\n";
        const auto& lat = *exifData_.gpsData->latitude;
        oss << lat.degrees << " " << lat.minutes << " " << lat.seconds << " "
            << lat.direction << "\n";
    } else {
        oss << "0\n";
    }

    if (exifData_.gpsData && exifData_.gpsData->longitude) {
        oss << "1\n";
        const auto& lon = *exifData_.gpsData->longitude;
        oss << lon.degrees << " " << lon.minutes << " " << lon.seconds << " "
            << lon.direction << "\n";
    } else {
        oss << "0\n";
    }

    return oss.str();
}

std::unique_ptr<ExifReader> ExifReader::deserialize(const std::string& data) {
    auto reader = std::make_unique<ExifReader>();
    std::istringstream iss(data);
    std::string line;

    // Filename
    std::getline(iss, reader->filename_);

    // Camera make
    std::getline(iss, line);
    if (!line.empty())
        reader->exifData_.cameraMake = line;

    // Camera model
    std::getline(iss, line);
    if (!line.empty())
        reader->exifData_.cameraModel = line;

    // DateTime
    std::getline(iss, line);
    if (!line.empty()) {
        reader->exifData_.dateTimeOriginal =
            DateTimeUtils::parseExifDateTime(line);
    }

    // Exposure time
    std::getline(iss, line);
    if (!line.empty()) {
        try {
            reader->exifData_.cameraSettings.exposureTime = std::stod(line);
        } catch (...) {
        }
    }

    // F-number
    std::getline(iss, line);
    if (!line.empty()) {
        try {
            reader->exifData_.cameraSettings.fNumber = std::stod(line);
        } catch (...) {
        }
    }

    // ISO
    std::getline(iss, line);
    if (!line.empty()) {
        try {
            reader->exifData_.cameraSettings.isoSpeed = std::stoi(line);
        } catch (...) {
        }
    }

    // Focal length
    std::getline(iss, line);
    if (!line.empty()) {
        try {
            reader->exifData_.cameraSettings.focalLength = std::stod(line);
        } catch (...) {
        }
    }

    // GPS latitude
    std::getline(iss, line);
    if (line == "1") {
        std::getline(iss, line);
        GpsCoordinate lat;
        std::istringstream coordStream(line);
        coordStream >> lat.degrees >> lat.minutes >> lat.seconds >>
            lat.direction;
        if (!reader->exifData_.gpsData) {
            reader->exifData_.gpsData = std::make_shared<GpsData>();
        }
        reader->exifData_.gpsData->latitude = lat;
    }

    // GPS longitude
    std::getline(iss, line);
    if (line == "1") {
        std::getline(iss, line);
        GpsCoordinate lon;
        std::istringstream coordStream(line);
        coordStream >> lon.degrees >> lon.minutes >> lon.seconds >>
            lon.direction;
        if (!reader->exifData_.gpsData) {
            reader->exifData_.gpsData = std::make_shared<GpsData>();
        }
        reader->exifData_.gpsData->longitude = lon;
    }

    return reader;
}

// ExifData implementation

bool ExifData::hasGps() const noexcept {
    return gpsData && gpsData->hasPosition();
}

void ExifData::clear() noexcept {
    cameraMake.reset();
    cameraModel.reset();
    software.reset();
    artist.reset();
    copyright.reset();
    dateTime.reset();
    dateTimeOriginal.reset();
    dateTimeDigitized.reset();
    subSecTime.reset();
    subSecTimeOriginal.reset();
    subSecTimeDigitized.reset();
    cameraSettings = CameraSettings{};
    lensInfo = LensInfo{};
    imageProperties = ImageProperties{};
    gpsData.reset();
    thumbnailData.reset();
    thumbnailWidth.reset();
    thumbnailHeight.reset();
    thumbnailOffset.reset();
    thumbnailLength.reset();
    userComment.reset();
    imageDescription.reset();
    makerNote.reset();
    customTags.clear();
    ifd0Entries.clear();
    exifIfdEntries.clear();
    gpsIfdEntries.clear();
    interopIfdEntries.clear();
}

}  // namespace atom::image::metadata
