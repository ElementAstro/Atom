#include "atom/web/time.hpp"

#include <ctime>
#include <iostream>

using namespace atom::web;

int main() {
    // Create a TimeManager instance
    TimeManager timeManager;

    // Get the current system time
    std::time_t currentTime = timeManager.getSystemTime();
    std::cout << "Current system time: " << std::ctime(&currentTime);

    // Set the system time
    timeManager.setSystemTime(2024, 1, 1, 12, 0, 0);
    std::cout << "System time set to: 2024-01-01 12:00:00" << std::endl;

    // Set the system timezone
    bool timezoneSet = timeManager.setSystemTimezone("UTC");
    std::cout << "System timezone set to UTC: " << std::boolalpha << timezoneSet
              << std::endl;

    // Synchronize the system time from the Real-Time Clock (RTC)
    bool timeSynced = timeManager.syncTimeFromRTC();
    std::cout << "Time synchronized from RTC: " << std::boolalpha << timeSynced
              << std::endl;

    // Get the Network Time Protocol (NTP) time from a specified hostname
    std::time_t ntpTime = timeManager.getNtpTime("pool.ntp.org");
    std::cout << "NTP time from pool.ntp.org: " << std::ctime(&ntpTime);
    return 0;
}