#include "exif_writer.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

#include "../utils/datetime_utils.hpp"
#include "exif_tags.hpp"
#include "gps_parser.hpp"

namespace atom::image::metadata {

namespace {

constexpr size_t IFD_ENTRY_SIZE = 12;
constexpr size_t MAX_EXIF_SIZE = 65533;  // Max APP1 segment size - 2

// JPEG markers
constexpr uint8_t MARKER_PREFIX = 0xFF;
constexpr uint8_t MARKER_SOI = 0xD8;
constexpr uint8_t MARKER_APP0 = 0xE0;
constexpr uint8_t MARKER_APP1 = 0xE1;
constexpr uint8_t MARKER_SOS = 0xDA;
constexpr uint8_t MARKER_EOI = 0xD9;

bool isJpegMarker(uint8_t byte) { return byte >= 0xC0 && byte != 0xFF; }

}  // namespace

ExifWriter::ExifWriter(const ExifData& exifData) : exifData_(exifData) {}

bool ExifWriter::writeToFile(const std::filesystem::path& inputFile,
                             const std::filesystem::path& outputFile) {
    // Read input file
    std::ifstream inFile(inputFile, std::ios::binary);
    if (!inFile.is_open()) {
        lastError_ = "Cannot open input file: " + inputFile.string();
        return false;
    }

    std::vector<uint8_t> imageData((std::istreambuf_iterator<char>(inFile)),
                                   std::istreambuf_iterator<char>());
    inFile.close();

    // Embed EXIF
    auto modifiedData = embedInJpeg(imageData);
    if (modifiedData.empty()) {
        return false;
    }

    // Write output file
    std::ofstream outFile(outputFile, std::ios::binary);
    if (!outFile.is_open()) {
        lastError_ = "Cannot open output file: " + outputFile.string();
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(modifiedData.data()),
                  static_cast<std::streamsize>(modifiedData.size()));

    return true;
}

bool ExifWriter::writeToFile(const std::filesystem::path& file) {
    // Create temporary output path
    auto tempFile = file;
    tempFile += ".tmp";

    if (!writeToFile(file, tempFile)) {
        return false;
    }

    // Replace original with temporary
    std::error_code ec;
    std::filesystem::remove(file, ec);
    if (ec) {
        lastError_ = "Cannot remove original file: " + ec.message();
        std::filesystem::remove(tempFile, ec);
        return false;
    }

    std::filesystem::rename(tempFile, file, ec);
    if (ec) {
        lastError_ = "Cannot rename temporary file: " + ec.message();
        return false;
    }

    return true;
}

std::vector<uint8_t> ExifWriter::embedInJpeg(
    const std::vector<uint8_t>& imageData) {
    if (imageData.size() < 4) {
        lastError_ = "Image data too small";
        return {};
    }

    // Check JPEG signature
    if (imageData[0] != MARKER_PREFIX || imageData[1] != MARKER_SOI) {
        lastError_ = "Not a valid JPEG file";
        return {};
    }

    // Generate new EXIF segment
    auto exifSegment = generateApp1Segment();
    if (exifSegment.empty()) {
        return {};
    }

    // Find insertion point (after SOI, before or replacing existing APP1)
    size_t insertPos = 2;
    size_t skipBytes = 0;

    size_t pos = 2;
    while (pos + 4 < imageData.size()) {
        if (imageData[pos] != MARKER_PREFIX) {
            ++pos;
            continue;
        }

        uint8_t marker = imageData[pos + 1];

        // Skip padding
        if (marker == MARKER_PREFIX) {
            ++pos;
            continue;
        }

        // Check segment length
        uint16_t segmentLength =
            (static_cast<uint16_t>(imageData[pos + 2]) << 8) |
            imageData[pos + 3];

        if (marker == MARKER_APP1) {
            // Check if this is EXIF
            if (pos + 10 < imageData.size() &&
                std::memcmp(&imageData[pos + 4], "Exif\0\0", 6) == 0) {
                // Replace existing EXIF
                insertPos = pos;
                skipBytes = 2 + segmentLength;
                break;
            }
        }

        // Stop at SOS (start of scan) - image data follows
        if (marker == MARKER_SOS) {
            insertPos = pos;
            break;
        }

        // Skip APP0 (JFIF) but insert after it
        if (marker == MARKER_APP0) {
            insertPos = pos + 2 + segmentLength;
        }

        pos += 2 + segmentLength;
    }

    // Build output
    std::vector<uint8_t> result;
    result.reserve(imageData.size() + exifSegment.size());

    // Copy data before insertion point
    result.insert(result.end(), imageData.begin(),
                  imageData.begin() + static_cast<ptrdiff_t>(insertPos));

    // Insert EXIF segment
    result.insert(result.end(), exifSegment.begin(), exifSegment.end());

    // Copy remaining data (skipping old EXIF if present)
    result.insert(
        result.end(),
        imageData.begin() + static_cast<ptrdiff_t>(insertPos + skipBytes),
        imageData.end());

    return result;
}

std::vector<uint8_t> ExifWriter::generateExifSegment() {
    ByteWriter writer(4096, byteOrder_);

    // EXIF header: "Exif\0\0"
    writer.writeString("Exif");
    writer.writeUint8(0);
    writer.writeUint8(0);

    // TIFF header
    size_t tiffStart = writer.position();

    if (byteOrder_ == ByteOrder::LITTLE_ENDIAN) {
        writer.writeUint8(0x49);  // 'I'
        writer.writeUint8(0x49);  // 'I'
    } else {
        writer.writeUint8(0x4D);  // 'M'
        writer.writeUint8(0x4D);  // 'M'
    }

    writer.writeUint16(0x002A);  // TIFF magic

    // First IFD offset (relative to TIFF start)
    writer.writeUint32(8);  // IFD0 starts right after TIFF header

    // Build IFD entries
    std::vector<IfdEntry> ifd0Entries;
    std::vector<IfdEntry> exifIfdEntries;
    std::vector<IfdEntry> gpsIfdEntries;

    buildIfd0Entries(ifd0Entries);
    buildExifIfdEntries(exifIfdEntries);

    if (exifData_.gpsData && exifData_.gpsData->hasPosition()) {
        buildGpsIfdEntries(gpsIfdEntries);
    }

    // Calculate offsets
    size_t ifd0Size = 2 + ifd0Entries.size() * IFD_ENTRY_SIZE + 4;
    size_t ifd0DataSize = calculateIfdSize(ifd0Entries) - ifd0Size;

    size_t exifIfdOffset = 8 + ifd0Size + ifd0DataSize;
    size_t exifIfdSize = 2 + exifIfdEntries.size() * IFD_ENTRY_SIZE + 4;
    size_t exifIfdDataSize = calculateIfdSize(exifIfdEntries) - exifIfdSize;

    size_t gpsIfdOffset = 0;
    if (!gpsIfdEntries.empty()) {
        gpsIfdOffset = exifIfdOffset + exifIfdSize + exifIfdDataSize;
    }

    // Add EXIF IFD pointer to IFD0
    if (!exifIfdEntries.empty()) {
        IfdEntry exifPtr;
        exifPtr.tag = ExifTag::EXIF_IFD_POINTER;
        exifPtr.type = ExifDataType::LONG;
        exifPtr.count = 1;
        exifPtr.value = static_cast<uint32_t>(exifIfdOffset);
        ifd0Entries.push_back(exifPtr);
    }

    // Add GPS IFD pointer to IFD0
    if (!gpsIfdEntries.empty()) {
        IfdEntry gpsPtr;
        gpsPtr.tag = ExifTag::GPS_IFD_POINTER;
        gpsPtr.type = ExifDataType::LONG;
        gpsPtr.count = 1;
        gpsPtr.value = static_cast<uint32_t>(gpsIfdOffset);
        ifd0Entries.push_back(gpsPtr);
    }

    // Sort entries by tag (required by EXIF spec)
    std::sort(
        ifd0Entries.begin(), ifd0Entries.end(),
        [](const IfdEntry& a, const IfdEntry& b) { return a.tag < b.tag; });
    std::sort(
        exifIfdEntries.begin(), exifIfdEntries.end(),
        [](const IfdEntry& a, const IfdEntry& b) { return a.tag < b.tag; });
    std::sort(
        gpsIfdEntries.begin(), gpsIfdEntries.end(),
        [](const IfdEntry& a, const IfdEntry& b) { return a.tag < b.tag; });

    // Recalculate offsets after adding pointers
    ifd0Size = 2 + ifd0Entries.size() * IFD_ENTRY_SIZE + 4;
    ifd0DataSize = calculateIfdSize(ifd0Entries) - ifd0Size;
    exifIfdOffset = 8 + ifd0Size + ifd0DataSize;

    // Update pointer values with correct offsets
    for (auto& entry : ifd0Entries) {
        if (entry.tag == ExifTag::EXIF_IFD_POINTER && !exifIfdEntries.empty()) {
            entry.value = static_cast<uint32_t>(exifIfdOffset);
        }
        if (entry.tag == ExifTag::GPS_IFD_POINTER && !gpsIfdEntries.empty()) {
            size_t gpsOffset = exifIfdOffset;
            if (!exifIfdEntries.empty()) {
                gpsOffset += 2 + exifIfdEntries.size() * IFD_ENTRY_SIZE + 4 +
                             (calculateIfdSize(exifIfdEntries) -
                              (2 + exifIfdEntries.size() * IFD_ENTRY_SIZE + 4));
            }
            entry.value = static_cast<uint32_t>(gpsOffset);
        }
    }

    // Write IFD0 and its data area sequentially
    size_t dataAreaOffset =
        tiffStart + 8 + 2 + ifd0Entries.size() * IFD_ENTRY_SIZE + 4;
    writeIfd(writer, ifd0Entries, dataAreaOffset);

    // Write EXIF IFD sequentially (data follows IFD0's data area)
    if (!exifIfdEntries.empty()) {
        dataAreaOffset =
            writer.position() + 2 + exifIfdEntries.size() * IFD_ENTRY_SIZE + 4;
        writeIfd(writer, exifIfdEntries, dataAreaOffset);
    }

    // Write GPS IFD sequentially
    if (!gpsIfdEntries.empty()) {
        dataAreaOffset =
            writer.position() + 2 + gpsIfdEntries.size() * IFD_ENTRY_SIZE + 4;
        writeIfd(writer, gpsIfdEntries, dataAreaOffset);
    }

    // Convert to uint8_t vector
    auto& byteBuffer = writer.buffer();
    std::vector<uint8_t> result(byteBuffer.size());
    std::transform(byteBuffer.begin(), byteBuffer.end(), result.begin(),
                   [](std::byte b) { return std::to_integer<uint8_t>(b); });

    return result;
}

std::vector<uint8_t> ExifWriter::generateApp1Segment() {
    auto exifData = generateExifSegment();
    if (exifData.empty()) {
        return {};
    }

    if (exifData.size() > MAX_EXIF_SIZE) {
        lastError_ = "EXIF data too large";
        return {};
    }

    std::vector<uint8_t> result;
    result.reserve(4 + exifData.size());

    // APP1 marker
    result.push_back(MARKER_PREFIX);
    result.push_back(MARKER_APP1);

    // Segment length (includes length bytes but not marker)
    uint16_t length = static_cast<uint16_t>(exifData.size() + 2);
    result.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(length & 0xFF));

