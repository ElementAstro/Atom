#include "atom/utils/qtimer.hpp"

#include <iostream>
#include <thread>

using namespace atom::utils;

int main() {
    // Create an ElapsedTimer instance
    ElapsedTimer timer;

    // Start the timer
    timer.start();
    std::cout << "Timer started." << std::endl;

    // Sleep for 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Check if the timer is valid
    bool isValid = timer.isValid();
    std::cout << "Timer is valid: " << std::boolalpha << isValid << std::endl;

    // Get elapsed time in various units
    int64_t elapsedNs = timer.elapsedNs();
    int64_t elapsedUs = timer.elapsedUs();
    int64_t elapsedMs = timer.elapsedMs();
    int64_t elapsedSec = timer.elapsedSec();
    int64_t elapsedMin = timer.elapsedMin();
    int64_t elapsedHrs = timer.elapsedHrs();

    std::cout << "Elapsed time: " << elapsedNs << " ns" << std::endl;
    std::cout << "Elapsed time: " << elapsedUs << " us" << std::endl;
    std::cout << "Elapsed time: " << elapsedMs << " ms" << std::endl;
    std::cout << "Elapsed time: " << elapsedSec << " sec" << std::endl;
    std::cout << "Elapsed time: " << elapsedMin << " min" << std::endl;
    std::cout << "Elapsed time: " << elapsedHrs << " hrs" << std::endl;

    // Check if a specified duration has passed
    bool hasExpired = timer.hasExpired(1000);  // 1000 ms = 1 second
    std::cout << "Has 1 second expired: " << std::boolalpha << hasExpired
              << std::endl;

    // Get the remaining time until a specified duration has passed
    int64_t remainingTime = timer.remainingTimeMs(2000);  // 2000 ms = 2 seconds
    std::cout << "Remaining time until 2 seconds: " << remainingTime << " ms"
              << std::endl;

    // Get the current absolute time in milliseconds since epoch
    int64_t currentTime = ElapsedTimer::currentTimeMs();
    std::cout << "Current time in milliseconds since epoch: " << currentTime
              << std::endl;

    // Invalidate the timer
    timer.invalidate();
    std::cout << "Timer invalidated." << std::endl;

    // Compare two ElapsedTimer objects
    ElapsedTimer timer1;
    ElapsedTimer timer2;
    timer1.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    timer2.start();

    bool isLessThan = timer1 < timer2;
    bool isGreaterThan = timer1 > timer2;
    bool isEqual = timer1 == timer2;

    std::cout << "Timer1 is less than Timer2: " << std::boolalpha << isLessThan
              << std::endl;
    std::cout << "Timer1 is greater than Timer2: " << std::boolalpha
              << isGreaterThan << std::endl;
    std::cout << "Timer1 is equal to Timer2: " << std::boolalpha << isEqual
              << std::endl;

    return 0;
}