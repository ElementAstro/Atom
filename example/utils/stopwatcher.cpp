#include "atom/utils/stopwatcher.hpp"

#include <iostream>
#include <thread>

using namespace atom::utils;

int main() {
    // Create a StopWatcher instance
    StopWatcher sw;

    // Start the stopwatch
    sw.start();
    std::cout << "Stopwatch started." << std::endl;

    // Sleep for 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Record a lap time
    sw.lap();
    std::cout << "Lap recorded." << std::endl;

    // Sleep for another 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Pause the stopwatch
    sw.pause();
    std::cout << "Stopwatch paused." << std::endl;

    // Sleep for another 1 second (this time won't be counted)
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Resume the stopwatch
    sw.resume();
    std::cout << "Stopwatch resumed." << std::endl;

    // Sleep for another 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Stop the stopwatch
    sw.stop();
    std::cout << "Stopwatch stopped." << std::endl;

    // Get the elapsed time in milliseconds
    double elapsedMs = sw.elapsedMilliseconds();
    std::cout << "Elapsed time: " << elapsedMs << " ms" << std::endl;

    // Get the elapsed time in seconds
    double elapsedSec = sw.elapsedSeconds();
    std::cout << "Elapsed time: " << elapsedSec << " sec" << std::endl;

    // Get the elapsed time as a formatted string
    std::string elapsedFormatted = sw.elapsedFormatted();
    std::cout << "Elapsed time: " << elapsedFormatted << std::endl;

    // Get the current state of the stopwatch
    StopWatcherState state = sw.getState();
    std::cout << "Current state: " << static_cast<int>(state) << std::endl;

    // Get all recorded lap times
    auto lapTimes = sw.getLapTimes();
    std::cout << "Lap times: ";
    for (double lapTime : lapTimes) {
        std::cout << lapTime << " ms ";
    }
    std::cout << std::endl;

    // Get the average lap time
    double averageLapTime = sw.getAverageLapTime();
    std::cout << "Average lap time: " << averageLapTime << " ms" << std::endl;

    // Register a callback to be called after 2 seconds
    sw.registerCallback(
        []() {
            std::cout << "Callback triggered after 2 seconds." << std::endl;
        },
        2000);

    // Reset the stopwatch
    sw.reset();
    std::cout << "Stopwatch reset." << std::endl;

    return 0;
}