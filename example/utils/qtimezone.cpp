#include "atom/utils/qtimezone.hpp"
#include "atom/utils/qdatetime.hpp"

#include <iostream>

using namespace atom::utils;

int main() {
    // Create a QTimeZone object using the default constructor
    QTimeZone defaultTimeZone;
    std::cout << "Default QTimeZone is valid: " << std::boolalpha
              << defaultTimeZone.isValid() << std::endl;

    // Create a QTimeZone object from a time zone identifier
    QTimeZone timeZone("America/New_York");
    std::cout << "QTimeZone is valid: " << std::boolalpha << timeZone.isValid()
              << std::endl;

    // Get the list of available time zone identifiers
    auto timeZoneIds = QTimeZone::availableTimeZoneIds();
    std::cout << "Available time zone IDs: ";
    for (const auto& id : timeZoneIds) {
        std::cout << id << " ";
    }
    std::cout << std::endl;

    // Get the time zone identifier
    std::string timeZoneId = timeZone.id();
    std::cout << "Time zone ID: " << timeZoneId << std::endl;

    // Get the display name of the time zone
    std::string displayName = timeZone.displayName();
    std::cout << "Display name: " << displayName << std::endl;

    // Create a QDateTime object
    QDateTime dateTime("2023-12-25 15:30:00", "%Y-%m-%d %H:%M:%S");

    // Get the offset from UTC for a specific date and time
    auto offsetFromUtc = timeZone.offsetFromUtc(dateTime);
    std::cout << "Offset from UTC: " << offsetFromUtc.count() << " seconds"
              << std::endl;

    // Get the standard time offset from UTC
    auto standardOffset = timeZone.standardTimeOffset();
    std::cout << "Standard time offset: " << standardOffset.count()
              << " seconds" << std::endl;

    // Get the daylight saving time offset from UTC
    auto daylightOffset = timeZone.daylightTimeOffset();
    std::cout << "Daylight time offset: " << daylightOffset.count()
              << " seconds" << std::endl;

    // Check if the time zone observes daylight saving time
    bool hasDaylight = timeZone.hasDaylightTime();
    std::cout << "Observes daylight saving time: " << std::boolalpha
              << hasDaylight << std::endl;

    // Check if a specific date and time is within the daylight saving time
    // period
    bool isDaylight = timeZone.isDaylightTime(dateTime);
    std::cout << "Is daylight saving time: " << std::boolalpha << isDaylight
              << std::endl;

    // Compare two QTimeZone objects
    QTimeZone timeZone1("America/New_York");
    QTimeZone timeZone2("Europe/London");

    bool isLessThan = timeZone1 < timeZone2;
    bool isGreaterThan = timeZone1 > timeZone2;
    bool isEqual = timeZone1 == timeZone2;

    std::cout << "TimeZone1 is less than TimeZone2: " << std::boolalpha
              << isLessThan << std::endl;
    std::cout << "TimeZone1 is greater than TimeZone2: " << std::boolalpha
              << isGreaterThan << std::endl;
    std::cout << "TimeZone1 is equal to TimeZone2: " << std::boolalpha
              << isEqual << std::endl;

    return 0;
}