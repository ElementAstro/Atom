#include "gps_parser.hpp"

#include <cmath>
#include <sstream>

#include "exif_tags.hpp"

namespace atom::image::metadata {

namespace {

constexpr double EARTH_RADIUS_METERS = 6371000.0;
constexpr double DEG_TO_RAD = 3.14159265358979323846 / 180.0;
constexpr double RAD_TO_DEG = 180.0 / 3.14159265358979323846;

}  // namespace

std::shared_ptr<GpsData> GpsParser::parse(const IfdParseResult& ifdResult) {
    auto gpsData = std::make_shared<GpsData>();

    // Parse latitude
    auto latRationals = getRationalArray(ifdResult, GpsTag::LATITUDE);
    if (!latRationals.empty()) {
        char latRef = getRefChar(ifdResult, GpsTag::LATITUDE_REF, 'N');
        gpsData->latitude = parseCoordinate(latRationals, latRef);
    }

    // Parse longitude
    auto lonRationals = getRationalArray(ifdResult, GpsTag::LONGITUDE);
    if (!lonRationals.empty()) {
        char lonRef = getRefChar(ifdResult, GpsTag::LONGITUDE_REF, 'E');
        gpsData->longitude = parseCoordinate(lonRationals, lonRef);
    }

    // Parse altitude
    if (auto alt = getRationalValue(ifdResult, GpsTag::ALTITUDE)) {
        gpsData->altitude = *alt;
        if (auto altRefInt = ifdResult.getInt(GpsTag::ALTITUDE_REF)) {
            gpsData->altitudeRef = (*altRefInt == 0)
                                       ? GpsAltitudeRef::ABOVE_SEA_LEVEL
                                       : GpsAltitudeRef::BELOW_SEA_LEVEL;
        }
    }

    // Parse timestamp
    auto timeRationals = getRationalArray(ifdResult, GpsTag::TIME_STAMP);
    if (!timeRationals.empty()) {
        gpsData->timestamp = parseTimestamp(timeRationals);
    }

    // Parse datestamp
    if (auto dateStr = getStringValue(ifdResult, GpsTag::DATE_STAMP)) {
        gpsData->datestamp = parseDate(*dateStr);
    }

    // Parse status
    if (auto status = getStringValue(ifdResult, GpsTag::STATUS)) {
        if (!status->empty()) {
            gpsData->status = static_cast<GpsStatus>((*status)[0]);
        }
    }

    // Parse measure mode
    if (auto mode = getStringValue(ifdResult, GpsTag::MEASURE_MODE)) {
        if (!mode->empty()) {
            gpsData->measureMode = static_cast<GpsMeasureMode>((*mode)[0]);
        }
    }

    // Parse DOP (Dilution of Precision)
    if (auto dop = getRationalValue(ifdResult, GpsTag::DOP)) {
        gpsData->dop = *dop;
    }

    // Parse speed
    if (auto speed = getRationalValue(ifdResult, GpsTag::SPEED)) {
        char speedRef = getRefChar(ifdResult, GpsTag::SPEED_REF, 'K');
        gpsData->speed = parseSpeed(*speed, speedRef);
    }

    // Parse track (direction of movement)
    if (auto track = getRationalValue(ifdResult, GpsTag::TRACK)) {
        char trackRef = getRefChar(ifdResult, GpsTag::TRACK_REF, 'T');
        gpsData->track = parseDirection(*track, trackRef);
    }

    // Parse image direction
    if (auto imgDir = getRationalValue(ifdResult, GpsTag::IMG_DIRECTION)) {
        char imgDirRef = getRefChar(ifdResult, GpsTag::IMG_DIRECTION_REF, 'T');
        gpsData->imgDirection = parseDirection(*imgDir, imgDirRef);
    }

    // Parse destination latitude
    auto destLatRationals = getRationalArray(ifdResult, GpsTag::DEST_LATITUDE);
    if (!destLatRationals.empty()) {
        char destLatRef = getRefChar(ifdResult, GpsTag::DEST_LATITUDE_REF, 'N');
        gpsData->destLatitude = parseCoordinate(destLatRationals, destLatRef);
    }

    // Parse destination longitude
    auto destLonRationals = getRationalArray(ifdResult, GpsTag::DEST_LONGITUDE);
    if (!destLonRationals.empty()) {
        char destLonRef =
            getRefChar(ifdResult, GpsTag::DEST_LONGITUDE_REF, 'E');
        gpsData->destLongitude = parseCoordinate(destLonRationals, destLonRef);
    }

    // Parse destination bearing
    if (auto destBearing = getRationalValue(ifdResult, GpsTag::DEST_BEARING)) {
        char bearingRef = getRefChar(ifdResult, GpsTag::DEST_BEARING_REF, 'T');
        gpsData->destBearing = parseDirection(*destBearing, bearingRef);
    }

    // Parse destination distance
    if (auto destDist = getRationalValue(ifdResult, GpsTag::DEST_DISTANCE)) {
        gpsData->destDistance = *destDist;
        if (auto distRefStr =
                getStringValue(ifdResult, GpsTag::DEST_DISTANCE_REF)) {
            if (!distRefStr->empty()) {
                gpsData->destDistanceRef =
                    static_cast<GpsDistanceRef>((*distRefStr)[0]);
            }
        }
    }

    // Parse processing method
    gpsData->processingMethod =
        getStringValue(ifdResult, GpsTag::PROCESSING_METHOD);

    // Parse area information
    gpsData->areaInformation =
        getStringValue(ifdResult, GpsTag::AREA_INFORMATION);

    // Parse map datum
    gpsData->mapDatum = getStringValue(ifdResult, GpsTag::MAP_DATUM);

    // Parse satellites
    gpsData->satellites = getStringValue(ifdResult, GpsTag::SATELLITES);

    // Parse differential
    if (auto diff = ifdResult.getInt(GpsTag::DIFFERENTIAL)) {
        gpsData->differential = (*diff == 0)
                                    ? GpsDifferential::WITHOUT_DIFFERENTIAL
                                    : GpsDifferential::DIFFERENTIAL_APPLIED;
    }

    // Parse version ID
    if (const auto* entry = ifdResult.findEntry(GpsTag::VERSION_ID)) {
        if (!entry->byteArray.empty() && entry->byteArray.size() >= 4) {
            std::ostringstream oss;
            oss << static_cast<int>(entry->byteArray[0]) << "."
                << static_cast<int>(entry->byteArray[1]) << "."
                << static_cast<int>(entry->byteArray[2]) << "."
                << static_cast<int>(entry->byteArray[3]);
            gpsData->versionId = oss.str();
        }
    }

    return gpsData;
}

std::optional<GpsCoordinate> GpsParser::parseCoordinate(
    const std::vector<Rational>& rationals, char ref) {
    if (rationals.size() < 3) {
        return std::nullopt;
    }

    GpsCoordinate coord;
    coord.degrees = rationals[0].toDouble();
    coord.minutes = rationals[1].toDouble();
    coord.seconds = rationals[2].toDouble();
    coord.direction = ref;

    return coord;
}

std::optional<GpsTimestamp> GpsParser::parseTimestamp(
    const std::vector<Rational>& rationals) {
    if (rationals.size() < 3) {
        return std::nullopt;
    }

    GpsTimestamp ts;
    ts.hour = static_cast<uint8_t>(rationals[0].toDouble());
    ts.minute = static_cast<uint8_t>(rationals[1].toDouble());
    ts.second = rationals[2].toDouble();

    return ts;
}

std::optional<GpsDate> GpsParser::parseDate(const std::string& dateStr) {
    // Format: "YYYY:MM:DD"
    if (dateStr.size() < 10) {
        return std::nullopt;
    }

    GpsDate date;
    try {
        date.year = static_cast<uint16_t>(std::stoi(dateStr.substr(0, 4)));
        date.month = static_cast<uint8_t>(std::stoi(dateStr.substr(5, 2)));
        date.day = static_cast<uint8_t>(std::stoi(dateStr.substr(8, 2)));
    } catch (...) {
        return std::nullopt;
    }

    if (!date.isValid()) {
        return std::nullopt;
    }

    return date;
}

GpsDirection GpsParser::parseDirection(double value, char ref) {
    GpsDirection dir;
    dir.degrees = value;
    dir.reference = (ref == 'M') ? GpsDirectionRef::MAGNETIC_DIRECTION
                                 : GpsDirectionRef::TRUE_DIRECTION;
    dir.normalize();
    return dir;
}

GpsSpeed GpsParser::parseSpeed(double value, char ref) {
    GpsSpeed speed;
    speed.value = value;
    switch (ref) {
        case 'M':
            speed.unit = GpsSpeedRef::MILES_PER_HOUR;
            break;
        case 'N':
            speed.unit = GpsSpeedRef::KNOTS;
            break;
        case 'K':
        default:
            speed.unit = GpsSpeedRef::KILOMETERS_PER_HOUR;
            break;
    }
    return speed;
}

std::vector<IfdEntry> GpsParser::toIfdEntries(const GpsData& gpsData) {
    std::vector<IfdEntry> entries;

    // GPS Version ID (2.3.0.0)
    {
        IfdEntry entry;
        entry.tag = GpsTag::VERSION_ID;
        entry.type = ExifDataType::BYTE;
        entry.count = 4;
        std::vector<uint8_t> version = {2, 3, 0, 0};
        entry.value = version;
        entries.push_back(entry);
    }

    // Latitude
    if (gpsData.latitude.has_value()) {
        const auto& lat = *gpsData.latitude;

        // Latitude Ref
        {
            IfdEntry entry;
            entry.tag = GpsTag::LATITUDE_REF;
            entry.type = ExifDataType::ASCII;
            entry.count = 2;
            entry.value = std::string(1, lat.direction);
            entries.push_back(entry);
        }

        // Latitude value
        {
            IfdEntry entry;
            entry.tag = GpsTag::LATITUDE;
            entry.type = ExifDataType::RATIONAL;
            entry.count = 3;
            std::vector<Rational> rationals = {
                Rational::fromDouble(std::abs(lat.degrees), 1),
                Rational::fromDouble(std::abs(lat.minutes), 1),
                Rational::fromDouble(std::abs(lat.seconds), 10000)};
            entry.value = rationals;
            entries.push_back(entry);
        }
    }

    // Longitude
    if (gpsData.longitude.has_value()) {
        const auto& lon = *gpsData.longitude;

        // Longitude Ref
        {
            IfdEntry entry;
            entry.tag = GpsTag::LONGITUDE_REF;
            entry.type = ExifDataType::ASCII;
            entry.count = 2;
            entry.value = std::string(1, lon.direction);
            entries.push_back(entry);
        }

        // Longitude value
        {
            IfdEntry entry;
            entry.tag = GpsTag::LONGITUDE;
            entry.type = ExifDataType::RATIONAL;
            entry.count = 3;
            std::vector<Rational> rationals = {
                Rational::fromDouble(std::abs(lon.degrees), 1),
                Rational::fromDouble(std::abs(lon.minutes), 1),
                Rational::fromDouble(std::abs(lon.seconds), 10000)};
            entry.value = rationals;
            entries.push_back(entry);
        }
    }

    // Altitude
    if (gpsData.altitude.has_value()) {
        // Altitude Ref
        {
            IfdEntry entry;
            entry.tag = GpsTag::ALTITUDE_REF;
            entry.type = ExifDataType::BYTE;
            entry.count = 1;
            uint8_t ref = (gpsData.altitudeRef.value_or(
                               GpsAltitudeRef::ABOVE_SEA_LEVEL) ==
                           GpsAltitudeRef::ABOVE_SEA_LEVEL)
                              ? 0
                              : 1;
            entry.value = ref;
            entries.push_back(entry);
        }

        // Altitude value
        {
            IfdEntry entry;
            entry.tag = GpsTag::ALTITUDE;
            entry.type = ExifDataType::RATIONAL;
            entry.count = 1;
            entry.value =
                Rational::fromDouble(std::abs(*gpsData.altitude), 100);
            entries.push_back(entry);
        }
    }

    // Timestamp
    if (gpsData.timestamp.has_value()) {
        const auto& ts = *gpsData.timestamp;
        IfdEntry entry;
        entry.tag = GpsTag::TIME_STAMP;
        entry.type = ExifDataType::RATIONAL;
        entry.count = 3;
        std::vector<Rational> rationals = {
            {ts.hour, 1},
            {ts.minute, 1},
            Rational::fromDouble(ts.second, 1000)};
        entry.value = rationals;
        entries.push_back(entry);
    }

    // Datestamp
    if (gpsData.datestamp.has_value()) {
        IfdEntry entry;
        entry.tag = GpsTag::DATE_STAMP;
        entry.type = ExifDataType::ASCII;
        entry.value = gpsData.datestamp->toString();
        entry.count = static_cast<uint32_t>(
            std::get<std::string>(entry.value).size() + 1);
        entries.push_back(entry);
    }

    return entries;
}

double GpsParser::calculateDistance(double lat1, double lon1, double lat2,
                                    double lon2) {
    double lat1Rad = lat1 * DEG_TO_RAD;
    double lat2Rad = lat2 * DEG_TO_RAD;
    double deltaLat = (lat2 - lat1) * DEG_TO_RAD;
    double deltaLon = (lon2 - lon1) * DEG_TO_RAD;

    double a = std::sin(deltaLat / 2) * std::sin(deltaLat / 2) +
               std::cos(lat1Rad) * std::cos(lat2Rad) * std::sin(deltaLon / 2) *
                   std::sin(deltaLon / 2);

    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    return EARTH_RADIUS_METERS * c;
}

double GpsParser::calculateBearing(double lat1, double lon1, double lat2,
                                   double lon2) {
    double lat1Rad = lat1 * DEG_TO_RAD;
    double lat2Rad = lat2 * DEG_TO_RAD;
    double deltaLon = (lon2 - lon1) * DEG_TO_RAD;

    double x = std::sin(deltaLon) * std::cos(lat2Rad);
    double y = std::cos(lat1Rad) * std::sin(lat2Rad) -
               std::sin(lat1Rad) * std::cos(lat2Rad) * std::cos(deltaLon);

    double bearing = std::atan2(x, y) * RAD_TO_DEG;

    // Normalize to 0-360
    while (bearing < 0)
        bearing += 360;
    while (bearing >= 360)
        bearing -= 360;

    return bearing;
}

char GpsParser::getRefChar(const IfdParseResult& ifd, uint16_t tag,
                           char defaultVal) {
    if (auto str = getStringValue(ifd, tag)) {
        if (!str->empty()) {
            return (*str)[0];
        }
    }
    return defaultVal;
}

std::optional<std::string> GpsParser::getStringValue(const IfdParseResult& ifd,
                                                     uint16_t tag) {
    return ifd.getString(tag);
}

std::optional<double> GpsParser::getRationalValue(const IfdParseResult& ifd,
                                                  uint16_t tag) {
    return ifd.getDouble(tag);
}

std::vector<Rational> GpsParser::getRationalArray(const IfdParseResult& ifd,
                                                  uint16_t tag) {
    return ifd.getRationalArray(tag);
}

// GpsData implementation (moved from gps_types.hpp)

std::optional<std::chrono::system_clock::time_point> GpsData::getDateTime()
    const {
    if (!datestamp.has_value() || !timestamp.has_value()) {
        return std::nullopt;
    }

    std::tm tm{};
    tm.tm_year = datestamp->year - 1900;
    tm.tm_mon = datestamp->month - 1;
    tm.tm_mday = datestamp->day;
    tm.tm_hour = timestamp->hour;
    tm.tm_min = timestamp->minute;
    tm.tm_sec = static_cast<int>(timestamp->second);

#if defined(_WIN32)
    std::time_t time = _mkgmtime(&tm);
#else
    std::time_t time = timegm(&tm);
#endif

    if (time == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }

    auto tp = std::chrono::system_clock::from_time_t(time);

    // Add fractional seconds
    double fracSec = timestamp->second - static_cast<int>(timestamp->second);
    tp += std::chrono::microseconds(static_cast<int64_t>(fracSec * 1000000));

    return tp;
}

std::optional<double> GpsData::distanceTo(const GpsData& other) const {
    if (!hasPosition() || !other.hasPosition()) {
        return std::nullopt;
    }

    return GpsParser::calculateDistance(latitude->toDecimalDegrees(),
                                        longitude->toDecimalDegrees(),
                                        other.latitude->toDecimalDegrees(),
                                        other.longitude->toDecimalDegrees());
}

std::optional<double> GpsData::bearingTo(const GpsData& other) const {
    if (!hasPosition() || !other.hasPosition()) {
        return std::nullopt;
    }

    return GpsParser::calculateBearing(latitude->toDecimalDegrees(),
                                       longitude->toDecimalDegrees(),
                                       other.latitude->toDecimalDegrees(),
                                       other.longitude->toDecimalDegrees());
}

// Note: GpsData::clear(), GpsCoordinate, GpsTimestamp, GpsDate, GpsDirection
// implementations are in types/gps_types.cpp

}  // namespace atom::image::metadata
