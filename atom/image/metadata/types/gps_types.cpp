#include "gps_types.hpp"

#include <cmath>
#include <iomanip>
#include <regex>
#include <sstream>

namespace atom::image::metadata {

std::string GpsCoordinate::toString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(0) << degrees << "°"
        << std::setprecision(0) << minutes << "'" << std::setprecision(2)
        << seconds << "\"" << direction;
    return oss.str();
}

std::string GpsCoordinate::toDecimalString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << toDecimalDegrees();
    return oss.str();
}

std::optional<GpsCoordinate> GpsCoordinate::fromString(const std::string& str) {
    // Try parsing DMS format: 37°46'29.64"N or 37 46 29.64 N
    std::regex dmsRegex(R"((\d+)[°\s]+(\d+)['\s]+([0-9.]+)["\s]*([NSEWnsew]))");
    std::smatch match;

    if (std::regex_search(str, match, dmsRegex) && match.size() == 5) {
        GpsCoordinate coord;
        try {
            coord.degrees = std::stod(match[1].str());
            coord.minutes = std::stod(match[2].str());
            coord.seconds = std::stod(match[3].str());
            coord.direction =
                static_cast<char>(std::toupper(match[4].str()[0]));
            return coord;
        } catch (...) {
            return std::nullopt;
        }
    }

    // Try parsing decimal format: 37.7749 or -122.4194
    std::regex decimalRegex(R"((-?\d+\.?\d*))");
    if (std::regex_search(str, match, decimalRegex) && match.size() >= 1) {
        try {
            double decimal = std::stod(match[1].str());
            // Determine if latitude or longitude based on range
            bool isLatitude = std::abs(decimal) <= 90.0;
            return fromDecimalDegrees(decimal, isLatitude);
        } catch (...) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::string GpsTimestamp::toString() const {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << static_cast<int>(hour) << ":"
        << std::setw(2) << static_cast<int>(minute) << ":" << std::fixed
        << std::setprecision(2) << second;
    return oss.str();
}

std::string GpsDate::toString() const {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << year << ":" << std::setw(2)
        << static_cast<int>(month) << ":" << std::setw(2)
        << static_cast<int>(day);
    return oss.str();
}

std::optional<GpsDate> GpsDate::fromString(const std::string& str) {
    // Parse YYYY:MM:DD or YYYY-MM-DD
    std::regex dateRegex(R"((\d{4})[:/-](\d{1,2})[:/-](\d{1,2}))");
    std::smatch match;

    if (std::regex_search(str, match, dateRegex) && match.size() == 4) {
        GpsDate date;
        try {
            date.year = static_cast<uint16_t>(std::stoi(match[1].str()));
            date.month = static_cast<uint8_t>(std::stoi(match[2].str()));
            date.day = static_cast<uint8_t>(std::stoi(match[3].str()));
            if (date.isValid()) {
                return date;
            }
        } catch (...) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::string GpsDirection::toCompassBearing() const {
    const char* directions[] = {"N",  "NNE", "NE", "ENE", "E",  "ESE",
                                "SE", "SSE", "S",  "SSW", "SW", "WSW",
                                "W",  "WNW", "NW", "NNW"};
    double normalized = degrees;
    while (normalized < 0)
        normalized += 360.0;
    while (normalized >= 360.0)
        normalized -= 360.0;

    int index = static_cast<int>((normalized + 11.25) / 22.5) % 16;
    return directions[index];
}

void GpsData::clear() noexcept {
    latitude.reset();
    longitude.reset();
    altitude.reset();
    altitudeRef.reset();
    timestamp.reset();
    datestamp.reset();
    status.reset();
    measureMode.reset();
    dop.reset();
    differential.reset();
    speed.reset();
    track.reset();
    imgDirection.reset();
    destLatitude.reset();
    destLongitude.reset();
    destBearing.reset();
    destDistance.reset();
    destDistanceRef.reset();
    processingMethod.reset();
    areaInformation.reset();
    mapDatum.reset();
    satellites.reset();
    versionId.reset();
}

}  // namespace atom::image::metadata
