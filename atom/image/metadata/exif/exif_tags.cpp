#include "exif_tags.hpp"

#include <iomanip>
#include <sstream>

namespace atom::image::metadata {

std::string getTagName(uint16_t tag, IfdType ifdType) {
    if (ifdType == IfdType::GPS_IFD) {
        switch (tag) {
            case GpsTag::VERSION_ID:
                return "GPSVersionID";
            case GpsTag::LATITUDE_REF:
                return "GPSLatitudeRef";
            case GpsTag::LATITUDE:
                return "GPSLatitude";
            case GpsTag::LONGITUDE_REF:
                return "GPSLongitudeRef";
            case GpsTag::LONGITUDE:
                return "GPSLongitude";
            case GpsTag::ALTITUDE_REF:
                return "GPSAltitudeRef";
            case GpsTag::ALTITUDE:
                return "GPSAltitude";
            case GpsTag::TIME_STAMP:
                return "GPSTimeStamp";
            case GpsTag::SATELLITES:
                return "GPSSatellites";
            case GpsTag::STATUS:
                return "GPSStatus";
            case GpsTag::MEASURE_MODE:
                return "GPSMeasureMode";
            case GpsTag::DOP:
                return "GPSDOP";
            case GpsTag::SPEED_REF:
                return "GPSSpeedRef";
            case GpsTag::SPEED:
                return "GPSSpeed";
            case GpsTag::TRACK_REF:
                return "GPSTrackRef";
            case GpsTag::TRACK:
                return "GPSTrack";
            case GpsTag::IMG_DIRECTION_REF:
                return "GPSImgDirectionRef";
            case GpsTag::IMG_DIRECTION:
                return "GPSImgDirection";
            case GpsTag::MAP_DATUM:
                return "GPSMapDatum";
            case GpsTag::DEST_LATITUDE_REF:
                return "GPSDestLatitudeRef";
            case GpsTag::DEST_LATITUDE:
                return "GPSDestLatitude";
            case GpsTag::DEST_LONGITUDE_REF:
                return "GPSDestLongitudeRef";
            case GpsTag::DEST_LONGITUDE:
                return "GPSDestLongitude";
            case GpsTag::DEST_BEARING_REF:
                return "GPSDestBearingRef";
            case GpsTag::DEST_BEARING:
                return "GPSDestBearing";
            case GpsTag::DEST_DISTANCE_REF:
                return "GPSDestDistanceRef";
            case GpsTag::DEST_DISTANCE:
                return "GPSDestDistance";
            case GpsTag::PROCESSING_METHOD:
                return "GPSProcessingMethod";
            case GpsTag::AREA_INFORMATION:
                return "GPSAreaInformation";
            case GpsTag::DATE_STAMP:
                return "GPSDateStamp";
            case GpsTag::DIFFERENTIAL:
                return "GPSDifferential";
            case GpsTag::H_POSITIONING_ERROR:
                return "GPSHPositioningError";
            default:
                break;
        }
    } else if (ifdType == IfdType::INTEROP_IFD) {
        switch (tag) {
            case InteropTag::INTEROPERABILITY_INDEX:
                return "InteroperabilityIndex";
            case InteropTag::INTEROPERABILITY_VERSION:
                return "InteroperabilityVersion";
            case InteropTag::RELATED_IMAGE_FILE_FORMAT:
                return "RelatedImageFileFormat";
            case InteropTag::RELATED_IMAGE_WIDTH:
                return "RelatedImageWidth";
            case InteropTag::RELATED_IMAGE_HEIGHT:
                return "RelatedImageHeight";
            default:
                break;
        }
    }

    // IFD0, IFD1, EXIF IFD tags
    switch (tag) {
        case ExifTag::IMAGE_WIDTH:
            return "ImageWidth";
        case ExifTag::IMAGE_HEIGHT:
            return "ImageHeight";
        case ExifTag::BITS_PER_SAMPLE:
            return "BitsPerSample";
        case ExifTag::COMPRESSION:
            return "Compression";
        case ExifTag::PHOTOMETRIC_INTERPRETATION:
            return "PhotometricInterpretation";
        case ExifTag::IMAGE_DESCRIPTION:
            return "ImageDescription";
        case ExifTag::MAKE:
            return "Make";
        case ExifTag::MODEL:
            return "Model";
        case ExifTag::STRIP_OFFSETS:
            return "StripOffsets";
        case ExifTag::ORIENTATION:
            return "Orientation";
        case ExifTag::SAMPLES_PER_PIXEL:
            return "SamplesPerPixel";
        case ExifTag::ROWS_PER_STRIP:
            return "RowsPerStrip";
        case ExifTag::STRIP_BYTE_COUNTS:
            return "StripByteCounts";
        case ExifTag::X_RESOLUTION:
            return "XResolution";
        case ExifTag::Y_RESOLUTION:
            return "YResolution";
        case ExifTag::PLANAR_CONFIGURATION:
            return "PlanarConfiguration";
        case ExifTag::RESOLUTION_UNIT:
            return "ResolutionUnit";
        case ExifTag::TRANSFER_FUNCTION:
            return "TransferFunction";
        case ExifTag::SOFTWARE:
            return "Software";
        case ExifTag::DATE_TIME:
            return "DateTime";
        case ExifTag::ARTIST:
            return "Artist";
        case ExifTag::WHITE_POINT:
            return "WhitePoint";
        case ExifTag::PRIMARY_CHROMATICITIES:
            return "PrimaryChromaticities";
        case ExifTag::JPEG_INTERCHANGE_FORMAT:
            return "JPEGInterchangeFormat";
        case ExifTag::JPEG_INTERCHANGE_FORMAT_LENGTH:
            return "JPEGInterchangeFormatLength";
        case ExifTag::YCBCR_COEFFICIENTS:
            return "YCbCrCoefficients";
        case ExifTag::YCBCR_SUB_SAMPLING:
            return "YCbCrSubSampling";
        case ExifTag::YCBCR_POSITIONING:
            return "YCbCrPositioning";
        case ExifTag::REFERENCE_BLACK_WHITE:
            return "ReferenceBlackWhite";
        case ExifTag::COPYRIGHT:
            return "Copyright";
        case ExifTag::EXIF_IFD_POINTER:
            return "ExifIFDPointer";
        case ExifTag::GPS_IFD_POINTER:
            return "GPSIFDPointer";
        case ExifTag::INTEROP_IFD_POINTER:
            return "InteroperabilityIFDPointer";
        case ExifTag::EXPOSURE_TIME:
            return "ExposureTime";
        case ExifTag::F_NUMBER:
            return "FNumber";
        case ExifTag::EXPOSURE_PROGRAM:
            return "ExposureProgram";
        case ExifTag::SPECTRAL_SENSITIVITY:
            return "SpectralSensitivity";
        case ExifTag::ISO_SPEED_RATINGS:
            return "ISOSpeedRatings";
        case ExifTag::OECF:
            return "OECF";
        case ExifTag::SENSITIVITY_TYPE:
            return "SensitivityType";
        case ExifTag::STANDARD_OUTPUT_SENSITIVITY:
            return "StandardOutputSensitivity";
        case ExifTag::RECOMMENDED_EXPOSURE_INDEX:
            return "RecommendedExposureIndex";
        case ExifTag::ISO_SPEED:
            return "ISOSpeed";
        case ExifTag::EXIF_VERSION:
            return "ExifVersion";
        case ExifTag::DATE_TIME_ORIGINAL:
            return "DateTimeOriginal";
        case ExifTag::DATE_TIME_DIGITIZED:
            return "DateTimeDigitized";
        case ExifTag::OFFSET_TIME:
            return "OffsetTime";
        case ExifTag::OFFSET_TIME_ORIGINAL:
            return "OffsetTimeOriginal";
        case ExifTag::OFFSET_TIME_DIGITIZED:
            return "OffsetTimeDigitized";
        case ExifTag::COMPONENTS_CONFIGURATION:
            return "ComponentsConfiguration";
        case ExifTag::COMPRESSED_BITS_PER_PIXEL:
            return "CompressedBitsPerPixel";
        case ExifTag::SHUTTER_SPEED_VALUE:
            return "ShutterSpeedValue";
        case ExifTag::APERTURE_VALUE:
            return "ApertureValue";
        case ExifTag::BRIGHTNESS_VALUE:
            return "BrightnessValue";
        case ExifTag::EXPOSURE_BIAS_VALUE:
            return "ExposureBiasValue";
        case ExifTag::MAX_APERTURE_VALUE:
            return "MaxApertureValue";
        case ExifTag::SUBJECT_DISTANCE:
            return "SubjectDistance";
        case ExifTag::METERING_MODE:
            return "MeteringMode";
        case ExifTag::LIGHT_SOURCE:
            return "LightSource";
        case ExifTag::FLASH:
            return "Flash";
        case ExifTag::FOCAL_LENGTH:
            return "FocalLength";
        case ExifTag::SUBJECT_AREA:
            return "SubjectArea";
        case ExifTag::MAKER_NOTE:
            return "MakerNote";
        case ExifTag::USER_COMMENT:
            return "UserComment";
        case ExifTag::SUB_SEC_TIME:
            return "SubSecTime";
        case ExifTag::SUB_SEC_TIME_ORIGINAL:
            return "SubSecTimeOriginal";
        case ExifTag::SUB_SEC_TIME_DIGITIZED:
            return "SubSecTimeDigitized";
        case ExifTag::FLASHPIX_VERSION:
            return "FlashpixVersion";
        case ExifTag::COLOR_SPACE:
            return "ColorSpace";
        case ExifTag::PIXEL_X_DIMENSION:
            return "PixelXDimension";
        case ExifTag::PIXEL_Y_DIMENSION:
            return "PixelYDimension";
        case ExifTag::RELATED_SOUND_FILE:
            return "RelatedSoundFile";
        case ExifTag::FLASH_ENERGY:
            return "FlashEnergy";
        case ExifTag::SPATIAL_FREQUENCY_RESPONSE:
            return "SpatialFrequencyResponse";
        case ExifTag::FOCAL_PLANE_X_RESOLUTION:
            return "FocalPlaneXResolution";
        case ExifTag::FOCAL_PLANE_Y_RESOLUTION:
            return "FocalPlaneYResolution";
        case ExifTag::FOCAL_PLANE_RESOLUTION_UNIT:
            return "FocalPlaneResolutionUnit";
        case ExifTag::SUBJECT_LOCATION:
            return "SubjectLocation";
        case ExifTag::EXPOSURE_INDEX:
            return "ExposureIndex";
        case ExifTag::SENSING_METHOD:
            return "SensingMethod";
        case ExifTag::FILE_SOURCE:
            return "FileSource";
        case ExifTag::SCENE_TYPE:
            return "SceneType";
        case ExifTag::CFA_PATTERN:
            return "CFAPattern";
        case ExifTag::CUSTOM_RENDERED:
            return "CustomRendered";
        case ExifTag::EXPOSURE_MODE:
            return "ExposureMode";
        case ExifTag::WHITE_BALANCE:
            return "WhiteBalance";
        case ExifTag::DIGITAL_ZOOM_RATIO:
            return "DigitalZoomRatio";
        case ExifTag::FOCAL_LENGTH_IN_35MM_FILM:
            return "FocalLengthIn35mmFilm";
        case ExifTag::SCENE_CAPTURE_TYPE:
            return "SceneCaptureType";
        case ExifTag::GAIN_CONTROL:
            return "GainControl";
        case ExifTag::CONTRAST:
            return "Contrast";
        case ExifTag::SATURATION:
            return "Saturation";
        case ExifTag::SHARPNESS:
            return "Sharpness";
        case ExifTag::DEVICE_SETTING_DESCRIPTION:
            return "DeviceSettingDescription";
        case ExifTag::SUBJECT_DISTANCE_RANGE:
            return "SubjectDistanceRange";
        case ExifTag::IMAGE_UNIQUE_ID:
            return "ImageUniqueID";
        case ExifTag::CAMERA_OWNER_NAME:
            return "CameraOwnerName";
        case ExifTag::BODY_SERIAL_NUMBER:
            return "BodySerialNumber";
        case ExifTag::LENS_SPECIFICATION:
            return "LensSpecification";
        case ExifTag::LENS_MAKE:
            return "LensMake";
        case ExifTag::LENS_MODEL:
            return "LensModel";
        case ExifTag::LENS_SERIAL_NUMBER:
            return "LensSerialNumber";
        case ExifTag::GAMMA:
            return "Gamma";
        default:
            break;
    }

    // Unknown tag
    std::ostringstream oss;
    oss << "Tag_0x" << std::hex << std::uppercase << std::setw(4)
        << std::setfill('0') << tag;
    return oss.str();
}

std::string getTagDescription(uint16_t tag, IfdType ifdType) {
    // Simplified - return tag name as description
    return getTagName(tag, ifdType);
}

std::string getOrientationDescription(int orientation) {
    switch (orientation) {
        case 1:
            return "Normal";
        case 2:
            return "Flipped horizontally";
        case 3:
            return "Rotated 180°";
        case 4:
            return "Flipped vertically";
        case 5:
            return "Rotated 90° CCW and flipped vertically";
        case 6:
            return "Rotated 90° CW";
        case 7:
            return "Rotated 90° CW and flipped vertically";
        case 8:
            return "Rotated 90° CCW";
        default:
            return "Unknown";
    }
}

std::string getMeteringModeDescription(int mode) {
    switch (mode) {
        case 0:
            return "Unknown";
        case 1:
            return "Average";
        case 2:
            return "Center-weighted average";
        case 3:
            return "Spot";
        case 4:
            return "Multi-spot";
        case 5:
            return "Pattern";
        case 6:
            return "Partial";
        case 255:
            return "Other";
        default:
            return "Unknown";
    }
}

std::string getExposureProgramDescription(int program) {
    switch (program) {
        case 0:
            return "Not defined";
        case 1:
            return "Manual";
        case 2:
            return "Normal program";
        case 3:
            return "Aperture priority";
        case 4:
            return "Shutter priority";
        case 5:
            return "Creative program";
        case 6:
            return "Action program";
        case 7:
            return "Portrait mode";
        case 8:
            return "Landscape mode";
        default:
            return "Unknown";
    }
}

std::string getFlashDescription(int flash) {
    std::string result;

    if ((flash & 0x01) == 0) {
        result = "Flash did not fire";
    } else {
        result = "Flash fired";
    }

    int returnMode = (flash >> 1) & 0x03;
    switch (returnMode) {
        case 2:
            result += ", return not detected";
            break;
        case 3:
            result += ", return detected";
            break;
        default:
            break;
    }

    int flashMode = (flash >> 3) & 0x03;
    switch (flashMode) {
        case 1:
            result += ", compulsory flash mode";
            break;
        case 2:
            result += ", compulsory flash suppression";
            break;
        case 3:
            result += ", auto mode";
            break;
        default:
            break;
    }

    if ((flash >> 5) & 0x01) {
        result += ", no flash function";
    }

    if ((flash >> 6) & 0x01) {
        result += ", red-eye reduction";
    }

    return result;
}

std::string getColorSpaceDescription(int colorSpace) {
    switch (colorSpace) {
        case 1:
            return "sRGB";
        case 2:
            return "Adobe RGB";
        case 0xFFFF:
            return "Uncalibrated";
        default:
            return "Unknown";
    }
}

std::string getWhiteBalanceDescription(int whiteBalance) {
    switch (whiteBalance) {
        case 0:
            return "Auto";
        case 1:
            return "Manual";
        default:
            return "Unknown";
    }
}

std::string getLightSourceDescription(int lightSource) {
    switch (lightSource) {
        case 0:
            return "Unknown";
        case 1:
            return "Daylight";
        case 2:
            return "Fluorescent";
        case 3:
            return "Tungsten (incandescent)";
        case 4:
            return "Flash";
        case 9:
            return "Fine weather";
        case 10:
            return "Cloudy weather";
        case 11:
            return "Shade";
        case 12:
            return "Daylight fluorescent (D 5700-7100K)";
        case 13:
            return "Day white fluorescent (N 4600-5500K)";
        case 14:
            return "Cool white fluorescent (W 3800-4500K)";
        case 15:
            return "White fluorescent (WW 3250-3800K)";
        case 16:
            return "Warm white fluorescent (L 2600-3250K)";
        case 17:
            return "Standard light A";
        case 18:
            return "Standard light B";
        case 19:
            return "Standard light C";
        case 20:
            return "D55";
        case 21:
            return "D65";
        case 22:
            return "D75";
        case 23:
            return "D50";
        case 24:
            return "ISO studio tungsten";
        case 255:
            return "Other";
        default:
            return "Unknown";
    }
}

std::string getCompressionDescription(int compression) {
    switch (compression) {
        case 1:
            return "Uncompressed";
        case 2:
            return "CCITT 1D";
        case 3:
            return "T4/Group 3 Fax";
        case 4:
            return "T6/Group 4 Fax";
        case 5:
            return "LZW";
        case 6:
            return "JPEG (old-style)";
        case 7:
            return "JPEG";
        case 8:
            return "Adobe Deflate";
        case 32773:
            return "PackBits";
        default:
            return "Unknown";
    }
}

std::string getResolutionUnitDescription(int unit) {
    switch (unit) {
        case 1:
            return "No unit";
        case 2:
            return "inches";
        case 3:
            return "centimeters";
        default:
            return "Unknown";
    }
}

std::string formatExposureTime(double seconds) {
    if (seconds >= 1.0) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << seconds << " sec";
        return oss.str();
    }

    // Express as fraction
    double reciprocal = 1.0 / seconds;
    int denom = static_cast<int>(reciprocal + 0.5);

    std::ostringstream oss;
    oss << "1/" << denom << " sec";
    return oss.str();
}

std::string formatFocalLength(double mm) {
    std::ostringstream oss;
    if (mm == static_cast<int>(mm)) {
        oss << static_cast<int>(mm) << " mm";
    } else {
        oss << std::fixed << std::setprecision(1) << mm << " mm";
    }
    return oss.str();
}

std::string formatFNumber(double f) {
    std::ostringstream oss;
    if (f == static_cast<int>(f)) {
        oss << "f/" << static_cast<int>(f);
    } else {
        oss << "f/" << std::fixed << std::setprecision(1) << f;
    }
    return oss.str();
}

}  // namespace atom::image::metadata
