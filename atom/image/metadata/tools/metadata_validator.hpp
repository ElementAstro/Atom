#ifndef ATOM_IMAGE_METADATA_VALIDATOR_HPP
#define ATOM_IMAGE_METADATA_VALIDATOR_HPP

#include <string>
#include <vector>

#include "../types/exif_types.hpp"
#include "../types/gps_types.hpp"
#include "../types/iptc_types.hpp"
#include "../types/xmp_types.hpp"

namespace atom::image::metadata {

enum class ValidationSeverity { INFO, WARNING, ERROR };

struct ValidationIssue {
    ValidationSeverity severity;
    std::string field;
    std::string message;
    std::string suggestion;

    [[nodiscard]] bool isError() const noexcept {
        return severity == ValidationSeverity::ERROR;
    }
};

struct ValidationResult {
    bool valid = true;
    std::vector<ValidationIssue> issues;

    void addIssue(ValidationSeverity severity, const std::string& field,
                  const std::string& message,
                  const std::string& suggestion = "") {
        issues.push_back({severity, field, message, suggestion});
        if (severity == ValidationSeverity::ERROR)
            valid = false;
    }

    [[nodiscard]] bool hasErrors() const noexcept {
        for (const auto& i : issues)
            if (i.isError())
                return true;
        return false;
    }

    [[nodiscard]] size_t errorCount() const noexcept {
        size_t c = 0;
        for (const auto& i : issues)
            if (i.isError())
                ++c;
        return c;
    }

    [[nodiscard]] size_t warningCount() const noexcept {
        size_t c = 0;
        for (const auto& i : issues)
            if (i.severity == ValidationSeverity::WARNING)
                ++c;
        return c;
    }
};

class MetadataValidator {
public:
    MetadataValidator() = default;

    [[nodiscard]] ValidationResult validate(const ExifData& exif);
    [[nodiscard]] ValidationResult validate(const GpsData& gps);
    [[nodiscard]] ValidationResult validate(const IptcData& iptc);
    [[nodiscard]] ValidationResult validate(const XmpData& xmp);

    int autoFix(ExifData& exif);
    int autoFix(GpsData& gps);

    void setStrictMode(bool strict) noexcept { strictMode_ = strict; }
    [[nodiscard]] bool isStrictMode() const noexcept { return strictMode_; }

private:
    bool strictMode_ = false;

    void validateTimestamps(const ExifData& exif, ValidationResult& result);
    void validateCameraSettings(const ExifData& exif, ValidationResult& result);
    void validateImageProperties(const ExifData& exif,
                                 ValidationResult& result);
    void validateGpsCoordinates(const GpsData& gps, ValidationResult& result);
    void validateIptcStrings(const IptcData& iptc, ValidationResult& result);
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_VALIDATOR_HPP