    // EXIF data
    result.insert(result.end(), exifData.begin(), exifData.end());

    return result;
}

std::vector<uint8_t> ExifWriter::stripExif(
    const std::vector<uint8_t>& imageData) {
    if (imageData.size() < 4) {
        return imageData;
    }

    // Check JPEG signature
    if (imageData[0] != MARKER_PREFIX || imageData[1] != MARKER_SOI) {
        return imageData;
    }

    std::vector<uint8_t> result;
    result.reserve(imageData.size());

    // Copy SOI
    result.push_back(imageData[0]);
    result.push_back(imageData[1]);

    size_t pos = 2;
    while (pos + 4 < imageData.size()) {
        if (imageData[pos] != MARKER_PREFIX) {
            // Not a marker - copy and continue
            result.push_back(imageData[pos]);
            ++pos;
            continue;
        }

        uint8_t marker = imageData[pos + 1];

        // Skip padding
        if (marker == MARKER_PREFIX) {
            result.push_back(imageData[pos]);
            ++pos;
            continue;
        }

        // Get segment length
        uint16_t segmentLength =
            (static_cast<uint16_t>(imageData[pos + 2]) << 8) |
            imageData[pos + 3];

        // Check if this is EXIF APP1
        if (marker == MARKER_APP1 && pos + 10 < imageData.size() &&
            std::memcmp(&imageData[pos + 4], "Exif\0\0", 6) == 0) {
            // Skip this segment
            pos += 2 + segmentLength;
            continue;
        }

        // Copy segment
        for (size_t i = 0; i < 2 + segmentLength && pos + i < imageData.size();
             ++i) {
            result.push_back(imageData[pos + i]);
        }

        // Stop at SOS - rest is image data
        if (marker == MARKER_SOS) {
            pos += 2 + segmentLength;
            // Copy remaining data
            while (pos < imageData.size()) {
                result.push_back(imageData[pos]);
                ++pos;
            }
            break;
        }

        pos += 2 + segmentLength;
    }

    return result;
}

