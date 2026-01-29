#ifndef ATOM_IMAGE_METADATA_GPS_TYPES_HPP
#define ATOM_IMAGE_METADATA_GPS_TYPES_HPP

#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

namespace atom::image::metadata {

/**
 * @brief GPS latitude/longitude reference
 */
enum class GpsLatitudeRef : char { NORTH = 'N', SOUTH = 'S' };

enum class GpsLongitudeRef : char { EAST = 'E', WEST = 'W' };

/**
 * @brief GPS altitude reference
 */
enum class GpsAltitudeRef : uint8_t {
    ABOVE_SEA_LEVEL = 0,
    BELOW_SEA_LEVEL = 1
};

/**
 * @brief GPS speed unit
 */
enum class GpsSpeedRef : char {
    KILOMETERS_PER_HOUR = 'K',
    MILES_PER_HOUR = 'M',
    KNOTS = 'N'
};

/**
 * @brief GPS distance unit
 */
enum class GpsDistanceRef : char {
    KILOMETERS = 'K',
    MILES = 'M',
    NAUTICAL_MILES = 'N'
};

/**
 * @brief GPS direction reference
 */
enum class GpsDirectionRef : char {
    TRUE_DIRECTION = 'T',
    MAGNETIC_DIRECTION = 'M'
};

/**
 * @brief GPS measurement mode
 */
enum class GpsMeasureMode : char {
    TWO_DIMENSIONAL = '2',
    THREE_DIMENSIONAL = '3'
};

/**
 * @brief GPS status
 */
enum class GpsStatus : char {
    MEASUREMENT_IN_PROGRESS = 'A',
    MEASUREMENT_INTEROPERABILITY = 'V'
};

/**
 * @brief GPS differential correction
 */
enum class GpsDifferential : uint16_t {
    WITHOUT_DIFFERENTIAL = 0,
    DIFFERENTIAL_APPLIED = 1
};

/**
 * @brief Structure to represent GPS coordinate data (latitude or longitude)
 */
struct GpsCoordinate {
    double degrees = 0.0;
    double minutes = 0.0;
    double seconds = 0.0;
    char direction = 'N';  ///< N, S, E, or W

    /**
     * @brief Default constructor
     */
    GpsCoordinate() = default;

    /**
     * @brief Constructor with values
     */
    GpsCoordinate(double deg, double min, double sec, char dir)
        : degrees(deg), minutes(min), seconds(sec), direction(dir) {}

    /**
     * @brief Convert to decimal degrees
     * @return Decimal degree representation (negative for S/W)
     */
    [[nodiscard]] double toDecimalDegrees() const noexcept {
        double value = std::abs(degrees) + std::abs(minutes) / 60.0 +
                       std::abs(seconds) / 3600.0;
        return (direction == 'S' || direction == 'W') ? -value : value;
    }

    /**
     * @brief Create from decimal degrees
     * @param decimal Decimal degree value
     * @param isLatitude True if latitude, false if longitude
     * @return GpsCoordinate instance
     */
    [[nodiscard]] static GpsCoordinate fromDecimalDegrees(
        double decimal, bool isLatitude) noexcept {
        GpsCoordinate coord;

        if (isLatitude) {
            coord.direction = decimal >= 0 ? 'N' : 'S';
        } else {
            coord.direction = decimal >= 0 ? 'E' : 'W';
        }

        double absDecimal = std::abs(decimal);
        coord.degrees = std::floor(absDecimal);

        double remaining = (absDecimal - coord.degrees) * 60.0;
        coord.minutes = std::floor(remaining);
        coord.seconds = (remaining - coord.minutes) * 60.0;

        return coord;
    }

    /**
     * @brief Convert to string representation (DMS format)
     * @return String like "37°46'29.64\"N"
     */
    [[nodiscard]] std::string toString() const;

    /**
     * @brief Convert to string representation (decimal degrees)
     * @return String like "37.7749"
     */
    [[nodiscard]] std::string toDecimalString() const;

    /**
     * @brief Parse from DMS string
     * @param str String in format "37°46'29.64\"N" or similar
     * @return Parsed coordinate or nullopt on failure
     */
    [[nodiscard]] static std::optional<GpsCoordinate> fromString(
        const std::string& str);

    /**
     * @brief Check if coordinate is valid
     */
    [[nodiscard]] bool isValid() const noexcept {
        if (direction == 'N' || direction == 'S') {
            // Latitude: -90 to +90
            double dec = std::abs(toDecimalDegrees());
            return dec <= 90.0;
        } else if (direction == 'E' || direction == 'W') {
            // Longitude: -180 to +180
            double dec = std::abs(toDecimalDegrees());
            return dec <= 180.0;
        }
        return false;
    }

    bool operator==(const GpsCoordinate& other) const noexcept {
        constexpr double EPSILON = 1e-9;
        return std::abs(degrees - other.degrees) < EPSILON &&
               std::abs(minutes - other.minutes) < EPSILON &&
               std::abs(seconds - other.seconds) < EPSILON &&
               direction == other.direction;
    }
};

/**
 * @brief GPS timestamp structure
 */
struct GpsTimestamp {
    uint8_t hour = 0;
    uint8_t minute = 0;
    double second = 0.0;

    /**
     * @brief Convert to string (HH:MM:SS format)
     */
    [[nodiscard]] std::string toString() const;

    /**
     * @brief Convert to total seconds since midnight
     */
    [[nodiscard]] double toTotalSeconds() const noexcept {
        return hour * 3600.0 + minute * 60.0 + second;
    }

