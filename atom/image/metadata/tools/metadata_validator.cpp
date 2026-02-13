#include "metadata_validator.hpp"

#include <chrono>
#include <cmath>

namespace atom::image::metadata {

ValidationResult MetadataValidator::validate(const ExifData& exif) {
    ValidationResult result;

    validateTimestamps(exif, result);
    validateCameraSettings(exif, result);
    validateImageProperties(exif, result);

    if (exif.gpsData) {
        auto gpsResult = validate(*exif.gpsData);
        result.issues.insert(result.issues.end(), gpsResult.issues.begin(),
                             gpsResult.issues.end());
        if (!gpsResult.valid)
            result.valid = false;
    }

    return result;
}

ValidationResult MetadataValidator::validate(const GpsData& gps) {
    ValidationResult result;
    validateGpsCoordinates(gps, result);
    return result;
}

ValidationResult MetadataValidator::validate(const IptcData& iptc) {
    ValidationResult result;
    validateIptcStrings(iptc, result);
    return result;
}

ValidationResult MetadataValidator::validate(const XmpData& xmp) {
    ValidationResult result;

    // Check for basic XMP validity
    if (xmp.xmpBasic.rating &&
        (*xmp.xmpBasic.rating < -1 || *xmp.xmpBasic.rating > 5)) {
        result.addIssue(ValidationSeverity::ERROR, "xmp:Rating",
                        "Rating must be between -1 and 5",
                        "Set to valid value");
    }

    return result;
}

int MetadataValidator::autoFix(ExifData& exif) {
    int fixed = 0;

    // Fix orientation if invalid
    if (exif.imageProperties.orientation) {
        int orient = static_cast<int>(*exif.imageProperties.orientation);
        if (orient < 1 || orient > 8) {
            exif.imageProperties.orientation = ExifOrientation::NORMAL;
            ++fixed;
        }
    }

    // Clamp ISO to reasonable range
    if (exif.cameraSettings.isoSpeed) {
        int iso = *exif.cameraSettings.isoSpeed;
        if (iso < 1) {
            exif.cameraSettings.isoSpeed = 100;
            ++fixed;
        } else if (iso > 10000000) {
            exif.cameraSettings.isoSpeed = 10000000;
            ++fixed;
        }
    }

    // Fix f-number
    if (exif.cameraSettings.fNumber) {
        double f = *exif.cameraSettings.fNumber;
        if (f < 0.5) {
            exif.cameraSettings.fNumber = 0.5;
            ++fixed;
        } else if (f > 100) {
            exif.cameraSettings.fNumber = 100;
            ++fixed;
        }
    }

    if (exif.gpsData) {
        fixed += autoFix(*exif.gpsData);
    }

    return fixed;
}

int MetadataValidator::autoFix(GpsData& gps) {
    int fixed = 0;

    // Normalize latitude
    if (gps.latitude) {
        if (gps.latitude->degrees > 90) {
            gps.latitude->degrees = 90;
            gps.latitude->minutes = 0;
            gps.latitude->seconds = 0;
            ++fixed;
        }
    }

    // Normalize longitude
    if (gps.longitude) {
        if (gps.longitude->degrees > 180) {
            gps.longitude->degrees = 180;
            gps.longitude->minutes = 0;
            gps.longitude->seconds = 0;
            ++fixed;
        }
    }

    return fixed;
}

void MetadataValidator::validateTimestamps(const ExifData& exif,
                                           ValidationResult& result) {
    auto now = std::chrono::system_clock::now();

    if (exif.dateTimeOriginal) {
        if (*exif.dateTimeOriginal > now) {
            result.addIssue(ValidationSeverity::WARNING, "DateTimeOriginal",
                            "Date is in the future",
                            "Check camera date settings");
        }

        // Check for dates before photography existed
        auto epoch1800 = std::chrono::system_clock::from_time_t(-5364662400);
        if (*exif.dateTimeOriginal < epoch1800) {
            result.addIssue(ValidationSeverity::ERROR, "DateTimeOriginal",
                            "Date is before 1800", "Invalid timestamp");
        }
    }

    // Check consistency between timestamps
    if (exif.dateTimeOriginal && exif.dateTimeDigitized) {
        if (*exif.dateTimeDigitized < *exif.dateTimeOriginal) {
            result.addIssue(ValidationSeverity::WARNING, "DateTimeDigitized",
                            "Digitized date is before original date",
                            "Timestamps may be inconsistent");
        }
    }
}

void MetadataValidator::validateCameraSettings(const ExifData& exif,
                                               ValidationResult& result) {
    // ISO validation
    if (exif.cameraSettings.isoSpeed) {
        int iso = *exif.cameraSettings.isoSpeed;
        if (iso < 1 || iso > 10000000) {
            result.addIssue(ValidationSeverity::ERROR, "ISOSpeedRatings",
                            "ISO value out of range (1-10000000)",
                            "Check ISO setting");
        }
    }

    // F-number validation
    if (exif.cameraSettings.fNumber) {
        double f = *exif.cameraSettings.fNumber;
        if (f < 0.5 || f > 100) {
            result.addIssue(ValidationSeverity::ERROR, "FNumber",
                            "F-number out of range (0.5-100)",
                            "Check aperture value");
        }
    }

    // Exposure time validation
    if (exif.cameraSettings.exposureTime) {
        double exp = *exif.cameraSettings.exposureTime;
        if (exp <= 0) {
            result.addIssue(ValidationSeverity::ERROR, "ExposureTime",
                            "Exposure time must be positive",
                            "Invalid exposure value");
        } else if (exp > 3600) {
            result.addIssue(ValidationSeverity::WARNING, "ExposureTime",
                            "Exposure time exceeds 1 hour",
                            "Unusually long exposure");
        }
    }

    // Focal length validation
    if (exif.cameraSettings.focalLength) {
        double fl = *exif.cameraSettings.focalLength;
        if (fl <= 0 || fl > 10000) {
            result.addIssue(ValidationSeverity::ERROR, "FocalLength",
                            "Focal length out of range",
                            "Check focal length value");
        }
    }
}

void MetadataValidator::validateImageProperties(const ExifData& exif,
                                                ValidationResult& result) {
    // Orientation validation
    if (exif.imageProperties.orientation) {
        int orient = static_cast<int>(*exif.imageProperties.orientation);
        if (orient < 1 || orient > 8) {
            result.addIssue(ValidationSeverity::ERROR, "Orientation",
                            "Orientation must be between 1 and 8",
                            "Invalid orientation value");
        }
    }

    // Dimension validation
    if (exif.imageProperties.width && *exif.imageProperties.width <= 0) {
        result.addIssue(ValidationSeverity::ERROR, "ImageWidth",
                        "Width must be positive", "Invalid dimension");
    }
    if (exif.imageProperties.height && *exif.imageProperties.height <= 0) {
        result.addIssue(ValidationSeverity::ERROR, "ImageHeight",
                        "Height must be positive", "Invalid dimension");
    }
}

void MetadataValidator::validateGpsCoordinates(const GpsData& gps,
                                               ValidationResult& result) {
    if (gps.latitude) {
        double lat = gps.latitude->toDecimalDegrees();
        if (lat < -90 || lat > 90) {
            result.addIssue(ValidationSeverity::ERROR, "GPSLatitude",
                            "Latitude must be between -90 and 90",
                            "Invalid GPS coordinate");
        }
    }

    if (gps.longitude) {
        double lon = gps.longitude->toDecimalDegrees();
        if (lon < -180 || lon > 180) {
            result.addIssue(ValidationSeverity::ERROR, "GPSLongitude",
                            "Longitude must be between -180 and 180",
                            "Invalid GPS coordinate");
        }
    }

    if (gps.altitude && *gps.altitude < -1000) {
        result.addIssue(ValidationSeverity::WARNING, "GPSAltitude",
                        "Altitude below -1000m seems unlikely",
                        "Check altitude value");
    }
}

void MetadataValidator::validateIptcStrings(const IptcData& iptc,
                                            ValidationResult& result) {
    // Check string length limits per IPTC spec
    if (iptc.headline && iptc.headline->length() > 256) {
        result.addIssue(ValidationSeverity::WARNING, "Headline",
                        "Headline exceeds 256 characters",
                        "Consider shortening");
    }

    if (iptc.caption && iptc.caption->length() > 2000) {
        result.addIssue(ValidationSeverity::WARNING, "Caption",
                        "Caption exceeds 2000 characters",
                        "Consider shortening");
    }

    if (iptc.objectName && iptc.objectName->length() > 64) {
        result.addIssue(ValidationSeverity::WARNING, "ObjectName",
                        "Object name exceeds 64 characters",
                        "Consider shortening");
    }
}

}  // namespace atom::image::metadata