void ExifWriter::setGpsCoordinates(double latitude, double longitude,
                                   std::optional<double> altitude) {
    if (!exifData_.gpsData) {
        exifData_.gpsData = std::make_shared<GpsData>();
    }

    exifData_.gpsData->setPosition(latitude, longitude);

    if (altitude) {
        exifData_.gpsData->altitude = std::abs(*altitude);
        exifData_.gpsData->altitudeRef = (*altitude >= 0)
                                             ? GpsAltitudeRef::ABOVE_SEA_LEVEL
                                             : GpsAltitudeRef::BELOW_SEA_LEVEL;
    }
}

void ExifWriter::removeGpsData() {
    exifData_.gpsData.reset();
    exifData_.gpsIfdEntries.clear();
}

void ExifWriter::buildIfd0Entries(std::vector<IfdEntry>& entries) {
    if (exifData_.cameraMake) {
        entries.push_back(
            createStringEntry(ExifTag::MAKE, *exifData_.cameraMake));
    }
    if (exifData_.cameraModel) {
        entries.push_back(
            createStringEntry(ExifTag::MODEL, *exifData_.cameraModel));
    }
    if (exifData_.software) {
        entries.push_back(
            createStringEntry(ExifTag::SOFTWARE, *exifData_.software));
    }
    if (exifData_.artist) {
        entries.push_back(
            createStringEntry(ExifTag::ARTIST, *exifData_.artist));
    }
    if (exifData_.copyright) {
        entries.push_back(
            createStringEntry(ExifTag::COPYRIGHT, *exifData_.copyright));
    }
    if (exifData_.imageDescription) {
        entries.push_back(createStringEntry(ExifTag::IMAGE_DESCRIPTION,
                                            *exifData_.imageDescription));
    }
    if (exifData_.dateTime) {
        entries.push_back(createStringEntry(
            ExifTag::DATE_TIME,
            DateTimeUtils::formatExifDateTime(*exifData_.dateTime)));
    }

    // Image properties
    auto& props = exifData_.imageProperties;
    if (props.orientation) {
        entries.push_back(createShortEntry(
            ExifTag::ORIENTATION, static_cast<uint16_t>(*props.orientation)));
    }
    if (props.xResolution) {
        entries.push_back(
            createRationalEntry(ExifTag::X_RESOLUTION, *props.xResolution));
    }
    if (props.yResolution) {
        entries.push_back(
            createRationalEntry(ExifTag::Y_RESOLUTION, *props.yResolution));
    }
}

