/*
 * stopwatcher_example.cpp
 *
 * Copyright (C) 2024 Max Q.
 *
 * Example usage of the atom::utils::StopWatcher class
 */

#include "atom/utils/stopwatcher.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

// Helper functions for demonstration
void performTask(const std::string& taskName, int durationMs) {
    std::cout << "Starting task: " << taskName << " (" << durationMs << "ms)"
              << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));
    std::cout << "Finished task: " << taskName << std::endl;
}

// Helper function to print stopwatch state
std::string stateToString(atom::utils::StopWatcherState state) {
    switch (state) {
        case atom::utils::StopWatcherState::Idle:
            return "Idle";
        case atom::utils::StopWatcherState::Running:
            return "Running";
        case atom::utils::StopWatcherState::Paused:
            return "Paused";
        case atom::utils::StopWatcherState::Stopped:
            return "Stopped";
        default:
            return "Unknown";
    }
}

// Helper function to print lap times
void printLapTimes(std::span<const double> lapTimes) {
    std::cout << "Lap times:" << std::endl;
    for (size_t i = 0; i < lapTimes.size(); ++i) {
        std::cout << "  Lap " << (i + 1) << ": " << std::fixed
                  << std::setprecision(3) << lapTimes[i] << " ms" << std::endl;
    }
}

