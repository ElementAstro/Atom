/*
 * qdatetime_example.cpp
 *
 * This example demonstrates the usage of QDateTime class in the atom::utils
 * namespace. It covers creation, manipulation, comparison, and formatting of
 * date-time objects along with timezone handling.
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

// Helper function to print a separator line
void printSeparator(const std::string& title) {
    std::cout << "\n=== " << title << " ===" << std::endl;
}

// Helper function to print QDateTime objects
void printDateTime(const atom::utils::QDateTime& dt, const std::string& label) {
    if (dt.isValid()) {
        std::cout << label << ": " << dt.toString("%Y-%m-%d %H:%M:%S")
                  << std::endl;
    } else {
        std::cout << label << ": Invalid DateTime" << std::endl;
    }
}

// Helper function to demonstrate error handling
template <typename Func>
void demonstrateErrorHandling(const std::string& description, Func func) {
    printSeparator("Error Handling: " + description);

    try {
        func();
        std::cout << "No exception thrown." << std::endl;
    } catch (const atom::utils::QDateTime::ParseError& e) {
        std::cout << "Caught ParseError: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught exception: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "===============================================" << std::endl;
    std::cout << "QDateTime Class Comprehensive Usage Example" << std::endl;
    std::cout << "===============================================" << std::endl;

    // ==========================================
    // 1. Creating QDateTime Objects
    // ==========================================
    printSeparator("Creating QDateTime Objects");

    // Default constructor (creates invalid datetime)
    atom::utils::QDateTime invalidDateTime;
    std::cout << "Default constructor - isValid(): "
              << (invalidDateTime.isValid() ? "true" : "false") << std::endl;

    // Create from string with format
    atom::utils::QDateTime dateTime1("2024-03-27 14:30:15",
                                     "%Y-%m-%d %H:%M:%S");
    printDateTime(dateTime1, "From string with format");

    // Using static fromString method
    auto dateTime2 = atom::utils::QDateTime::fromString("27/03/2024 14:30:15",
                                                        "%d/%m/%Y %H:%M:%S");
    printDateTime(dateTime2, "Using fromString");

    // Get current datetime
    auto currentDT = atom::utils::QDateTime::currentDateTime();
    printDateTime(currentDT, "Current datetime");

    // ==========================================
    // 2. Working with Time Zones
    // ==========================================
    printSeparator("Working with Time Zones");

    // Create timezone objects
    atom::utils::QTimeZone utcTz("UTC");
    atom::utils::QTimeZone estTz("America/New_York");
    atom::utils::QTimeZone jstTz("Asia/Tokyo");
    atom::utils::QTimeZone cstTz("Asia/Shanghai");

    // Get current datetime in different time zones
    auto currentUtc = atom::utils::QDateTime::currentDateTime(utcTz);
    auto currentEst = atom::utils::QDateTime::currentDateTime(estTz);
    auto currentJst = atom::utils::QDateTime::currentDateTime(jstTz);
    auto currentCst = atom::utils::QDateTime::currentDateTime(cstTz);

    printDateTime(currentUtc, "Current time in UTC");
    printDateTime(currentEst, "Current time in EST");
    printDateTime(currentJst, "Current time in JST");
    printDateTime(currentCst, "Current time in CST");

    // Create datetime with explicit timezone
    auto dateTimeWithTz = atom::utils::QDateTime::fromString(
        "2024-03-27 14:30:15", "%Y-%m-%d %H:%M:%S", estTz);
    printDateTime(dateTimeWithTz, "DateTime with EST timezone");

    // ==========================================
    // 3. Converting to Different Formats
    // ==========================================
    printSeparator("Converting to Different Formats");

    // Convert to string with different formats
    std::cout << "ISO format: " << dateTime1.toString("%Y-%m-%dT%H:%M:%S")
              << std::endl;
    std::cout << "US format: " << dateTime1.toString("%m/%d/%Y %I:%M:%S %p")
              << std::endl;
    std::cout << "European format: " << dateTime1.toString("%d.%m.%Y %H:%M:%S")
              << std::endl;
    std::cout << "Custom format: "
              << dateTime1.toString("%A, %B %d, %Y at %H:%M:%S") << std::endl;

    // Convert to time_t (Unix timestamp)
    std::time_t timestamp = dateTime1.toTimeT();
    std::cout << "Unix timestamp: " << timestamp << std::endl;

    // Display in different time zones
    std::cout << "Tokyo time: "
              << dateTime1.toString("%Y-%m-%d %H:%M:%S", jstTz) << std::endl;
    std::cout << "New York time: "
              << dateTime1.toString("%Y-%m-%d %H:%M:%S", estTz) << std::endl;

    // ==========================================
    // 4. DateTime Arithmetic
    // ==========================================
    printSeparator("DateTime Arithmetic");

    // Add days
    auto futureDT = dateTime1.addDays(5);
    printDateTime(futureDT, "Original + 5 days");

    // Add seconds
    auto futureSeconds = dateTime1.addSecs(3600);  // Add 1 hour
    printDateTime(futureSeconds, "Original + 3600 seconds (1 hour)");

    // Subtract (using negative values)
    auto pastDT = dateTime1.addDays(-10);
    printDateTime(pastDT, "Original - 10 days");

    auto pastSeconds = dateTime1.addSecs(-7200);  // Subtract 2 hours
    printDateTime(pastSeconds, "Original - 7200 seconds (2 hours)");

    // ==========================================
    // 5. DateTime Differences
    // ==========================================
    printSeparator("DateTime Differences");

    // Calculate days between dates
    int daysDiff = dateTime1.daysTo(futureDT);
    std::cout << "Days between dateTime1 and futureDT: " << daysDiff
              << std::endl;

    // Calculate seconds between dates
    int secsDiff = dateTime1.secsTo(futureSeconds);
    std::cout << "Seconds between dateTime1 and futureSeconds: " << secsDiff
              << std::endl;

    // Difference between timezones
    int tzSecsDiff = currentUtc.secsTo(currentJst);
    std::cout << "Seconds difference between UTC and JST: " << tzSecsDiff
              << std::endl;

    // ==========================================
    // 6. DateTime Comparisons
    // ==========================================
    printSeparator("DateTime Comparisons");

    // Using comparison operators
    std::cout << "dateTime1 == dateTime2: "
              << (dateTime1 == dateTime2 ? "true" : "false") << std::endl;
    std::cout << "dateTime1 != dateTime2: "
              << (dateTime1 != dateTime2 ? "true" : "false") << std::endl;
    std::cout << "dateTime1 < futureDT: "
              << (dateTime1 < futureDT ? "true" : "false") << std::endl;
    std::cout << "dateTime1 <= futureDT: "
              << (dateTime1 <= futureDT ? "true" : "false") << std::endl;
    std::cout << "dateTime1 > pastDT: "
              << (dateTime1 > pastDT ? "true" : "false") << std::endl;
    std::cout << "dateTime1 >= pastDT: "
              << (dateTime1 >= pastDT ? "true" : "false") << std::endl;

    // Create a vector of dates and sort them
    std::vector<atom::utils::QDateTime> dateTimes = {dateTime1, futureDT,
                                                     pastDT, currentDT};

    std::cout << "\nBefore sorting:" << std::endl;
    for (const auto& dt : dateTimes) {
        std::cout << "  " << dt.toString("%Y-%m-%d %H:%M:%S") << std::endl;
    }

    std::sort(dateTimes.begin(), dateTimes.end());

    std::cout << "\nAfter sorting:" << std::endl;
    for (const auto& dt : dateTimes) {
        std::cout << "  " << dt.toString("%Y-%m-%d %H:%M:%S") << std::endl;
    }

    // ==========================================
    // 7. Error Handling
    // ==========================================

    // Invalid format specifier
    demonstrateErrorHandling("Invalid format specifier", []() {
        atom::utils::QDateTime dt("2024-03-27",
                                  "%Y-%m-%d %H:%M:%S");  // Doesn't match format
        std::cout << "This should not be printed" << std::endl;
    });

    // Invalid datetime string
    demonstrateErrorHandling("Invalid datetime string", []() {
        atom::utils::QDateTime dt("not a date", "%Y-%m-%d");
        std::cout << "This should not be printed" << std::endl;
    });

    // Operations with invalid datetime
    demonstrateErrorHandling("Operations with invalid datetime", []() {
        atom::utils::QDateTime invalidDT;
        auto timeT = invalidDT.toTimeT();  // Should handle gracefully
        std::cout << "toTimeT() on invalid datetime returned: " << timeT
                  << std::endl;
    });

    // ==========================================
    // 8. Advanced Use Cases
    // ==========================================
    printSeparator("Advanced Use Cases");

    // Parse and convert between formats
    try {
        // Parse RFC 2822 format
        auto emailDT = atom::utils::QDateTime::fromString(
            "Wed, 27 Mar 2024 14:30:15 +0000", "%a, %d %b %Y %H:%M:%S %z");

        // Convert to ISO 8601 format
        std::string isoFormat = emailDT.toString("%Y-%m-%dT%H:%M:%SZ");
        std::cout << "Converted from RFC 2822 to ISO 8601: " << isoFormat
                  << std::endl;

        // Convert to a custom human-readable format
        std::string readableFormat =
            emailDT.toString("%A, %B %d, %Y at %I:%M %p");
        std::cout << "Human-readable format: " << readableFormat << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error in advanced parsing: " << e.what() << std::endl;
    }

    // Date calculations for business logic
    try {
        // Current date
        auto today = atom::utils::QDateTime::currentDateTime();

        // Calculate due date (30 days from now)
        auto dueDate = today.addDays(30);
        std::cout << "Invoice due date (30 days from today): "
                  << dueDate.toString("%Y-%m-%d") << std::endl;

        // Check if payment is overdue
        auto paymentDate =
            atom::utils::QDateTime::fromString("2024-05-01", "%Y-%m-%d");
        bool isOverdue = paymentDate > dueDate;
        std::cout << "Payment on May 1, 2024 is "
                  << (isOverdue ? "overdue" : "not overdue") << std::endl;

        // Calculate days remaining until due date
        int daysRemaining = today.daysTo(dueDate);
        std::cout << "Days remaining until due date: " << daysRemaining
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error in date calculations: " << e.what() << std::endl;
    }

    // Working with multiple timezones (international flight)
    try {
        // Flight departs Tokyo at 10:00 AM JST
        auto departureDT = atom::utils::QDateTime::fromString(
            "2024-03-28 10:00:00", "%Y-%m-%d %H:%M:%S", jstTz);

        // Flight takes 12 hours to arrive in New York
        auto arrivalDT = departureDT.addSecs(12 * 3600);

        std::cout << "Flight departs Tokyo at: "
                  << departureDT.toString("%Y-%m-%d %H:%M:%S", jstTz) << " JST"
                  << std::endl;

        std::cout << "Flight arrives in New York at: "
                  << arrivalDT.toString("%Y-%m-%d %H:%M:%S", estTz) << " EST"
                  << std::endl;

        // Calculate local time difference
        auto departureLocal = departureDT.toString("%H:%M", jstTz);
        auto arrivalLocal = arrivalDT.toString("%H:%M", estTz);

        std::cout << "Departure: " << departureLocal
                  << " JST, Arrival: " << arrivalLocal << " EST" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error in timezone calculations: " << e.what()
                  << std::endl;
    }

    // ==========================================
    // 9. Performance Testing
    // ==========================================
    printSeparator("Performance Testing");

    // Test creation of multiple QDateTime objects
    {
        auto start = std::chrono::high_resolution_clock::now();

        const int iterations = 10000;
        for (int i = 0; i < iterations; ++i) {
            auto dt = atom::utils::QDateTime::fromString("2024-03-27 14:30:15",
                                                         "%Y-%m-%d %H:%M:%S");
            (void)dt;  // Prevent optimization
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;

        std::cout << "Created " << iterations << " QDateTime objects in "
                  << elapsed.count() << " ms" << std::endl;
        std::cout << "Average time per object: "
                  << (elapsed.count() / iterations) << " ms" << std::endl;
    }

    // Test timezone operations with caching
    {
        auto start = std::chrono::high_resolution_clock::now();

        const int iterations = 1000;
        for (int i = 0; i < iterations; ++i) {
            // This should use the cache for better performance
            auto dt = atom::utils::QDateTime::currentDateTime(jstTz);
            auto str = dt.toString("%Y-%m-%d %H:%M:%S", estTz);
            (void)str;  // Prevent optimization
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;

        std::cout << "Performed " << iterations << " timezone operations in "
                  << elapsed.count() << " ms" << std::endl;
        std::cout << "Average time per operation: "
                  << (elapsed.count() / iterations) << " ms" << std::endl;
    }

    std::cout << "\n==============================================="
              << std::endl;
    std::cout << "QDateTime Example Completed" << std::endl;
    std::cout << "===============================================" << std::endl;

    return 0;
}