void ExifWriter::buildExifIfdEntries(std::vector<IfdEntry>& entries) {
    // EXIF version
    IfdEntry version;
    version.tag = ExifTag::EXIF_VERSION;
    version.type = ExifDataType::UNDEFINED;
    version.count = 4;
    version.value = std::vector<uint8_t>{'0', '2', '3', '2'};
    entries.push_back(version);

    // Camera settings
    auto& settings = exifData_.cameraSettings;
    if (settings.exposureTime) {
        entries.push_back(createRationalEntry(ExifTag::EXPOSURE_TIME,
                                              *settings.exposureTime, 1000000));
    }
    if (settings.fNumber) {
        entries.push_back(
            createRationalEntry(ExifTag::F_NUMBER, *settings.fNumber, 100));
    }
    if (settings.isoSpeed) {
        entries.push_back(
            createShortEntry(ExifTag::ISO_SPEED_RATINGS,
                             static_cast<uint16_t>(*settings.isoSpeed)));
    }
    if (settings.focalLength) {
        entries.push_back(createRationalEntry(ExifTag::FOCAL_LENGTH,
                                              *settings.focalLength, 100));
    }
    if (settings.focalLength35mm) {
        entries.push_back(
            createShortEntry(ExifTag::FOCAL_LENGTH_IN_35MM_FILM,
                             static_cast<uint16_t>(*settings.focalLength35mm)));
    }
    if (settings.exposureProgram) {
        entries.push_back(
            createShortEntry(ExifTag::EXPOSURE_PROGRAM,
                             static_cast<uint16_t>(*settings.exposureProgram)));
    }
    if (settings.meteringMode) {
        entries.push_back(
            createShortEntry(ExifTag::METERING_MODE,
                             static_cast<uint16_t>(*settings.meteringMode)));
    }
    if (settings.flashMode) {
        entries.push_back(createShortEntry(
            ExifTag::FLASH, static_cast<uint16_t>(*settings.flashMode)));
    }
    if (settings.whiteBalance) {
        entries.push_back(
            createShortEntry(ExifTag::WHITE_BALANCE,
                             static_cast<uint16_t>(*settings.whiteBalance)));
    }
    if (settings.exposureBias) {
        IfdEntry entry;
        entry.tag = ExifTag::EXPOSURE_BIAS_VALUE;
        entry.type = ExifDataType::SRATIONAL;
        entry.count = 1;
        entry.value = SRational::fromDouble(*settings.exposureBias, 100);
        entries.push_back(entry);
    }

    // Timestamps
    if (exifData_.dateTimeOriginal) {
        entries.push_back(createStringEntry(
            ExifTag::DATE_TIME_ORIGINAL,
            DateTimeUtils::formatExifDateTime(*exifData_.dateTimeOriginal)));
    }
    if (exifData_.dateTimeDigitized) {
        entries.push_back(createStringEntry(
            ExifTag::DATE_TIME_DIGITIZED,
            DateTimeUtils::formatExifDateTime(*exifData_.dateTimeDigitized)));
    }

    // Subseconds
    if (exifData_.subSecTimeOriginal) {
        entries.push_back(createStringEntry(ExifTag::SUB_SEC_TIME_ORIGINAL,
                                            *exifData_.subSecTimeOriginal));
    }

    // User comment
    if (exifData_.userComment) {
        IfdEntry entry;
        entry.tag = ExifTag::USER_COMMENT;
        entry.type = ExifDataType::UNDEFINED;
        std::string comment = "ASCII\0\0\0" + *exifData_.userComment;
        entry.count = static_cast<uint32_t>(comment.size());
        std::vector<uint8_t> data(comment.begin(), comment.end());
        entry.value = data;
        entries.push_back(entry);
    }

    // Image dimensions
    auto& props = exifData_.imageProperties;
    if (props.width) {
        entries.push_back(createLongEntry(ExifTag::PIXEL_X_DIMENSION,
                                          static_cast<uint32_t>(*props.width)));
    }
    if (props.height) {
        entries.push_back(createLongEntry(
            ExifTag::PIXEL_Y_DIMENSION, static_cast<uint32_t>(*props.height)));
    }
    if (props.colorSpace) {
        entries.push_back(createShortEntry(
            ExifTag::COLOR_SPACE, static_cast<uint16_t>(*props.colorSpace)));
    }

    // Lens info
    auto& lens = exifData_.lensInfo;
    if (lens.make) {
        entries.push_back(createStringEntry(ExifTag::LENS_MAKE, *lens.make));
    }
    if (lens.model) {
        entries.push_back(createStringEntry(ExifTag::LENS_MODEL, *lens.model));
    }
    if (lens.serialNumber) {
        entries.push_back(
            createStringEntry(ExifTag::LENS_SERIAL_NUMBER, *lens.serialNumber));
    }
}

