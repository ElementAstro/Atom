/*
 * qtimezone_example.cpp
 *
 * This example demonstrates the usage of QTimeZone class in the atom::utils
 * namespace. It covers timezone creation, information retrieval, daylight
 * saving time operations, and integration with QDateTime objects.
 *
 * Copyright (C) 2024 Example User
 */

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "atom/log/loguru.hpp"
#include "atom/utils/qdatetime.hpp"
#include "atom/utils/qtimezone.hpp"

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

// Helper function to print timezone information
void printTimeZoneInfo(const atom::utils::QTimeZone& tz) {
    std::cout << "Time Zone ID: " << tz.identifier() << std::endl;
    std::cout << "Display Name: " << tz.displayName() << std::endl;
    std::cout << "Is Valid: " << (tz.isValid() ? "Yes" : "No") << std::endl;
    std::cout << "Standard Time Offset: " << tz.standardTimeOffset().count()
              << " seconds" << std::endl;
    std::cout << "Has Daylight Time: " << (tz.hasDaylightTime() ? "Yes" : "No")
              << std::endl;

    if (tz.hasDaylightTime()) {
        std::cout << "Daylight Time Offset: " << tz.daylightTimeOffset().count()
                  << " seconds" << std::endl;
    }
    std::cout << std::endl;
}

// Helper function to print a date in a specific timezone
void printDateInTimeZone(const atom::utils::QDateTime& dt,
                         const atom::utils::QTimeZone& tz,
                         const std::string& label) {
    std::cout << label << ": " << dt.toString("%Y-%m-%d %H:%M:%S", tz)
              << std::endl;
    std::cout << "  UTC Offset: " << tz.offsetFromUtc(dt).count() << " seconds"
              << std::endl;
    std::cout << "  In Daylight Time: "
              << (tz.isDaylightTime(dt) ? "Yes" : "No") << std::endl;
    std::cout << std::endl;
}