    /**
     * @brief Create from total seconds since midnight
     */
    [[nodiscard]] static GpsTimestamp fromTotalSeconds(
        double seconds) noexcept {
        GpsTimestamp ts;
        ts.hour = static_cast<uint8_t>(static_cast<int>(seconds) / 3600);
        seconds -= ts.hour * 3600;
        ts.minute = static_cast<uint8_t>(static_cast<int>(seconds) / 60);
        ts.second = seconds - ts.minute * 60;
        return ts;
    }
};

/**
 * @brief GPS date structure
 */
struct GpsDate {
    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;

    /**
     * @brief Convert to string (YYYY:MM:DD format)
     */
    [[nodiscard]] std::string toString() const;

    /**
     * @brief Parse from EXIF GPS date format (YYYY:MM:DD)
     */
    [[nodiscard]] static std::optional<GpsDate> fromString(
        const std::string& str);

    /**
     * @brief Check if date is valid
     */
    [[nodiscard]] bool isValid() const noexcept {
        return year >= 1 && month >= 1 && month <= 12 && day >= 1 && day <= 31;
    }
};

/**
 * @brief GPS direction (bearing/heading)
 */
struct GpsDirection {
    double degrees = 0.0;  ///< Direction in degrees (0-359.99)
    GpsDirectionRef reference = GpsDirectionRef::TRUE_DIRECTION;

    /**
     * @brief Normalize direction to 0-360 range
     */
    void normalize() noexcept {
        while (degrees < 0)
            degrees += 360.0;
        while (degrees >= 360.0)
            degrees -= 360.0;
    }

    /**
     * @brief Convert to compass bearing string
     */
    [[nodiscard]] std::string toCompassBearing() const;
};

/**
 * @brief GPS speed information
 */
struct GpsSpeed {
    double value = 0.0;
    GpsSpeedRef unit = GpsSpeedRef::KILOMETERS_PER_HOUR;

    /**
     * @brief Convert to kilometers per hour
     */
    [[nodiscard]] double toKilometersPerHour() const noexcept {
        switch (unit) {
            case GpsSpeedRef::KILOMETERS_PER_HOUR:
                return value;
            case GpsSpeedRef::MILES_PER_HOUR:
                return value * 1.60934;
            case GpsSpeedRef::KNOTS:
                return value * 1.852;
            default:
                return value;
        }
    }

    /**
     * @brief Convert to miles per hour
     */
    [[nodiscard]] double toMilesPerHour() const noexcept {
        return toKilometersPerHour() / 1.60934;
    }

    /**
     * @brief Convert to knots
     */
    [[nodiscard]] double toKnots() const noexcept {
        return toKilometersPerHour() / 1.852;
    }
};

/**
 * @brief Complete GPS data structure
 */
struct GpsData {
    // Position
    std::optional<GpsCoordinate> latitude;
    std::optional<GpsCoordinate> longitude;
    std::optional<double> altitude;
    std::optional<GpsAltitudeRef> altitudeRef;

    // Time and date
    std::optional<GpsTimestamp> timestamp;
    std::optional<GpsDate> datestamp;

    // Status and accuracy
    std::optional<GpsStatus> status;
    std::optional<GpsMeasureMode> measureMode;
    std::optional<double> dop;  ///< Degree of precision
    std::optional<GpsDifferential> differential;

    // Movement
    std::optional<GpsSpeed> speed;
    std::optional<GpsDirection> track;         ///< Direction of movement
    std::optional<GpsDirection> imgDirection;  ///< Direction of image

    // Destination
    std::optional<GpsCoordinate> destLatitude;
    std::optional<GpsCoordinate> destLongitude;
    std::optional<GpsDirection> destBearing;
    std::optional<double> destDistance;
    std::optional<GpsDistanceRef> destDistanceRef;

    // Processing
    std::optional<std::string> processingMethod;
    std::optional<std::string> areaInformation;
    std::optional<std::string> mapDatum;

    // Satellites
    std::optional<std::string> satellites;

    // Version
    std::optional<std::string> versionId;

    /**
     * @brief Check if basic position data is available
     */
    [[nodiscard]] bool hasPosition() const noexcept {
        return latitude.has_value() && longitude.has_value();
    }

    /**
     * @brief Get latitude as decimal degrees
     */
    [[nodiscard]] std::optional<double> getLatitudeDecimal() const {
        if (latitude.has_value()) {
            return latitude->toDecimalDegrees();
        }
        return std::nullopt;
    }

    /**
     * @brief Get longitude as decimal degrees
     */
    [[nodiscard]] std::optional<double> getLongitudeDecimal() const {
        if (longitude.has_value()) {
            return longitude->toDecimalDegrees();
        }
        return std::nullopt;
    }

    /**
     * @brief Get altitude in meters (considering reference)
     */
    [[nodiscard]] std::optional<double> getAltitudeMeters() const {
        if (!altitude.has_value()) {
            return std::nullopt;
        }
        double alt = *altitude;
        if (altitudeRef.has_value() &&
            *altitudeRef == GpsAltitudeRef::BELOW_SEA_LEVEL) {
            alt = -alt;
        }
        return alt;
    }

    /**
     * @brief Get combined date and time
     */
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point>
    getDateTime() const;

    /**
     * @brief Set position from decimal degrees
     */
    void setPosition(double lat, double lon) {
        latitude = GpsCoordinate::fromDecimalDegrees(lat, true);
        longitude = GpsCoordinate::fromDecimalDegrees(lon, false);
    }

    /**
     * @brief Calculate distance to another GPS position (in meters)
     * Uses Haversine formula
     */
    [[nodiscard]] std::optional<double> distanceTo(const GpsData& other) const;

    /**
     * @brief Calculate bearing to another GPS position (in degrees)
     */
    [[nodiscard]] std::optional<double> bearingTo(const GpsData& other) const;

    /**
     * @brief Clear all GPS data
     */
    void clear() noexcept;
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_GPS_TYPES_HPP
