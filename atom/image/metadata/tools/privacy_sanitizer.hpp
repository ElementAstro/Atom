#ifndef ATOM_IMAGE_METADATA_PRIVACY_SANITIZER_HPP
#define ATOM_IMAGE_METADATA_PRIVACY_SANITIZER_HPP

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "../types/exif_types.hpp"
#include "../types/iptc_types.hpp"
#include "../types/xmp_types.hpp"

namespace atom::image::metadata {

enum class PrivacyLevel {
    MINIMAL,   ///< Remove only GPS and serial numbers
    MODERATE,  ///< Remove GPS, serials, timestamps, and author info
    STRICT     ///< Remove all metadata except basic image properties
};

struct SanitizeOptions {
    bool removeGps = true;
    bool removeTimestamps = false;
    bool removeSerialNumbers = true;
    bool removeCameraInfo = false;
    bool removeAuthorInfo = false;
    bool removeThumbnail = false;
    bool removeUserComment = true;
    bool removeMakerNote = true;
    bool removeIptc = false;
    bool removeXmp = false;
    std::set<uint16_t> customTagsToRemove;

    static SanitizeOptions fromLevel(PrivacyLevel level);
};

struct SanitizeResult {
    int fieldsRemoved = 0;
    std::vector<std::string> removedFields;
    bool success = true;
    std::string error;
};

class PrivacySanitizer {
public:
    PrivacySanitizer() = default;
    explicit PrivacySanitizer(PrivacyLevel level);
    explicit PrivacySanitizer(const SanitizeOptions& options);

    void setOptions(const SanitizeOptions& options) { options_ = options; }
    [[nodiscard]] const SanitizeOptions& getOptions() const noexcept {
        return options_;
    }

    [[nodiscard]] SanitizeResult sanitize(ExifData& exif);
    [[nodiscard]] SanitizeResult sanitize(IptcData& iptc);
    [[nodiscard]] SanitizeResult sanitize(XmpData& xmp);

    bool sanitizeFile(const std::filesystem::path& inputFile,
                      const std::filesystem::path& outputFile);
    bool sanitizeFileInPlace(const std::filesystem::path& file);

    [[nodiscard]] static std::vector<uint8_t> stripAllMetadata(
        const std::vector<uint8_t>& jpegData);

    [[nodiscard]] const std::string& lastError() const noexcept {
        return lastError_;
    }

private:
    SanitizeOptions options_;
    std::string lastError_;

    void removeGpsData(ExifData& exif, SanitizeResult& result);
    void removeTimestampData(ExifData& exif, SanitizeResult& result);
    void removeSerialNumbers(ExifData& exif, SanitizeResult& result);
    void removeCameraData(ExifData& exif, SanitizeResult& result);
    void removeAuthorData(ExifData& exif, SanitizeResult& result);
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_PRIVACY_SANITIZER_HPP