void ExifWriter::buildGpsIfdEntries(std::vector<IfdEntry>& entries) {
    if (!exifData_.gpsData) {
        return;
    }

    entries = GpsParser::toIfdEntries(*exifData_.gpsData);
}

size_t ExifWriter::writeIfd(ByteWriter& writer,
                            const std::vector<IfdEntry>& entries,
                            size_t dataAreaOffset) {
    // Entry count
    writer.writeUint16(static_cast<uint16_t>(entries.size()));

    // Collect data that needs to go to data area
    std::vector<std::pair<size_t, std::vector<uint8_t>>> dataAreaItems;
    size_t currentDataOffset = dataAreaOffset;

    // Write entries
    for (const auto& entry : entries) {
        writer.writeUint16(entry.tag);
        writer.writeUint16(static_cast<uint16_t>(entry.type));
        writer.writeUint32(entry.count);

        size_t valueSize = getExifDataTypeSize(entry.type) * entry.count;

        if (valueSize <= 4) {
            // Value fits inline
            size_t valuePos = writer.position();
            writeEntryValue(writer, entry);
            // Pad to 4 bytes
            while (writer.position() < valuePos + 4) {
                writer.writeUint8(0);
            }
        } else {
            // Write offset to data area
            writer.writeUint32(static_cast<uint32_t>(currentDataOffset));

            // Prepare data for data area
            ByteWriter dataWriter(valueSize, byteOrder_);
            ExifWriter tempWriter;
            tempWriter.setByteOrder(byteOrder_);

            // Store data to write later
            std::vector<uint8_t> valueData;
            // We'll serialize the value
            ByteWriter valueWriter(valueSize, byteOrder_);

            // Write value to temp buffer
            if (auto* str = std::get_if<std::string>(&entry.value)) {
                for (char c : *str) {
                    valueWriter.writeUint8(static_cast<uint8_t>(c));
                }
                valueWriter.writeUint8(0);  // Null terminator
            } else if (auto* rat = std::get_if<Rational>(&entry.value)) {
                valueWriter.writeUint32(rat->numerator);
                valueWriter.writeUint32(rat->denominator);
            } else if (auto* rats =
                           std::get_if<std::vector<Rational>>(&entry.value)) {
                for (const auto& r : *rats) {
                    valueWriter.writeUint32(r.numerator);
                    valueWriter.writeUint32(r.denominator);
                }
            } else if (auto* bytes =
                           std::get_if<std::vector<uint8_t>>(&entry.value)) {
                for (uint8_t b : *bytes) {
                    valueWriter.writeUint8(b);
                }
            }

            auto& buf = valueWriter.buffer();
            valueData.resize(buf.size());
            std::transform(
                buf.begin(), buf.end(), valueData.begin(),
                [](std::byte b) { return std::to_integer<uint8_t>(b); });

            dataAreaItems.emplace_back(currentDataOffset, std::move(valueData));
            currentDataOffset += valueSize;
        }
    }

    // Write next IFD offset (0 for no next IFD)
    size_t nextIfdOffsetPos = writer.position();
    writer.writeUint32(0);

    // Write data area items sequentially (they follow the IFD)
    for (const auto& [offset, data] : dataAreaItems) {
        (void)offset;  // Offset was pre-calculated, data written sequentially
        for (uint8_t b : data) {
            writer.writeUint8(b);
        }
    }

    return nextIfdOffsetPos;
}

