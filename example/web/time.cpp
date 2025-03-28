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
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

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
    LOG_F(INFO, "TimeManager Example Application Starting");

    try {
        // Create a TimeManager instance
        atom::web::TimeManager timeManager;
        LOG_F(INFO, "TimeManager instance created successfully");

        // Example 1: Get current system time
        std::time_t currentTime = timeManager.getSystemTime();
        std::cout << "Current system time: " << formatTime(currentTime)
                  << std::endl;
        LOG_F(INFO, "Current system time: %s", formatTime(currentTime).c_str());

        // Example 2: Get system time with higher precision
        auto timePoint = timeManager.getSystemTimePoint();
        auto timeT = std::chrono::system_clock::to_time_t(timePoint);
        std::cout << "Current system time (high precision): "
                  << formatTime(timeT) << std::endl;
        LOG_F(INFO, "Fetched system time point and converted to time_t: %s",
              formatTime(timeT).c_str());

        // Example 3: Check if we have administrator/root privileges
        bool hasAdminPrivileges = atom::system::isRoot();
        std::cout << "Has administrator/root privileges: "
                  << (hasAdminPrivileges ? "Yes" : "No") << std::endl;
        LOG_F(INFO, "Administrator/root privileges check: %s",
              hasAdminPrivileges ? "Yes" : "No");

        // If we have admin privileges, we can try to set the system time
        if (hasAdminPrivileges) {
            // Example 4: Set system time
            std::cout << "Setting system time to 2025-01-01 12:00:00..."
                      << std::endl;
            std::error_code ec =
                timeManager.setSystemTime(2025, 1, 1, 12, 0, 0);

            if (ec) {
                std::cout << "Failed to set system time: " << ec.message()
                          << std::endl;
                LOG_F(ERROR, "Failed to set system time: %s",
                      ec.message().c_str());
            } else {
                std::cout << "System time set successfully" << std::endl;
                LOG_F(INFO, "System time set successfully");

                // Verify the time was set
                currentTime = timeManager.getSystemTime();
                std::cout << "New system time: " << formatTime(currentTime)
                          << std::endl;
                LOG_F(INFO, "New system time: %s",
                      formatTime(currentTime).c_str());
            }

// Example 5: Set system timezone
#ifdef _WIN32
            std::string timezone =
                "Pacific Standard Time";  // Windows timezone name
#else
            std::string timezone =
                "America/Los_Angeles";  // POSIX timezone name
#endif

            std::cout << "Setting system timezone to " << timezone << "..."
                      << std::endl;
            ec = timeManager.setSystemTimezone(timezone);

            if (ec) {
                std::cout << "Failed to set timezone: " << ec.message()
                          << std::endl;
                LOG_F(ERROR, "Failed to set timezone: %s",
                      ec.message().c_str());
            } else {
                std::cout << "Timezone set successfully" << std::endl;
                LOG_F(INFO, "Timezone set successfully to %s",
                      timezone.c_str());
            }

            // Example 6: Sync time from RTC
            std::cout << "Syncing time from RTC..." << std::endl;
            ec = timeManager.syncTimeFromRTC();

            if (ec) {
                std::cout << "Failed to sync time from RTC: " << ec.message()
                          << std::endl;
                LOG_F(ERROR, "Failed to sync time from RTC: %s",
                      ec.message().c_str());
            } else {
                std::cout << "Time synced from RTC successfully" << std::endl;
                LOG_F(INFO, "Time synced from RTC successfully");

                // Verify the time after sync
                currentTime = timeManager.getSystemTime();
                std::cout << "System time after RTC sync: "
                          << formatTime(currentTime) << std::endl;
                LOG_F(INFO, "System time after RTC sync: %s",
                      formatTime(currentTime).c_str());
            }
        } else {
            std::cout << "Administrator/root privileges required for setting "
                         "time and timezone"
                      << std::endl;
            LOG_F(WARNING,
                  "Administrator/root privileges required for setting time and "
                  "timezone");
        }

        // Example 7: Get time from NTP server (doesn't require admin
        // privileges)
        std::cout << "Getting time from NTP server..." << std::endl;

        // List of NTP servers to try
        std::vector<std::string> ntpServers = {
            "pool.ntp.org", "time.google.com", "time.windows.com",
            "time.apple.com", "time-a-g.nist.gov"};

        bool ntpSuccess = false;
        for (const auto& server : ntpServers) {
            std::cout << "Trying NTP server: " << server << std::endl;
            LOG_F(INFO, "Attempting to get time from NTP server: %s",
                  server.c_str());

            auto ntpTime =
                timeManager.getNtpTime(server, std::chrono::seconds(2));
            if (ntpTime) {
                std::cout << "NTP time from " << server << ": "
                          << formatTime(*ntpTime) << std::endl;
                LOG_F(INFO, "NTP time from %s: %s", server.c_str(),
                      formatTime(*ntpTime).c_str());

                // Calculate time difference between system and NTP
                std::time_t systemTime = timeManager.getSystemTime();
                double diffSeconds = std::difftime(systemTime, *ntpTime);
                std::cout << "System time differs from NTP time by "
                          << diffSeconds << " seconds" << std::endl;
                LOG_F(INFO, "System time differs from NTP time by %.2f seconds",
                      diffSeconds);

                ntpSuccess = true;
                break;
            } else {
                std::cout << "Failed to get time from NTP server: " << server
                          << std::endl;
                LOG_F(WARNING, "Failed to get time from NTP server: %s",
                      server.c_str());
            }
        }

        if (!ntpSuccess) {
            std::cout << "Failed to get time from any NTP server" << std::endl;
            LOG_F(ERROR, "Failed to get time from any NTP server");
        }

        // Example 8: Demonstrate caching behavior of NTP time
        if (ntpSuccess) {
            std::cout << "\nDemonstrating NTP cache...\n" << std::endl;
            LOG_F(INFO, "Demonstrating NTP time caching behavior");

            std::cout << "First call (should use network):" << std::endl;
            auto startTime = std::chrono::high_resolution_clock::now();
            auto ntpTime1 = timeManager.getNtpTime("pool.ntp.org");
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                                      startTime)
                    .count();
            std::cout << "Time from NTP: " << formatTime(*ntpTime1)
                      << std::endl;
            std::cout << "Request took " << duration << " ms" << std::endl;

            std::cout << "\nSecond call (should use cache):" << std::endl;
            startTime = std::chrono::high_resolution_clock::now();
            auto ntpTime2 = timeManager.getNtpTime("pool.ntp.org");
            endTime = std::chrono::high_resolution_clock::now();
            duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                           endTime - startTime)
                           .count();
            std::cout << "Time from NTP cache: " << formatTime(*ntpTime2)
                      << std::endl;
            std::cout << "Request took " << duration << " ms" << std::endl;

            LOG_F(INFO,
                  "First NTP request took %lld ms, second (cached) request "
                  "took %lld ms",
                  std::chrono::duration_cast<std::chrono::milliseconds>(
                      endTime - startTime)
                      .count(),
                  duration);
        }

        // Example 9: Testing error handling with invalid parameters
        std::cout << "\nTesting error handling with invalid parameters:\n"
                  << std::endl;
        LOG_F(INFO, "Testing error handling with invalid parameters");

        // Invalid date (February 30th)
        std::error_code ec = timeManager.setSystemTime(2025, 2, 30, 12, 0, 0);
        std::cout << "Setting invalid date (Feb 30): "
                  << (ec ? "Failed as expected: " + ec.message()
                         : "Unexpectedly succeeded")
                  << std::endl;
        LOG_F(INFO, "Invalid date test: %s",
              ec ? "Failed as expected" : "Unexpectedly succeeded");

        // Invalid time
        ec = timeManager.setSystemTime(2025, 1, 1, 25, 0, 0);
        std::cout << "Setting invalid time (hour 25): "
                  << (ec ? "Failed as expected: " + ec.message()
                         : "Unexpectedly succeeded")
                  << std::endl;
        LOG_F(INFO, "Invalid time test: %s",
              ec ? "Failed as expected" : "Unexpectedly succeeded");

        // Invalid timezone
        ec = timeManager.setSystemTimezone("NonExistentTimeZone");
        std::cout << "Setting invalid timezone: "
                  << (ec ? "Failed as expected: " + ec.message()
                         : "Unexpectedly succeeded")
                  << std::endl;
        LOG_F(INFO, "Invalid timezone test: %s",
              ec ? "Failed as expected" : "Unexpectedly succeeded");

        // Invalid NTP server
        auto ntpTime =
            timeManager.getNtpTime("this-does-not-exist.example.com");
        std::cout << "Using invalid NTP server: "
                  << (ntpTime ? "Unexpectedly succeeded" : "Failed as expected")
                  << std::endl;
        LOG_F(INFO, "Invalid NTP server test: %s",
              ntpTime ? "Unexpectedly succeeded" : "Failed as expected");

        std::cout << "\nTimeManager example completed successfully"
                  << std::endl;
        LOG_F(INFO, "TimeManager example completed successfully");

    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        LOG_F(ERROR, "Exception caught: %s", e.what());
        return 1;
    }

    return 0;
}