int main() {
    std::cout << "=== StopWatcher Comprehensive Example ===" << std::endl
              << std::endl;

    std::cout << "Example 1: Basic timing operations" << std::endl;
    {
        atom::utils::StopWatcher timer;

        std::cout << "Initial state: " << stateToString(timer.getState())
                  << std::endl;

        // Start the timer
        timer.start();
        std::cout << "After start(): " << stateToString(timer.getState())
                  << std::endl;

        // Perform a task
        performTask("Basic operation", 100);

        // Stop the timer
        timer.stop();
        std::cout << "After stop(): " << stateToString(timer.getState())
                  << std::endl;

        // Display elapsed time in different formats
        std::cout << "Elapsed time (ms): " << timer.elapsedMilliseconds()
                  << " ms" << std::endl;
        std::cout << "Elapsed time (s): " << timer.elapsedSeconds() << " s"
                  << std::endl;
        std::cout << "Elapsed time (formatted): " << timer.elapsedFormatted()
                  << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Example 2: Pause and resume operations" << std::endl;
    {
        atom::utils::StopWatcher timer;

        timer.start();
        performTask("First segment", 100);

        // Pause the timer
        timer.pause();
        std::cout << "Timer paused. State: " << stateToString(timer.getState())
                  << std::endl;
        std::cout << "Time at pause: " << timer.elapsedMilliseconds() << " ms"
                  << std::endl;

        // This work won't be timed
        performTask("Untimed work", 200);

        // Resume the timer
        timer.resume();
        std::cout << "Timer resumed. State: " << stateToString(timer.getState())
                  << std::endl;

        performTask("Second segment", 150);

        timer.stop();
        std::cout << "Final time (should exclude pause): "
                  << timer.elapsedMilliseconds() << " ms" << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Example 3: Lap timing" << std::endl;
    {
        atom::utils::StopWatcher timer;
        timer.start();

        // Record several lap times
        performTask("Lap 1 work", 100);
        double lap1 = timer.lap();
        std::cout << "Lap 1 time: " << lap1 << " ms" << std::endl;

        performTask("Lap 2 work", 150);
        double lap2 = timer.lap();
        std::cout << "Lap 2 time: " << lap2 << " ms" << std::endl;

        performTask("Lap 3 work", 75);
        double lap3 = timer.lap();
        std::cout << "Lap 3 time: " << lap3 << " ms" << std::endl;

        timer.stop();

        // Get all lap times
        auto lapTimes = timer.getLapTimes();
        printLapTimes(lapTimes);

        // Get lap statistics
        std::cout << "Number of laps: " << timer.getLapCount() << std::endl;
        std::cout << "Average lap time: " << timer.getAverageLapTime() << " ms"
                  << std::endl;

        // Note: Total elapsed time includes all laps
        std::cout << "Total elapsed time: " << timer.elapsedMilliseconds()
                  << " ms" << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Example 4: Reset functionality" << std::endl;
    {
        atom::utils::StopWatcher timer;
        timer.start();
        performTask("Initial task", 100);
        timer.stop();
        std::cout << "Time before reset: " << timer.elapsedMilliseconds()
                  << " ms" << std::endl;

        // Reset the timer
        timer.reset();
        std::cout << "State after reset: " << stateToString(timer.getState())
                  << std::endl;

        // Start fresh timing
        timer.start();
        performTask("Task after reset", 150);
        timer.stop();
        std::cout << "Time after reset and new task: "
                  << timer.elapsedMilliseconds() << " ms" << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Example 5: Callbacks" << std::endl;
    {
        atom::utils::StopWatcher timer;
        std::atomic<bool> callbackCalled = false;

        // Register a callback to be triggered after 200ms
        timer.registerCallback(
            [&callbackCalled]() {
                std::cout << "Callback triggered!" << std::endl;
                callbackCalled = true;
            },
            200);

        timer.start();

        // Wait for callback to be potentially triggered (not yet)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "After 100ms - Callback triggered: "
                  << (callbackCalled ? "Yes" : "No") << std::endl;

        // Wait more until callback should be triggered
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        timer.stop();
        std::cout << "After 250ms - Callback triggered: "
                  << (callbackCalled ? "Yes" : "No") << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Example 6: Using StopWatcher to profile code" << std::endl;
    {
        // Create a vector with random data
        std::vector<int> data(50000);
        std::generate(data.begin(), data.end(),
                      []() { return rand() % 10000; });

        atom::utils::StopWatcher profiler;

        // Profile sorting algorithm
        profiler.start();
        std::sort(data.begin(), data.end());
        profiler.lap();
        std::cout << "Time to sort 50,000 integers: "
                  << profiler.getLapTimes().back() << " ms" << std::endl;

        // Profile searching algorithm
        int searchValue = rand() % 10000;
        std::binary_search(data.begin(), data.end(), searchValue);
        profiler.lap();
        std::cout << "Time to binary search: "
                  << (profiler.getLapTimes()[1] - profiler.getLapTimes()[0])
                  << " ms" << std::endl;

        // Profile reverse algorithm
        std::reverse(data.begin(), data.end());
        profiler.lap();
        std::cout << "Time to reverse vector: "
                  << (profiler.getLapTimes()[2] - profiler.getLapTimes()[1])
                  << " ms" << std::endl;

        profiler.stop();
        std::cout << "Total profiling time: " << profiler.elapsedMilliseconds()
                  << " ms" << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Example 7: Error handling" << std::endl;
    {
        atom::utils::StopWatcher timer;

        // Try to stop before starting
        std::cout << "Attempting to stop before starting: ";
        bool result = timer.stop();
        std::cout << (result ? "Succeeded" : "Failed") << std::endl;

        // Try to pause before starting
        std::cout << "Attempting to pause before starting: ";
        try {
            timer.pause();
            std::cout << "Succeeded" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Failed - " << e.what() << std::endl;
        }

        // Start and then try to start again
        timer.start();
        std::cout << "Attempting to start timer that's already running: ";
        try {
            timer.start();
            std::cout << "Succeeded" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Failed - " << e.what() << std::endl;
        }

        // Try to lap while not running
        timer.stop();
        std::cout << "Attempting to record lap while stopped: ";
        try {
            timer.lap();
            std::cout << "Succeeded" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Failed - " << e.what() << std::endl;
        }

        // Try to register callback with negative time
        std::cout << "Attempting to register callback with negative time: ";
        try {
            timer.registerCallback([]() {}, -100);
            std::cout << "Succeeded" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Failed - " << e.what() << std::endl;
        }
    }
    std::cout << std::endl;

    std::cout << "Example 8: Move operations" << std::endl;
    {
        atom::utils::StopWatcher timer1;
        timer1.start();
        performTask("Task for timer1", 100);

        // Move construction
        atom::utils::StopWatcher timer2(std::move(timer1));
        std::cout << "State of moved-to timer: "
                  << stateToString(timer2.getState()) << std::endl;

        // Continue using the moved-to timer
        performTask("Task for timer2", 100);
        timer2.stop();
        std::cout << "Elapsed time from moved timer: "
                  << timer2.elapsedMilliseconds() << " ms" << std::endl;

        // Move assignment
        atom::utils::StopWatcher timer3;
        timer3.start();
        performTask("Task for timer3", 50);

        atom::utils::StopWatcher timer4;
        timer4 = std::move(timer3);

        performTask("Task for timer4", 50);
        timer4.stop();
        std::cout << "Elapsed time from move-assigned timer: "
                  << timer4.elapsedMilliseconds() << " ms" << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Example 9: Checking timer state" << std::endl;
    {
        atom::utils::StopWatcher timer;

        std::cout << "Initial isRunning(): "
                  << (timer.isRunning() ? "True" : "False") << std::endl;

        timer.start();
        std::cout << "After start() - isRunning(): "
                  << (timer.isRunning() ? "True" : "False") << std::endl;

        timer.pause();
        std::cout << "After pause() - isRunning(): "
                  << (timer.isRunning() ? "True" : "False") << std::endl;

        timer.resume();
        std::cout << "After resume() - isRunning(): "
                  << (timer.isRunning() ? "True" : "False") << std::endl;

        timer.stop();
        std::cout << "After stop() - isRunning(): "
                  << (timer.isRunning() ? "True" : "False") << std::endl;
    }

    return 0;
}