size_t ExifWriter::calculateIfdSize(
    const std::vector<IfdEntry>& entries) const {
    size_t size = 2 + entries.size() * IFD_ENTRY_SIZE +
                  4;  // count + entries + next offset

    for (const auto& entry : entries) {
        size_t valueSize = getExifDataTypeSize(entry.type) * entry.count;
        if (valueSize > 4) {
            size += valueSize;
        }
    }

    return size;
}

void ExifWriter::writeEntryValue(ByteWriter& writer, const IfdEntry& entry) {
    std::visit(
        [&writer, &entry](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                // No value
            } else if constexpr (std::is_same_v<T, uint8_t>) {
                writer.writeUint8(arg);
            } else if constexpr (std::is_same_v<T, std::string>) {
                for (char c : arg) {
                    writer.writeUint8(static_cast<uint8_t>(c));
                }
                if (entry.type == ExifDataType::ASCII) {
                    writer.writeUint8(0);
                }
            } else if constexpr (std::is_same_v<T, uint16_t>) {
                writer.writeUint16(arg);
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                writer.writeUint32(arg);
            } else if constexpr (std::is_same_v<T, Rational>) {
                writer.writeRational(arg);
            } else if constexpr (std::is_same_v<T, SRational>) {
                writer.writeSRational(arg);
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                for (uint8_t b : arg) {
                    writer.writeUint8(b);
                }
            } else if constexpr (std::is_same_v<T, std::vector<uint16_t>>) {
                for (uint16_t v : arg) {
                    writer.writeUint16(v);
                }
            } else if constexpr (std::is_same_v<T, std::vector<uint32_t>>) {
                for (uint32_t v : arg) {
                    writer.writeUint32(v);
                }
            } else if constexpr (std::is_same_v<T, std::vector<Rational>>) {
                for (const auto& r : arg) {
                    writer.writeRational(r);
                }
            } else if constexpr (std::is_same_v<T, std::vector<SRational>>) {
                for (const auto& r : arg) {
                    writer.writeSRational(r);
                }
            }
        },
        entry.value);
}

