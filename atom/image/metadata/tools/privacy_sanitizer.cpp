#include "privacy_sanitizer.hpp"

#include <cstring>
#include <fstream>

#include "../exif/exif_reader.hpp"
#include "../exif/exif_writer.hpp"

namespace atom::image::metadata {

SanitizeOptions SanitizeOptions::fromLevel(PrivacyLevel level) {
    SanitizeOptions opts;
    switch (level) {
        case PrivacyLevel::MINIMAL:
            opts.removeGps = true;
            opts.removeSerialNumbers = true;
            opts.removeMakerNote = true;
            break;
        case PrivacyLevel::MODERATE:
            opts.removeGps = true;
            opts.removeTimestamps = true;
            opts.removeSerialNumbers = true;
            opts.removeAuthorInfo = true;
            opts.removeUserComment = true;
            opts.removeMakerNote = true;
            break;
        case PrivacyLevel::STRICT:
            opts.removeGps = true;
            opts.removeTimestamps = true;
            opts.removeSerialNumbers = true;
            opts.removeCameraInfo = true;
            opts.removeAuthorInfo = true;
            opts.removeThumbnail = true;
            opts.removeUserComment = true;
            opts.removeMakerNote = true;
            opts.removeIptc = true;
            opts.removeXmp = true;
            break;
    }
    return opts;
}

PrivacySanitizer::PrivacySanitizer(PrivacyLevel level)
    : options_(SanitizeOptions::fromLevel(level)) {}

PrivacySanitizer::PrivacySanitizer(const SanitizeOptions& options)
    : options_(options) {}

SanitizeResult PrivacySanitizer::sanitize(ExifData& exif) {
    SanitizeResult result;

    if (options_.removeGps)
        removeGpsData(exif, result);
    if (options_.removeTimestamps)
        removeTimestampData(exif, result);
    if (options_.removeSerialNumbers)
        removeSerialNumbers(exif, result);
    if (options_.removeCameraInfo)
        removeCameraData(exif, result);
    if (options_.removeAuthorInfo)
        removeAuthorData(exif, result);

    if (options_.removeThumbnail && exif.thumbnailData) {
        exif.thumbnailData.reset();
        exif.thumbnailOffset.reset();
        exif.thumbnailLength.reset();
        exif.thumbnailWidth.reset();
        exif.thumbnailHeight.reset();
        result.removedFields.push_back("Thumbnail");
        ++result.fieldsRemoved;
    }

    if (options_.removeUserComment && exif.userComment) {
        exif.userComment.reset();
        result.removedFields.push_back("UserComment");
        ++result.fieldsRemoved;
    }

    if (options_.removeMakerNote && exif.makerNote) {
        exif.makerNote.reset();
        result.removedFields.push_back("MakerNote");
        ++result.fieldsRemoved;
    }

    return result;
}

SanitizeResult PrivacySanitizer::sanitize(IptcData& iptc) {
    SanitizeResult result;

    if (options_.removeIptc) {
        iptc.clear();
        result.removedFields.push_back("AllIPTC");
        ++result.fieldsRemoved;
        return result;
    }

    if (options_.removeAuthorInfo) {
        if (iptc.byline) {
            iptc.byline.reset();
            result.removedFields.push_back("IPTC:Byline");
            ++result.fieldsRemoved;
        }
        if (iptc.bylineTitle) {
            iptc.bylineTitle.reset();
            result.removedFields.push_back("IPTC:BylineTitle");
            ++result.fieldsRemoved;
        }
        if (iptc.contact.address) {
            iptc.contact = IptcContact{};
            result.removedFields.push_back("IPTC:Contact");
            ++result.fieldsRemoved;
        }
    }

    if (options_.removeGps) {
        if (iptc.location.city || iptc.location.country) {
            iptc.location = IptcLocation{};
            result.removedFields.push_back("IPTC:Location");
            ++result.fieldsRemoved;
        }
    }

    return result;
}

SanitizeResult PrivacySanitizer::sanitize(XmpData& xmp) {
    SanitizeResult result;

    if (options_.removeXmp) {
        xmp.clear();
        result.removedFields.push_back("AllXMP");
        ++result.fieldsRemoved;
        return result;
    }

    if (options_.removeAuthorInfo) {
        if (!xmp.dc.creator.empty()) {
            xmp.dc.creator.clear();
            result.removedFields.push_back("XMP:dc:creator");
            ++result.fieldsRemoved;
        }
        if (!xmp.dc.rights.empty()) {
            xmp.dc.rights.clear();
            result.removedFields.push_back("XMP:dc:rights");
            ++result.fieldsRemoved;
        }
    }

    if (options_.removeTimestamps) {
        if (xmp.xmpBasic.createDate) {
            xmp.xmpBasic.createDate.reset();
            result.removedFields.push_back("XMP:xmp:CreateDate");
            ++result.fieldsRemoved;
        }
        if (xmp.xmpBasic.modifyDate) {
            xmp.xmpBasic.modifyDate.reset();
            result.removedFields.push_back("XMP:xmp:ModifyDate");
            ++result.fieldsRemoved;
        }
    }

    if (options_.removeGps) {
        if (xmp.photoshop.city) {
            xmp.photoshop.city.reset();
            result.removedFields.push_back("XMP:photoshop:City");
            ++result.fieldsRemoved;
        }
        if (xmp.photoshop.country) {
            xmp.photoshop.country.reset();
            result.removedFields.push_back("XMP:photoshop:Country");
            ++result.fieldsRemoved;
        }
        if (xmp.photoshop.state) {
            xmp.photoshop.state.reset();
            result.removedFields.push_back("XMP:photoshop:State");
            ++result.fieldsRemoved;
        }
    }

    return result;
}

bool PrivacySanitizer::sanitizeFile(const std::filesystem::path& inputFile,
                                    const std::filesystem::path& outputFile) {
    ExifReader reader(inputFile);
    if (!reader.parse()) {
        lastError_ = reader.lastError();
        return false;
    }

    auto& exifData = reader.getExifData();
    sanitize(exifData);

    ExifWriter writer(exifData);
    if (!writer.writeToFile(inputFile, outputFile)) {
        lastError_ = writer.lastError();
        return false;
    }

    return true;
}

bool PrivacySanitizer::sanitizeFileInPlace(const std::filesystem::path& file) {
    auto tempFile = file;
    tempFile += ".sanitized.tmp";

    if (!sanitizeFile(file, tempFile))
        return false;

    std::error_code ec;
    std::filesystem::remove(file, ec);
    std::filesystem::rename(tempFile, file, ec);
    return !ec;
}

std::vector<uint8_t> PrivacySanitizer::stripAllMetadata(
    const std::vector<uint8_t>& jpegData) {
    if (jpegData.size() < 4 || jpegData[0] != 0xFF || jpegData[1] != 0xD8) {
        return jpegData;
    }

    std::vector<uint8_t> result;
    result.reserve(jpegData.size());

    // Copy SOI
    result.push_back(jpegData[0]);
    result.push_back(jpegData[1]);

    size_t pos = 2;
    while (pos + 4 < jpegData.size()) {
        if (jpegData[pos] != 0xFF) {
            result.push_back(jpegData[pos++]);
            continue;
        }

        uint8_t marker = jpegData[pos + 1];
        if (marker == 0xFF) {
            result.push_back(jpegData[pos++]);
            continue;
        }

        uint16_t segLen =
            (static_cast<uint16_t>(jpegData[pos + 2]) << 8) | jpegData[pos + 3];

        // Skip all APP markers (0xE0-0xEF) and COM (0xFE)
        if ((marker >= 0xE0 && marker <= 0xEF) || marker == 0xFE) {
            pos += 2 + segLen;
            continue;
        }

        // Copy other segments
        for (size_t i = 0; i < 2 + segLen && pos + i < jpegData.size(); ++i) {
            result.push_back(jpegData[pos + i]);
        }

        if (marker == 0xDA) {  // SOS - rest is image data
            pos += 2 + segLen;
            while (pos < jpegData.size()) {
                result.push_back(jpegData[pos++]);
            }
            break;
        }

        pos += 2 + segLen;
    }

    return result;
}

void PrivacySanitizer::removeGpsData(ExifData& exif, SanitizeResult& result) {
    if (exif.gpsData) {
        exif.gpsData.reset();
        exif.gpsIfdEntries.clear();
        result.removedFields.push_back("GPSInfo");
        ++result.fieldsRemoved;
    }
}

void PrivacySanitizer::removeTimestampData(ExifData& exif,
                                           SanitizeResult& result) {
    if (exif.dateTime) {
        exif.dateTime.reset();
        result.removedFields.push_back("DateTime");
        ++result.fieldsRemoved;
    }
    if (exif.dateTimeOriginal) {
        exif.dateTimeOriginal.reset();
        result.removedFields.push_back("DateTimeOriginal");
        ++result.fieldsRemoved;
    }
    if (exif.dateTimeDigitized) {
        exif.dateTimeDigitized.reset();
        result.removedFields.push_back("DateTimeDigitized");
        ++result.fieldsRemoved;
    }
    if (exif.subSecTime) {
        exif.subSecTime.reset();
        result.removedFields.push_back("SubSecTime");
        ++result.fieldsRemoved;
    }
    if (exif.subSecTimeOriginal) {
        exif.subSecTimeOriginal.reset();
        result.removedFields.push_back("SubSecTimeOriginal");
        ++result.fieldsRemoved;
    }
    if (exif.subSecTimeDigitized) {
        exif.subSecTimeDigitized.reset();
        result.removedFields.push_back("SubSecTimeDigitized");
        ++result.fieldsRemoved;
    }
}

void PrivacySanitizer::removeSerialNumbers(ExifData& exif,
                                           SanitizeResult& result) {
    if (exif.lensInfo.serialNumber) {
        exif.lensInfo.serialNumber.reset();
        result.removedFields.push_back("LensSerialNumber");
        ++result.fieldsRemoved;
    }
    // Camera body serial is often in MakerNote, handled separately
}

void PrivacySanitizer::removeCameraData(ExifData& exif,
                                        SanitizeResult& result) {
    if (exif.cameraMake) {
        exif.cameraMake.reset();
        result.removedFields.push_back("Make");
        ++result.fieldsRemoved;
    }
    if (exif.cameraModel) {
        exif.cameraModel.reset();
        result.removedFields.push_back("Model");
        ++result.fieldsRemoved;
    }
    if (exif.software) {
        exif.software.reset();
        result.removedFields.push_back("Software");
        ++result.fieldsRemoved;
    }
    if (exif.lensInfo.make) {
        exif.lensInfo.make.reset();
        result.removedFields.push_back("LensMake");
        ++result.fieldsRemoved;
    }
    if (exif.lensInfo.model) {
        exif.lensInfo.model.reset();
        result.removedFields.push_back("LensModel");
        ++result.fieldsRemoved;
    }
}

void PrivacySanitizer::removeAuthorData(ExifData& exif,
                                        SanitizeResult& result) {
    if (exif.artist) {
        exif.artist.reset();
        result.removedFields.push_back("Artist");
        ++result.fieldsRemoved;
    }
    if (exif.copyright) {
        exif.copyright.reset();
        result.removedFields.push_back("Copyright");
        ++result.fieldsRemoved;
    }
    if (exif.imageDescription) {
        exif.imageDescription.reset();
        result.removedFields.push_back("ImageDescription");
        ++result.fieldsRemoved;
    }
}

}  // namespace atom::image::metadata
