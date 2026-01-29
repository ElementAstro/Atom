#ifndef ATOM_IMAGE_METADATA_GPS_PARSER_HPP
#define ATOM_IMAGE_METADATA_GPS_PARSER_HPP

#include <memory>
#include <optional>

#include "../types/gps_types.hpp"
#include "ifd_parser.hpp"

namespace atom::image::metadata {

/**
 * @brief GPS IFD parser
 *
 * Specialized parser for GPS metadata in EXIF data.
 */
class GpsParser {
public:
    /**
     * @brief Parse GPS data from IFD parse result
     * @param ifdResult Parsed GPS IFD
     * @return Parsed GPS data
     */
    [[nodiscard]] static std::shared_ptr<GpsData> parse(
        const IfdParseResult& ifdResult);

    /**
     * @brief Parse GPS coordinate from rational array
     * @param rationals Array of 3 rationals (degrees, minutes, seconds)
     * @param ref Reference direction (N/S or E/W)
     * @return Parsed coordinate or nullopt
     */
    [[nodiscard]] static std::optional<GpsCoordinate> parseCoordinate(
        const std::vector<Rational>& rationals, char ref);

    /**
     * @brief Parse GPS timestamp from rational array
     * @param rationals Array of 3 rationals (hour, minute, second)
     * @return Parsed timestamp or nullopt
     */
    [[nodiscard]] static std::optional<GpsTimestamp> parseTimestamp(
        const std::vector<Rational>& rationals);

    /**
     * @brief Parse GPS date from string
     * @param dateStr Date string in format "YYYY:MM:DD"
     * @return Parsed date or nullopt
     */
    [[nodiscard]] static std::optional<GpsDate> parseDate(
        const std::string& dateStr);

    /**
     * @brief Parse GPS direction
     * @param value Direction value in degrees
     * @param ref Reference ('T' for true, 'M' for magnetic)
     * @return Parsed direction
     */
    [[nodiscard]] static GpsDirection parseDirection(double value, char ref);

    /**
     * @brief Parse GPS speed
     * @param value Speed value
     * @param ref Unit reference ('K', 'M', or 'N')
     * @return Parsed speed
     */
    [[nodiscard]] static GpsSpeed parseSpeed(double value, char ref);

    /**
     * @brief Convert GPS data to IFD entries for writing
     * @param gpsData GPS data to convert
     * @return Vector of IFD entries
     */
    [[nodiscard]] static std::vector<IfdEntry> toIfdEntries(
        const GpsData& gpsData);

    /**
     * @brief Calculate distance between two GPS positions (Haversine formula)
     * @param lat1 Latitude 1 in decimal degrees
     * @param lon1 Longitude 1 in decimal degrees
     * @param lat2 Latitude 2 in decimal degrees
     * @param lon2 Longitude 2 in decimal degrees
     * @return Distance in meters
     */
    [[nodiscard]] static double calculateDistance(double lat1, double lon1,
                                                  double lat2, double lon2);

    /**
     * @brief Calculate bearing between two GPS positions
     * @param lat1 Latitude 1 in decimal degrees
     * @param lon1 Longitude 1 in decimal degrees
     * @param lat2 Latitude 2 in decimal degrees
     * @param lon2 Longitude 2 in decimal degrees
     * @return Bearing in degrees (0-360)
     */
    [[nodiscard]] static double calculateBearing(double lat1, double lon1,
                                                 double lat2, double lon2);

private:
    /**
     * @brief Get reference character from IFD entry
     */
    [[nodiscard]] static char getRefChar(const IfdParseResult& ifd,
                                         uint16_t tag, char defaultVal);

    /**
     * @brief Get string value from IFD entry
     */
    [[nodiscard]] static std::optional<std::string> getStringValue(
        const IfdParseResult& ifd, uint16_t tag);

    /**
     * @brief Get rational value from IFD entry
     */
    [[nodiscard]] static std::optional<double> getRationalValue(
        const IfdParseResult& ifd, uint16_t tag);

    /**
     * @brief Get rational array from IFD entry
     */
    [[nodiscard]] static std::vector<Rational> getRationalArray(
        const IfdParseResult& ifd, uint16_t tag);
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_GPS_PARSER_HPP