IfdEntry ExifWriter::createStringEntry(uint16_t tag, const std::string& value) {
    IfdEntry entry;
    entry.tag = tag;
    entry.type = ExifDataType::ASCII;
    entry.count = static_cast<uint32_t>(value.size() + 1);  // Include null
    entry.value = value;
    return entry;
}

IfdEntry ExifWriter::createRationalEntry(uint16_t tag, double value,
                                         uint32_t precision) {
    IfdEntry entry;
    entry.tag = tag;
    entry.type = ExifDataType::RATIONAL;
    entry.count = 1;
    entry.value = Rational::fromDouble(value, precision);
    return entry;
}

IfdEntry ExifWriter::createRationalArrayEntry(uint16_t tag,
                                              const std::vector<double>& values,
                                              uint32_t precision) {
    IfdEntry entry;
    entry.tag = tag;
    entry.type = ExifDataType::RATIONAL;
    entry.count = static_cast<uint32_t>(values.size());
    std::vector<Rational> rationals;
    rationals.reserve(values.size());
    for (double v : values) {
        rationals.push_back(Rational::fromDouble(v, precision));
    }
    entry.value = rationals;
    return entry;
}

IfdEntry ExifWriter::createShortEntry(uint16_t tag, uint16_t value) {
    IfdEntry entry;
    entry.tag = tag;
    entry.type = ExifDataType::SHORT;
    entry.count = 1;
    entry.value = value;
    return entry;
}

IfdEntry ExifWriter::createLongEntry(uint16_t tag, uint32_t value) {
    IfdEntry entry;
    entry.tag = tag;
    entry.type = ExifDataType::LONG;
    entry.count = 1;
    entry.value = value;
    return entry;
}

}  // namespace atom::image::metadata
