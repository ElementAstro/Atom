#include "atom/utils/time.hpp"

#include <iostream>

using namespace atom::utils;

int main() {
    // Retrieve the current timestamp as a formatted string
    std::string currentTimestamp = getTimestampString();
    std::cout << "Current timestamp: " << currentTimestamp << std::endl;

    // Convert a UTC time string to China Standard Time (CST)
    std::string utcTime = "2023-12-25 12:00:00";
    std::string chinaTime = convertToChinaTime(utcTime);
    std::cout << "China Standard Time: " << chinaTime << std::endl;

    // Retrieve the current China Standard Time (CST) as a formatted timestamp
    // string
    std::string currentChinaTimestamp = getChinaTimestampString();
    std::cout << "Current China Standard Time: " << currentChinaTimestamp
              << std::endl;

    // Convert a timestamp to a formatted string
    time_t timestamp = 1672531199;  // Example timestamp
    std::string timestampStr = timeStampToString(timestamp);
    std::cout << "Formatted timestamp: " << timestampStr << std::endl;

    // Convert a tm structure to a formatted string
    std::tm timeStruct = {};
    timeStruct.tm_year = 123;  // Year since 1900
    timeStruct.tm_mon = 11;    // Month (0-11)
    timeStruct.tm_mday = 25;   // Day of the month (1-31)
    timeStruct.tm_hour = 12;   // Hour (0-23)
    timeStruct.tm_min = 0;     // Minute (0-59)
    timeStruct.tm_sec = 0;     // Second (0-59)
    std::string formattedTime = toString(timeStruct, "%Y-%m-%d %H:%M:%S");
    std::cout << "Formatted time: " << formattedTime << std::endl;

    // Retrieve the current UTC time as a formatted string
    std::string currentUtcTime = getUtcTime();
    std::cout << "Current UTC time: " << currentUtcTime << std::endl;

    // Convert a timestamp to a tm structure
    long long timestampValue = 1672531199;  // Example timestamp
    std::tm convertedTimeStruct = timestampToTime(timestampValue);
    std::cout << "Converted time: "
              << toString(convertedTimeStruct, "%Y-%m-%d %H:%M:%S")
              << std::endl;

    return 0;
}