// Helper function to demonstrate error handling
template <typename Func>
void demonstrateErrorHandling(const std::string& description, Func func) {
    std::cout << "\n--- Error Handling: " << description << " ---" << std::endl;

    try {
        func();
        std::cout << "No exception thrown." << std::endl;
    } catch (const atom::utils::GetTimeException& e) {
        std::cout << "Caught GetTimeException: " << e.what() << std::endl;
    } catch (const atom::error::Exception& e) {
        std::cout << "Caught Exception: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught standard exception: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // Initialize loguru
    loguru::g_stderr_verbosity = 1;
    loguru::init(argc, argv);

    std::cout << "=================================================="
              << std::endl;
    std::cout << "QTimeZone Class Comprehensive Usage Example" << std::endl;
    std::cout << "=================================================="
              << std::endl;

    // ==========================================
    // 1. Creating QTimeZone Objects
    // ==========================================
    printSection("1. Creating QTimeZone Objects");

    // Default constructor (creates UTC timezone)
    std::cout << "Creating default timezone (UTC):" << std::endl;
    atom::utils::QTimeZone defaultTz;
    printTimeZoneInfo(defaultTz);

    // Creating timezone with explicit ID
    std::cout << "Creating timezone with explicit ID (EST):" << std::endl;
    atom::utils::QTimeZone estTz("EST");
    printTimeZoneInfo(estTz);

    // Creating timezone with string literal
    std::cout << "Creating timezone with string literal (PST):" << std::endl;
    atom::utils::QTimeZone pstTz("PST");
    printTimeZoneInfo(pstTz);

    // Creating timezone with std::string
    std::cout << "Creating timezone with std::string (CST):" << std::endl;
    std::string cstId = "CST";
    atom::utils::QTimeZone cstTz(cstId);
    printTimeZoneInfo(cstTz);

    // ==========================================
    // 2. Available Time Zone IDs
    // ==========================================
    printSection("2. Available Time Zone IDs");

    // Get all available time zone IDs
    std::vector<std::string> availableIds =
        atom::utils::QTimeZone::availableTimeZoneIds();
    std::cout << "Available Time Zone IDs:" << std::endl;
    for (const auto& id : availableIds) {
        std::cout << "  - " << id << std::endl;
    }
    std::cout << std::endl;

    // ==========================================
    // 3. Time Zone Properties
    // ==========================================
    printSection("3. Time Zone Properties");

    // Using identifier() and id() methods
    std::cout << "Timezone Identifiers:" << std::endl;
    std::cout << "  defaultTz.identifier(): " << defaultTz.identifier()
              << std::endl;
    std::cout << "  defaultTz.id(): " << defaultTz.id()
              << " (alias for identifier)" << std::endl;
    std::cout << std::endl;

    // Display names
    std::cout << "Display Names:" << std::endl;
    std::cout << "  UTC: " << defaultTz.displayName() << std::endl;
    std::cout << "  EST: " << estTz.displayName() << std::endl;
    std::cout << "  PST: " << pstTz.displayName() << std::endl;
    std::cout << "  CST: " << cstTz.displayName() << std::endl;
    std::cout << std::endl;

    // Validity checking
    std::cout << "Validity Checking:" << std::endl;
    std::cout << "  defaultTz.isValid(): "
              << (defaultTz.isValid() ? "true" : "false") << std::endl;
    std::cout << "  estTz.isValid(): " << (estTz.isValid() ? "true" : "false")
              << std::endl;
    std::cout << std::endl;

    // ==========================================
    // 4. Offsets from UTC
    // ==========================================
    printSection("4. Offsets from UTC");

    // Create a fixed datetime for consistent testing
    atom::utils::QDateTime fixedDateTime("2024-07-15 12:00:00",
                                         "%Y-%m-%d %H:%M:%S");
    std::cout << "Fixed datetime for testing: 2024-07-15 12:00:00" << std::endl;

    // Get standard time offsets
    std::cout << "Standard Time Offsets:" << std::endl;
    std::cout << "  UTC: " << defaultTz.standardTimeOffset().count()
              << " seconds" << std::endl;
    std::cout << "  EST: " << estTz.standardTimeOffset().count() << " seconds"
              << std::endl;
    std::cout << "  PST: " << pstTz.standardTimeOffset().count() << " seconds"
              << std::endl;
    std::cout << "  CST: " << cstTz.standardTimeOffset().count() << " seconds"
              << std::endl;
    std::cout << std::endl;

    // Get offsets from UTC for different dates
    std::cout << "Offsets from UTC for fixed datetime:" << std::endl;
    std::cout << "  UTC: " << defaultTz.offsetFromUtc(fixedDateTime).count()
              << " seconds" << std::endl;
    std::cout << "  EST: " << estTz.offsetFromUtc(fixedDateTime).count()
              << " seconds" << std::endl;
    std::cout << "  PST: " << pstTz.offsetFromUtc(fixedDateTime).count()
              << " seconds" << std::endl;
    std::cout << "  CST: " << cstTz.offsetFromUtc(fixedDateTime).count()
              << " seconds" << std::endl;
    std::cout << std::endl;

    // Get current datetime
    auto currentDateTime = atom::utils::QDateTime::currentDateTime();
    std::cout << "Current datetime: "
              << currentDateTime.toString("%Y-%m-%d %H:%M:%S") << std::endl;
    std::cout << "Current offsets from UTC:" << std::endl;
    std::cout << "  UTC: " << defaultTz.offsetFromUtc(currentDateTime).count()
              << " seconds" << std::endl;
    std::cout << "  EST: " << estTz.offsetFromUtc(currentDateTime).count()
              << " seconds" << std::endl;
    std::cout << "  PST: " << pstTz.offsetFromUtc(currentDateTime).count()
              << " seconds" << std::endl;
    std::cout << "  CST: " << cstTz.offsetFromUtc(currentDateTime).count()
              << " seconds" << std::endl;
    std::cout << std::endl;

    // ==========================================
    // 5. Daylight Saving Time
    // ==========================================
    printSection("5. Daylight Saving Time");

    // Check if timezones have DST
    std::cout << "Timezones with Daylight Saving Time:" << std::endl;
    std::cout << "  UTC has DST: "
              << (defaultTz.hasDaylightTime() ? "Yes" : "No") << std::endl;
    std::cout << "  EST has DST: " << (estTz.hasDaylightTime() ? "Yes" : "No")
              << std::endl;
    std::cout << "  PST has DST: " << (pstTz.hasDaylightTime() ? "Yes" : "No")
              << std::endl;
    std::cout << "  CST has DST: " << (cstTz.hasDaylightTime() ? "Yes" : "No")
              << std::endl;
    std::cout << std::endl;

    // Get daylight time offsets
    std::cout << "Daylight Time Offsets:" << std::endl;
    std::cout << "  UTC: " << defaultTz.daylightTimeOffset().count()
              << " seconds" << std::endl;
    std::cout << "  EST: " << estTz.daylightTimeOffset().count() << " seconds"
              << std::endl;
    std::cout << "  PST: " << pstTz.daylightTimeOffset().count() << " seconds"
              << std::endl;
    std::cout << "  CST: " << cstTz.daylightTimeOffset().count() << " seconds"
              << std::endl;
    std::cout << std::endl;

    // Check if specific dates are in DST
    std::cout << "Checking DST status for different dates:" << std::endl;

    // Create dates in different seasons
    atom::utils::QDateTime winterDate("2024-01-15 12:00:00",
                                      "%Y-%m-%d %H:%M:%S");
    atom::utils::QDateTime springDate("2024-04-15 12:00:00",
                                      "%Y-%m-%d %H:%M:%S");
    atom::utils::QDateTime summerDate("2024-07-15 12:00:00",
                                      "%Y-%m-%d %H:%M:%S");
    atom::utils::QDateTime fallDate("2024-10-15 12:00:00", "%Y-%m-%d %H:%M:%S");

    std::cout << "Winter (January 15):" << std::endl;
    std::cout << "  EST in DST: "
              << (estTz.isDaylightTime(winterDate) ? "Yes" : "No") << std::endl;

    std::cout << "Spring (April 15):" << std::endl;
    std::cout << "  EST in DST: "
              << (estTz.isDaylightTime(springDate) ? "Yes" : "No") << std::endl;

    std::cout << "Summer (July 15):" << std::endl;
    std::cout << "  EST in DST: "
              << (estTz.isDaylightTime(summerDate) ? "Yes" : "No") << std::endl;

    std::cout << "Fall (October 15):" << std::endl;
    std::cout << "  EST in DST: "
              << (estTz.isDaylightTime(fallDate) ? "Yes" : "No") << std::endl;

    std::cout << std::endl;

    // ==========================================
    // 6. Working with QDateTime
    // ==========================================
    printSection("6. Working with QDateTime");

    // Current time in different timezones
    std::cout << "Current Time in Different Timezones:" << std::endl;

    auto utcNow = atom::utils::QDateTime::currentDateTime(defaultTz);
    auto estNow = atom::utils::QDateTime::currentDateTime(estTz);
    auto pstNow = atom::utils::QDateTime::currentDateTime(pstTz);
    auto cstNow = atom::utils::QDateTime::currentDateTime(cstTz);

    printDateInTimeZone(utcNow, defaultTz, "UTC Now");
    printDateInTimeZone(estNow, estTz, "EST Now");
    printDateInTimeZone(pstNow, pstTz, "PST Now");
    printDateInTimeZone(cstNow, cstTz, "CST Now");

    // Create a specific time and display in different timezones
    atom::utils::QDateTime specificTime("2024-03-27 14:30:00",
                                        "%Y-%m-%d %H:%M:%S");
    std::cout << "Specific Time (2024-03-27 14:30:00) in Different Timezones:"
              << std::endl;

    printDateInTimeZone(specificTime, defaultTz, "UTC");
    printDateInTimeZone(specificTime, estTz, "EST");
    printDateInTimeZone(specificTime, pstTz, "PST");
    printDateInTimeZone(specificTime, cstTz, "CST");

    // Show the same time point in different time zones
    std::cout << "Same Time Point in Different Timezones:" << std::endl;

    atom::utils::QDateTime newYorkTime("2024-03-27 14:30:00",
                                       "%Y-%m-%d %H:%M:%S", estTz);
    std::cout << "Original time in EST: "
              << newYorkTime.toString("%Y-%m-%d %H:%M:%S", estTz) << std::endl;
    std::cout << "Same time in UTC: "
              << newYorkTime.toString("%Y-%m-%d %H:%M:%S", defaultTz)
              << std::endl;
    std::cout << "Same time in PST: "
              << newYorkTime.toString("%Y-%m-%d %H:%M:%S", pstTz) << std::endl;
    std::cout << "Same time in CST: "
              << newYorkTime.toString("%Y-%m-%d %H:%M:%S", cstTz) << std::endl;
    std::cout << std::endl;

    // ==========================================
    // 7. Error Handling
    // ==========================================
    printSection("7. Error Handling");

    // Attempt to create with invalid timezone ID
    demonstrateErrorHandling("Creating with Invalid Timezone ID", []() {
        atom::utils::QTimeZone invalidTz("INVALID_TIMEZONE");
        std::cout << "This line should not be reached." << std::endl;
    });

    // Attempt to get offset from UTC with invalid datetime
    demonstrateErrorHandling("Offset from UTC with Invalid DateTime", [&]() {
        atom::utils::QDateTime invalidDateTime;  // Invalid datetime
        auto offset = estTz.offsetFromUtc(invalidDateTime);
        std::cout << "Offset: " << offset.count() << " seconds" << std::endl;
    });

    // Attempt to check DST status with invalid datetime
    demonstrateErrorHandling("Check DST with Invalid DateTime", [&]() {
        atom::utils::QDateTime invalidDateTime;  // Invalid datetime
        bool isDST = estTz.isDaylightTime(invalidDateTime);
        std::cout << "Is DST: " << (isDST ? "Yes" : "No") << std::endl;
    });

    // ==========================================
    // 8. Practical Examples
    // ==========================================
    printSection("8. Practical Examples");

    // Example 1: Converting between timezones
    std::cout << "Example 1: Converting Between Timezones" << std::endl;

    atom::utils::QDateTime nycMeeting("2024-03-27 10:00:00",
                                      "%Y-%m-%d %H:%M:%S", estTz);
    std::cout << "Meeting in New York (EST): "
              << nycMeeting.toString("%Y-%m-%d %I:%M %p", estTz) << std::endl;
    std::cout << "Meeting time for attendees in:" << std::endl;
    std::cout << "  UTC: "
              << nycMeeting.toString("%Y-%m-%d %I:%M %p", defaultTz)
              << std::endl;
    std::cout << "  PST: " << nycMeeting.toString("%Y-%m-%d %I:%M %p", pstTz)
              << std::endl;
    std::cout << "  CST: " << nycMeeting.toString("%Y-%m-%d %I:%M %p", cstTz)
              << std::endl;
    std::cout << std::endl;

    // Example 2: Flight arrival times
    std::cout << "Example 2: Flight Arrival Times" << std::endl;

    atom::utils::QDateTime departure("2024-03-27 08:00:00", "%Y-%m-%d %H:%M:%S",
                                     estTz);
    std::cout << "Flight departs from New York (EST): "
              << departure.toString("%Y-%m-%d %I:%M %p", estTz) << std::endl;

    // Flight duration: 6 hours
    atom::utils::QDateTime arrival = departure.addSecs(6 * 3600);
    std::cout << "Flight arrives in Los Angeles (PST): "
              << arrival.toString("%Y-%m-%d %I:%M %p", pstTz) << std::endl;

    // Calculate local time difference
    int hourDiff =
        (estTz.offsetFromUtc(departure) - pstTz.offsetFromUtc(arrival))
            .count() /
        3600;
    std::cout << "Time zone difference: " << std::abs(hourDiff) << " hours"
              << std::endl;
    std::cout << std::endl;

    // Example 3: Working with international deadlines
    std::cout << "Example 3: International Deadline" << std::endl;

    atom::utils::QDateTime deadline("2024-04-01 00:00:00", "%Y-%m-%d %H:%M:%S",
                                    defaultTz);
    std::cout << "Global deadline (UTC): "
              << deadline.toString("%Y-%m-%d %I:%M %p", defaultTz) << std::endl;
    std::cout << "Local deadlines:" << std::endl;
    std::cout << "  EST: " << deadline.toString("%Y-%m-%d %I:%M %p", estTz)
              << std::endl;
    std::cout << "  PST: " << deadline.toString("%Y-%m-%d %I:%M %p", pstTz)
              << std::endl;
    std::cout << "  CST: " << deadline.toString("%Y-%m-%d %I:%M %p", cstTz)
              << std::endl;

    // Get current time
    auto now = atom::utils::QDateTime::currentDateTime();

    // Calculate time remaining until deadline
    int hoursRemaining = now.secsTo(deadline) / 3600;
    std::cout << "Hours remaining until deadline: " << hoursRemaining
              << std::endl;
    std::cout << std::endl;

    // Example 4: DST transition effect
    std::cout << "Example 4: DST Transition Effect" << std::endl;

    // Create a date just before DST transition (2nd Sunday in March)
    atom::utils::QDateTime beforeDST("2024-03-10 01:30:00", "%Y-%m-%d %H:%M:%S",
                                     estTz);

    // Create a date with the same wall clock time after DST transition
    atom::utils::QDateTime afterDST("2024-03-10 03:30:00", "%Y-%m-%d %H:%M:%S",
                                    estTz);

    std::cout << "Before DST transition: "
              << beforeDST.toString("%Y-%m-%d %I:%M %p", estTz) << std::endl;
    std::cout << "  In DST: "
              << (estTz.isDaylightTime(beforeDST) ? "Yes" : "No") << std::endl;
    std::cout << "  UTC offset: "
              << estTz.offsetFromUtc(beforeDST).count() / 3600 << " hours"
              << std::endl;

    std::cout << "After DST transition: "
              << afterDST.toString("%Y-%m-%d %I:%M %p", estTz) << std::endl;
    std::cout << "  In DST: " << (estTz.isDaylightTime(afterDST) ? "Yes" : "No")
              << std::endl;
    std::cout << "  UTC offset: "
              << estTz.offsetFromUtc(afterDST).count() / 3600 << " hours"
              << std::endl;

    // Calculate wall clock hours difference
    int wallClockDiff = 2;  // 3:30 - 1:30

    // Calculate actual hours difference
    int actualDiff = afterDST.secsTo(beforeDST) / 3600;

    std::cout << "Wall clock hours difference: " << wallClockDiff << " hours"
              << std::endl;
    std::cout << "Actual elapsed time difference: " << std::abs(actualDiff)
              << " hours" << std::endl;
    std::cout << "Effect of DST transition: 1 hour \"lost\"" << std::endl;
    std::cout << std::endl;

    std::cout << "=================================================="
              << std::endl;
    std::cout << "QTimeZone Example Completed" << std::endl;
    std::cout << "=================================================="
              << std::endl;  // filepath: examples/qtimezone_example.cpp
}