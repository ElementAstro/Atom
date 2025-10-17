/**
 * @file time_example.cpp
 * @brief Comprehensive examples demonstrating time utilities
 *
 * This example demonstrates all functions available in atom::utils::time.hpp:
 * - Current time retrieval and formatting
 * - Timezone conversions and handling
 * - Timestamp conversions and manipulations
 * - Time structure operations
 * - Date arithmetic and calculations
 * - Performance timing and benchmarking
 */

#include "atom/utils/time/time.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

using namespace atom::utils;

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================" << std::endl;
}

// Helper function to print subsection headers
void printSubsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  Time Utilities Demo" << std::endl;
    std::cout << "==========================================" << std::endl;

    // ============================
    // Example 1: Current Time Operations
    // ============================
    printSection("1. Current Time Operations");

    printSubsection("Basic Time Retrieval");

    std::string currentTimestamp = getTimestampString();
    std::cout << "Current timestamp: " << currentTimestamp << std::endl;

    std::string currentUtcTime = getUtcTime();
    std::cout << "Current UTC time: " << currentUtcTime << std::endl;

    std::string currentChinaTimestamp = getChinaTimestampString();
    std::cout << "Current China Standard Time: " << currentChinaTimestamp
              << std::endl;

    printSubsection("Multiple Time Formats");

    // Get current time and format it in different ways
    auto now = std::chrono::system_clock::now();
    time_t currentTime = std::chrono::system_clock::to_time_t(now);

    std::tm* localTm = std::localtime(&currentTime);
    std::tm* utcTm = std::gmtime(&currentTime);

    std::cout << "Local ISO 8601: " << toString(*localTm, "%Y-%m-%dT%H:%M:%S")
              << std::endl;
    std::cout << "UTC ISO 8601: " << toString(*utcTm, "%Y-%m-%dT%H:%M:%SZ")
              << std::endl;
    std::cout << "US format: " << toString(*localTm, "%m/%d/%Y %I:%M:%S %p")
              << std::endl;
    std::cout << "European format: " << toString(*localTm, "%d.%m.%Y %H:%M:%S")
              << std::endl;
    std::cout << "RFC 2822 format: "
              << toString(*localTm, "%a, %d %b %Y %H:%M:%S") << std::endl;
    std::cout << "Unix timestamp: " << currentTime << std::endl;

    // ============================
    // Example 2: Timezone Conversions
    // ============================
    printSection("2. Timezone Conversions");

    printSubsection("UTC to China Time Conversion");

    std::vector<std::string> utcTimes = {
        "2023-12-25 12:00:00", "2024-01-01 00:00:00", "2024-06-15 18:30:45",
        "2024-12-31 23:59:59"};

    for (const auto& utcTime : utcTimes) {
        std::string chinaTime = convertToChinaTime(utcTime);
        std::cout << "UTC: " << utcTime << " -> China: " << chinaTime
                  << std::endl;
    }

    printSubsection("Time Zone Offset Calculations");

    // Demonstrate time zone offset calculations
    time_t testTime = 1672531200;  // 2023-01-01 00:00:00 UTC
    std::tm* testUtc = std::gmtime(&testTime);
    std::tm* testLocal = std::localtime(&testTime);

    std::cout << "Test timestamp: " << testTime << std::endl;
    std::cout << "UTC time: " << toString(*testUtc, "%Y-%m-%d %H:%M:%S")
              << std::endl;
    std::cout << "Local time: " << toString(*testLocal, "%Y-%m-%d %H:%M:%S")
              << std::endl;

    // Calculate offset
    time_t utcSeconds = mktime(testUtc);
    time_t localSeconds = mktime(testLocal);
    int offsetHours = static_cast<int>((localSeconds - utcSeconds) / 3600);
    std::cout << "Local timezone offset: " << offsetHours << " hours from UTC"
              << std::endl;

    // ============================
    // Example 3: Timestamp Conversions
    // ============================
    printSection("3. Timestamp Conversions");

    printSubsection("Timestamp to String Conversions");

    std::vector<time_t> timestamps = {
        0,           // Unix epoch
        946684800,   // Y2K (2000-01-01 00:00:00 UTC)
        1234567890,  // 2009-02-13 23:31:30 UTC
        1672531199,  // 2022-12-31 23:59:59 UTC
        2147483647   // 2038-01-19 03:14:07 UTC (32-bit limit)
    };

    for (time_t ts : timestamps) {
        std::string timestampStr = timeStampToString(ts);
        std::cout << "Timestamp " << ts << " -> " << timestampStr << std::endl;
    }

    printSubsection("String to Timestamp Conversions");

    std::vector<long long> timestampValues = {1672531199LL, 1234567890LL,
                                              946684800LL};

    for (long long tsValue : timestampValues) {
        auto convertedTimeOpt = timestampToTime(tsValue);
        if (convertedTimeOpt) {
            std::string formattedTime =
                toString(*convertedTimeOpt, "%Y-%m-%d %H:%M:%S");
            std::cout << "Timestamp " << tsValue << " -> " << formattedTime
                      << std::endl;
        } else {
            std::cout << "Timestamp " << tsValue << " -> Invalid timestamp"
                      << std::endl;
        }
    }

    // ============================
    // Example 4: Time Structure Operations
    // ============================
    printSection("4. Time Structure Operations");

    printSubsection("Manual Time Structure Creation");

    std::vector<std::tm> timeStructs;

    // Create various time structures
    std::tm newYear = {};
    newYear.tm_year = 124;  // 2024
    newYear.tm_mon = 0;     // January
    newYear.tm_mday = 1;    // 1st
    newYear.tm_hour = 0;
    newYear.tm_min = 0;
    newYear.tm_sec = 0;
    timeStructs.push_back(newYear);

    std::tm christmas = {};
    christmas.tm_year = 124;  // 2024
    christmas.tm_mon = 11;    // December
    christmas.tm_mday = 25;   // 25th
    christmas.tm_hour = 12;
    christmas.tm_min = 0;
    christmas.tm_sec = 0;
    timeStructs.push_back(christmas);

    std::tm leapDay = {};
    leapDay.tm_year = 124;  // 2024 (leap year)
    leapDay.tm_mon = 1;     // February
    leapDay.tm_mday = 29;   // 29th
    leapDay.tm_hour = 15;
    leapDay.tm_min = 30;
    leapDay.tm_sec = 45;
    timeStructs.push_back(leapDay);

    std::vector<std::string> descriptions = {"New Year 2024", "Christmas 2024",
                                             "Leap Day 2024"};

    for (size_t i = 0; i < timeStructs.size(); ++i) {
        std::string formatted =
            toString(timeStructs[i], "%A, %B %d, %Y at %H:%M:%S");
        std::cout << descriptions[i] << ": " << formatted << std::endl;
    }

    std::cout << "\nAll time utility examples completed successfully!"
              << std::endl;

    return 0;
}
