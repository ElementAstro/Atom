/**
 * @file time_utils.hpp
 *
 * @brief Time utility functions
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**************************************************
 *
 * @date 2023-3-31
 *
 * @brief Time utility functions
 *
 **************************************************/

#ifndef ATOM_WEB_TIME_UTILS_HPP
#define ATOM_WEB_TIME_UTILS_HPP

#include <cstdint>
#include <string_view>

namespace atom::web {
namespace time_utils {

/** @brief Minimum valid year */
constexpr int MIN_VALID_YEAR = 1970;
/** @brief Maximum valid year */
constexpr int MAX_VALID_YEAR = 2038;
/** @brief Minimum valid month */
constexpr int MIN_VALID_MONTH = 1;
/** @brief Maximum valid month */
constexpr int MAX_VALID_MONTH = 12;
/** @brief Minimum valid day */
constexpr int MIN_VALID_DAY = 1;
/** @brief Maximum valid day */
constexpr int MAX_VALID_DAY = 31;
/** @brief Minimum valid hour */
constexpr int MIN_VALID_HOUR = 0;
/** @brief Maximum valid hour */
constexpr int MAX_VALID_HOUR = 23;
/** @brief Minimum valid minute */
constexpr int MIN_VALID_MINUTE = 0;
/** @brief Maximum valid minute */
constexpr int MAX_VALID_MINUTE = 59;
/** @brief Minimum valid second */
constexpr int MIN_VALID_SECOND = 0;
/** @brief Maximum valid second */
constexpr int MAX_VALID_SECOND = 59;

/** @brief NTP packet size */
constexpr int NTP_PACKET_SIZE = 48;
/** @brief NTP service port */
constexpr uint16_t NTP_PORT = 123;
/** @brief Seconds difference between 1900 and 1970 */
constexpr uint32_t NTP_DELTA = 2208988800UL;  // seconds between 1900 and 1970

/**
 * @brief Validates date and time parameters
 *
 * @param year Year value
 * @param month Month value
 * @param day Day value
 * @param hour Hour value
 * @param minute Minute value
 * @param second Second value
 *
 * @return true If parameters are valid
 * @return false If parameters are invalid
 */
bool validateDateTime(int year, int month, int day, int hour, int minute,
                      int second);

/**
 * @brief Validates if hostname is valid
 *
 * @param hostname Hostname string
 *
 * @return true If hostname is valid
 * @return false If hostname is invalid
 */
bool validateHostname(std::string_view hostname);

}  // namespace time_utils
}  // namespace atom::web

#endif  // ATOM_WEB_TIME_UTILS_HPP
