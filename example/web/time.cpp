/*
 * time_manager_example.cpp
 *
 * Copyright (C) 2025 Developers <example.com>
 *
 * A comprehensive example demonstrating the use of the Atom TimeManager class
 */

#include "atom/web/time.hpp"
#include "atom/log/loguru.hpp"
#include <chrono>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>

// For prettier time output
std::string formatTime(std::time_t time) {
    char buffer[80];
    struct tm* timeinfo = localtime(&time);
    strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", timeinfo);
    return std::string(buffer);
}

int main(int argc, char** argv) {
    // Initialize logging
    loguru::init(argc, argv);
    loguru::add_file("time_manager_example.log", loguru::Append,
                     loguru::Verbosity_MAX);
    spdlog::info("TimeManager Example Application Starting");

    try {
        // Create a TimeManager instance
        atom::web::TimeManager timeManager;
        spdlog::info("TimeManager instance created successfully");

        // Example 1: Get current system time
        std::time_t currentTime = timeManager.getSystemTime();
        spdlog::info("Current system time: {}", formatTime(currentTime));

        // Example 2: Get system time with higher precision
        auto timePoint = timeManager.getSystemTimePoint();
        auto timeT = std::chrono::system_clock::to_time_t(timePoint);
        spdlog::info("Current system time (high precision): {}", formatTime(timeT));

        // Check for admin/root privileges
        bool hasAdminPrivileges = timeManager.hasAdminPrivileges();
        spdlog::info("Administrator/root privileges check: {}", hasAdminPrivileges ? "Yes" : "No");
        std::cout << "Has administrator/root privileges: " << (hasAdminPrivileges ? "Yes" : "No") << std::endl;

        // If we have admin privileges, we can try to set the system time
        if (hasAdminPrivileges) {
            // Example 4: Set system time
            spdlog::info("Setting system time to 2025-01-01 12:00:00...");
            std::error_code ec = timeManager.setSystemTime(2025, 1, 1, 12, 0, 0);

            if (ec) {
                spdlog::error("Failed to set system time: {}", ec.message());
            } else {
                spdlog::info("System time set successfully");
                currentTime = timeManager.getSystemTime();
                spdlog::info("New system time: {}", formatTime(currentTime));
            }

// Example 5: Set system timezone
#ifdef _WIN32
            std::string timezone = "Pacific Standard Time";
#else
            std::string timezone = "America/Los_Angeles";
#endif
            spdlog::info("Setting system timezone to {}", timezone);
            ec = timeManager.setSystemTimezone(timezone);

            if (ec) {
                spdlog::error("Failed to set timezone: {}", ec.message());
            } else {
                spdlog::info("Timezone set successfully to {}", timezone);
            }

            // Example 6: Sync time from RTC
            spdlog::info("Syncing time from RTC...");
            ec = timeManager.syncTimeFromRTC();

            if (ec) {
                spdlog::error("Failed to sync time from RTC: {}", ec.message());
            } else {
                spdlog::info("Time synced from RTC successfully");
                currentTime = timeManager.getSystemTime();
                spdlog::info("System time after RTC sync: {}", formatTime(currentTime));
            }
        } else {
            spdlog::warn("Administrator/root privileges required for setting time and timezone");
            std::cout << "Administrator/root privileges required for setting time and timezone" << std::endl;
        }

        // Example 7: Get time from NTP server (doesn't require admin
        // privileges)
        spdlog::info("Getting time from NTP server...");
        std::vector<std::string> ntpServers = {
            "pool.ntp.org", "time.google.com", "time.windows.com",
            "time.apple.com", "time-a-g.nist.gov"
        };

        bool ntpSuccess = false;
        for (const auto& server : ntpServers) {
            spdlog::info("Attempting to get time from NTP server: {}", server);
            auto ntpTime = timeManager.getNtpTime(server, std::chrono::seconds(2));
            if (ntpTime) {
                spdlog::info("NTP time from {}: {}", server, formatTime(*ntpTime));
                std::time_t systemTime = timeManager.getSystemTime();
                double diffSeconds = std::difftime(systemTime, *ntpTime);
                spdlog::info("System time differs from NTP time by {:.2f} seconds", diffSeconds);
                ntpSuccess = true;
                break;
            } else {
                spdlog::warn("Failed to get time from NTP server: {}", server);
            }
        }

        if (!ntpSuccess) {
            spdlog::error("Failed to get time from any NTP server");
        }

        // Example 8: Demonstrate caching behavior of NTP time
        if (ntpSuccess) {
            spdlog::info("Demonstrating NTP time caching behavior");
            auto startTime = std::chrono::high_resolution_clock::now();
            auto ntpTime1 = timeManager.getNtpTime("pool.ntp.org");
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            spdlog::info("Time from NTP: {}", formatTime(*ntpTime1));
            spdlog::info("First NTP request took {} ms", duration);

            startTime = std::chrono::high_resolution_clock::now();
            auto ntpTime2 = timeManager.getNtpTime("pool.ntp.org");
            endTime = std::chrono::high_resolution_clock::now();
            duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            spdlog::info("Time from NTP cache: {}", formatTime(*ntpTime2));
            spdlog::info("Second (cached) NTP request took {} ms", duration);
        }

        // Example 9: Testing error handling with invalid parameters
        spdlog::info("Testing error handling with invalid parameters");
        std::error_code ec = timeManager.setSystemTime(2025, 2, 30, 12, 0, 0);
        spdlog::info("Setting invalid date (Feb 30): {}", ec ? "Failed as expected: " + ec.message() : "Unexpectedly succeeded");

        ec = timeManager.setSystemTime(2025, 1, 1, 25, 0, 0);
        spdlog::info("Setting invalid time (hour 25): {}", ec ? "Failed as expected: " + ec.message() : "Unexpectedly succeeded");

        ec = timeManager.setSystemTimezone("NonExistentTimeZone");
        spdlog::info("Setting invalid timezone: {}", ec ? "Failed as expected: " + ec.message() : "Unexpectedly succeeded");

        auto ntpTime = timeManager.getNtpTime("this-does-not-exist.example.com");
        spdlog::info("Using invalid NTP server: {}", ntpTime ? "Unexpectedly succeeded" : "Failed as expected");

        spdlog::info("TimeManager example completed successfully");
        std::cout << "\nTimeManager example completed successfully" << std::endl;

    } catch (const std::exception& e) {
        spdlog::error("Exception caught: {}", e.what());
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}