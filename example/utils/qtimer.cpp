/*
 * elapsed_timer_example.cpp
 *
 * This example demonstrates the usage of the ElapsedTimer class in the
 * atom::utils namespace. It covers creation, time measurement, comparison
 * operations, and various utility functions for working with time intervals.
 *
 * Copyright (C) 2024 Example User
 */

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "atom/utils/qtimer.hpp"

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

// Helper function to print subsection headers
void printSubsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

// Helper function to format time durations
template <typename T>
std::string formatDuration(T value, const std::string& unit) {
    std::ostringstream oss;
    oss << value << " " << unit << (value == 1 ? "" : "s");
    return oss.str();
}

// Helper function to simulate work
void simulateWork(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// Simple benchmark function to measure execution time of a given function
template <typename Func>
int64_t benchmark(Func func, const std::string& description) {
    atom::utils::ElapsedTimer timer(true);

    func();

    int64_t elapsed = timer.elapsedMs();
    std::cout << "Operation '" << description << "' took "
              << formatDuration(elapsed, "millisecond") << std::endl;

    return elapsed;
}

int main() {
    std::cout << "=================================================="
              << std::endl;
    std::cout << "ElapsedTimer Class Comprehensive Usage Example" << std::endl;
    std::cout << "=================================================="
              << std::endl;

    // ==========================================
    // 1. Creating and Starting Timers
    // ==========================================
    printSection("1. Creating and Starting Timers");

    // Create an unstarted timer
    printSubsection("Default Construction");
    atom::utils::ElapsedTimer unstartedTimer;
    std::cout << "Timer created but not started." << std::endl;
    std::cout << "Is timer valid? " << (unstartedTimer.isValid() ? "Yes" : "No")
              << std::endl;

    // Start the timer
    printSubsection("Starting a Timer");
    unstartedTimer.start();
    std::cout << "Timer started." << std::endl;
    std::cout << "Is timer valid? " << (unstartedTimer.isValid() ? "Yes" : "No")
              << std::endl;

    // Create a timer that starts immediately
    printSubsection("Immediate Start Construction");
    atom::utils::ElapsedTimer immediateTimer(true);
    std::cout << "Timer created and started immediately." << std::endl;
    std::cout << "Is timer valid? " << (immediateTimer.isValid() ? "Yes" : "No")
              << std::endl;

    // ==========================================
    // 2. Measuring Elapsed Time in Different Units
    // ==========================================
    printSection("2. Measuring Elapsed Time in Different Units");

    // Create a timer that starts immediately
    atom::utils::ElapsedTimer multiUnitTimer(true);

    // Simulate some work
    std::cout << "Performing work for 1.5 seconds..." << std::endl;
    simulateWork(1500);

    // Display elapsed time in different units
    std::cout << "Elapsed time in various units:" << std::endl;
    std::cout << "  Nanoseconds:  "
              << formatDuration(multiUnitTimer.elapsedNs(), "nanosecond")
              << std::endl;
    std::cout << "  Microseconds: "
              << formatDuration(multiUnitTimer.elapsedUs(), "microsecond")
              << std::endl;
    std::cout << "  Milliseconds: "
              << formatDuration(multiUnitTimer.elapsedMs(), "millisecond")
              << std::endl;
    std::cout << "  Seconds:      "
              << formatDuration(multiUnitTimer.elapsedSec(), "second")
              << std::endl;
    std::cout << "  Minutes:      "
              << formatDuration(multiUnitTimer.elapsedMin(), "minute")
              << std::endl;
    std::cout << "  Hours:        "
              << formatDuration(multiUnitTimer.elapsedHrs(), "hour")
              << std::endl;
    std::cout << "  Default:      "
              << formatDuration(multiUnitTimer.elapsed(), "millisecond")
              << " (same as milliseconds)" << std::endl;

    // Using the template method for custom duration types
    std::cout << "\nUsing template methods for custom duration types:"
              << std::endl;
    std::cout << "  Nanoseconds:  "
              << formatDuration(
                     multiUnitTimer.elapsed<std::chrono::nanoseconds>(),
                     "nanosecond")
              << std::endl;
    std::cout << "  Microseconds: "
              << formatDuration(
                     multiUnitTimer.elapsed<std::chrono::microseconds>(),
                     "microsecond")
              << std::endl;
    std::cout << "  Seconds:      "
              << formatDuration(multiUnitTimer.elapsed<std::chrono::seconds>(),
                                "second")
              << std::endl;

    // ==========================================
    // 3. Restarting and Invalidating Timers
    // ==========================================
    printSection("3. Restarting and Invalidating Timers");

    // Create and start a timer
    atom::utils::ElapsedTimer restartTimer(true);

    // Simulate initial work
    std::cout << "Performing initial work for 500ms..." << std::endl;
    simulateWork(500);
    std::cout << "Initial elapsed time: "
              << formatDuration(restartTimer.elapsedMs(), "millisecond")
              << std::endl;

    // Restart the timer
    printSubsection("Restarting a Timer");
    restartTimer.start();
    std::cout << "Timer restarted." << std::endl;

    // Simulate more work
    std::cout << "Performing more work for 300ms..." << std::endl;
    simulateWork(300);
    std::cout << "New elapsed time: "
              << formatDuration(restartTimer.elapsedMs(), "millisecond")
              << std::endl;

    // Invalidate the timer
    printSubsection("Invalidating a Timer");
    restartTimer.invalidate();
    std::cout << "Timer invalidated." << std::endl;
    std::cout << "Is timer valid? " << (restartTimer.isValid() ? "Yes" : "No")
              << std::endl;
    std::cout << "Elapsed time after invalidation: "
              << formatDuration(restartTimer.elapsedMs(), "millisecond")
              << std::endl;

    // ==========================================
    // 4. Using Timeout Functions
    // ==========================================
    printSection("4. Using Timeout Functions");

    // Create and start a timer
    atom::utils::ElapsedTimer timeoutTimer(true);

    // Check for expiration
    printSubsection("Checking if Timer has Expired");

    int checkPoints[] = {100, 300, 600};

    for (int timeoutMs : checkPoints) {
        std::cout << "Checking if " << timeoutMs << "ms has expired..."
                  << std::endl;
        bool expired = timeoutTimer.hasExpired(timeoutMs);
        std::cout << "Elapsed: " << timeoutTimer.elapsedMs()
                  << "ms, Expired: " << (expired ? "Yes" : "No") << std::endl;

        if (!expired) {
            int64_t remaining = timeoutTimer.remainingTimeMs(timeoutMs);
            std::cout << "Remaining time: "
                      << formatDuration(remaining, "millisecond") << std::endl;
        }

        // Wait a bit before next check
        simulateWork(200);
    }

    // ==========================================
    // 5. Error Handling
    // ==========================================
    printSection("5. Error Handling");

    // Try with invalid timer
    printSubsection("Using Invalid Timer");
    atom::utils::ElapsedTimer invalidTimer;
    invalidTimer.invalidate();  // Ensure it's invalid

    std::cout << "Elapsed time with invalid timer: " << invalidTimer.elapsedMs()
              << "ms" << std::endl;
    std::cout << "Has expired with invalid timer: "
              << (invalidTimer.hasExpired(1000) ? "Yes" : "No") << std::endl;
    std::cout << "Remaining time with invalid timer: "
              << invalidTimer.remainingTimeMs(1000) << "ms" << std::endl;

    // Try with negative timeout
    printSubsection("Using Negative Timeout");
    atom::utils::ElapsedTimer validTimer(true);

    try {
        std::cout << "Attempting to check expiration with negative timeout..."
                  << std::endl;
        bool expired = validTimer.hasExpired(-1000);
        std::cout << "This line should not be reached." << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught exception as expected: " << e.what() << std::endl;
    }

    try {
        std::cout
            << "Attempting to check remaining time with negative timeout..."
            << std::endl;
        int64_t remaining = validTimer.remainingTimeMs(-1000);
        std::cout << "This line should not be reached." << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught exception as expected: " << e.what() << std::endl;
    }

    // Using template method with throw_if_invalid set to true
    printSubsection("Using Template Method with throw_if_invalid");
    try {
        std::cout
            << "Attempting to get elapsed time with throw_if_invalid=true..."
            << std::endl;
        int64_t elapsed =
            invalidTimer.elapsed<std::chrono::milliseconds, true>();
        std::cout << "This line should not be reached." << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught exception as expected: " << e.what() << std::endl;
    }

    // ==========================================
    // 6. Static Current Time
    // ==========================================
    printSection("6. Static Current Time");

    int64_t currentTime = atom::utils::ElapsedTimer::currentTimeMs();
    std::cout << "Current time since epoch: " << currentTime << "ms"
              << std::endl;

    // Display in a more human-readable format
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_time);

    std::cout << "Current local time: "
              << std::put_time(now_tm, "%Y-%m-%d %H:%M:%S") << std::endl;

    // ==========================================
    // 7. Comparing Timers
    // ==========================================
    printSection("7. Comparing Timers");

    // Create multiple timers with different start times
    atom::utils::ElapsedTimer firstTimer(true);
    simulateWork(100);

    atom::utils::ElapsedTimer secondTimer(true);
    simulateWork(100);

    atom::utils::ElapsedTimer thirdTimer(true);

    atom::utils::ElapsedTimer invalidCompareTimer;

    // Display timer start times
    std::cout << "First timer started " << firstTimer.elapsedMs() << "ms ago"
              << std::endl;
    std::cout << "Second timer started " << secondTimer.elapsedMs() << "ms ago"
              << std::endl;
    std::cout << "Third timer started " << thirdTimer.elapsedMs() << "ms ago"
              << std::endl;
    std::cout << "Fourth timer is invalid" << std::endl;

    // Compare timers
    printSubsection("Equality Comparisons");

    std::cout << "First == Second: "
              << (firstTimer == secondTimer ? "true" : "false") << std::endl;
    std::cout << "First != Second: "
              << (firstTimer != secondTimer ? "true" : "false") << std::endl;

    // Clone a timer for equality testing
    atom::utils::ElapsedTimer cloneTimer = firstTimer;
    std::cout << "Clone == First: "
              << (cloneTimer == firstTimer ? "true" : "false") << std::endl;

    // Compare invalid timers
    std::cout << "Invalid == Invalid: "
              << (invalidCompareTimer == invalidCompareTimer ? "true" : "false")
              << std::endl;
    std::cout << "Invalid == Valid: "
              << (invalidCompareTimer == firstTimer ? "true" : "false")
              << std::endl;

    printSubsection("Ordering Comparisons");

    std::cout << "First < Second: "
              << (firstTimer < secondTimer ? "true" : "false") << std::endl;
    std::cout << "First <= Second: "
              << (firstTimer <= secondTimer ? "true" : "false") << std::endl;
    std::cout << "First > Second: "
              << (firstTimer > secondTimer ? "true" : "false") << std::endl;
    std::cout << "First >= Second: "
              << (firstTimer >= secondTimer ? "true" : "false") << std::endl;

    // Compare with invalid timer (invalid is always less than valid)
    std::cout << "Invalid < Valid: "
              << (invalidCompareTimer < firstTimer ? "true" : "false")
              << std::endl;
    std::cout << "Valid > Invalid: "
              << (firstTimer > invalidCompareTimer ? "true" : "false")
              << std::endl;

    // ==========================================
    // 8. Practical Use Cases
    // ==========================================
    printSection("8. Practical Use Cases");

    // Case 1: Function performance benchmarking
    printSubsection("Function Benchmarking");

    // Benchmark a sorting operation
    benchmark(
        []() {
            std::vector<int> data(100000);
            for (int i = 0; i < 100000; ++i) {
                data[i] = rand() % 100000;
            }
            std::sort(data.begin(), data.end());
        },
        "Sorting 100,000 integers");

    // Case 2: Implementing a timeout-based operation
    printSubsection("Timeout-Based Operation");

    atom::utils::ElapsedTimer timeoutOperationTimer(true);
    const int64_t OPERATION_TIMEOUT_MS = 500;
    bool operationSuccess = false;

    std::cout << "Starting operation with " << OPERATION_TIMEOUT_MS
              << "ms timeout..." << std::endl;

    // Simulate an operation that may succeed or time out
    while (!timeoutOperationTimer.hasExpired(OPERATION_TIMEOUT_MS)) {
        // Simulate work steps
        simulateWork(100);

        std::cout << "Operation step completed. Elapsed: "
                  << timeoutOperationTimer.elapsedMs() << "ms" << std::endl;

        // Simulate a success condition (randomly)
        if (rand() % 10 == 0) {
            operationSuccess = true;
            break;
        }
    }

    if (operationSuccess) {
        std::cout << "Operation completed successfully within timeout!"
                  << std::endl;
    } else {
        std::cout << "Operation timed out after "
                  << timeoutOperationTimer.elapsedMs() << "ms" << std::endl;
    }

    // Case 3: Rate limiting
    printSubsection("Rate Limiting");

    const int RATE_LIMIT_MS = 200;  // Allow operations every 200ms
    atom::utils::ElapsedTimer rateLimitTimer(true);

    for (int i = 1; i <= 5; ++i) {
        std::cout << "Attempting operation " << i << "..." << std::endl;

        // Check if enough time has passed since the last operation
        if (rateLimitTimer.elapsedMs() < RATE_LIMIT_MS) {
            int64_t waitTime = rateLimitTimer.remainingTimeMs(RATE_LIMIT_MS);
            std::cout << "Rate limit hit. Waiting for " << waitTime << "ms"
                      << std::endl;
            simulateWork(waitTime);
        }

        // Perform the operation
        std::cout << "Performing operation " << i << std::endl;

        // Reset the timer for the next rate limit check
        rateLimitTimer.start();
    }

    // Case 4: Creating a simple profiler
    printSubsection("Simple Profiler");

    class SimpleProfiler {
    private:
        atom::utils::ElapsedTimer timer;
        std::string operationName;

    public:
        SimpleProfiler(const std::string& name) : operationName(name) {
            timer.start();
            std::cout << "Starting operation: " << operationName << std::endl;
        }

        ~SimpleProfiler() {
            std::cout << "Operation '" << operationName << "' completed in "
                      << formatDuration(timer.elapsedMs(), "millisecond")
                      << std::endl;
        }
    };

    // Use the simple profiler with RAII
    {
        SimpleProfiler profiler("Complex Calculation");

        // Simulate complex work
        simulateWork(350);
        std::cout << "Step 1 completed" << std::endl;

        simulateWork(250);
        std::cout << "Step 2 completed" << std::endl;

        simulateWork(150);
        std::cout << "Step 3 completed" << std::endl;
    }  // Profiler destructor will print the elapsed time

    // ==========================================
    // 9. Combined Usage Scenarios
    // ==========================================
    printSection("9. Combined Usage Scenarios");

    // Implementing a retry mechanism with exponential backoff
    printSubsection("Retry Mechanism with Exponential Backoff");

    const int MAX_RETRIES = 5;
    int retryCount = 0;
    int baseDelayMs = 100;
    bool operationSuccessful = false;

    atom::utils::ElapsedTimer totalTimeTimer(true);

    while (retryCount < MAX_RETRIES && !operationSuccessful) {
        std::cout << "Attempt " << (retryCount + 1) << " of " << MAX_RETRIES
                  << "..." << std::endl;

        // Simulate an operation that might fail
        atom::utils::ElapsedTimer attemptTimer(true);
        simulateWork(50);  // Simulate work

        // Simulate success/failure (mostly failure for demonstration)
        operationSuccessful = (rand() % 10 == 0);

        std::cout << "Operation "
                  << (operationSuccessful ? "succeeded" : "failed") << " in "
                  << attemptTimer.elapsedMs() << "ms" << std::endl;

        if (!operationSuccessful) {
            retryCount++;

            if (retryCount < MAX_RETRIES) {
                // Calculate exponential backoff time
                int delayMs = baseDelayMs *
                              (1 << retryCount);  // 100, 200, 400, 800, 1600 ms
                std::cout << "Backing off for " << delayMs
                          << "ms before retry..." << std::endl;
                simulateWork(delayMs);
            }
        }
    }

    std::cout << "Operation " << (operationSuccessful ? "succeeded" : "failed")
              << " after " << retryCount
              << " retries. Total time: " << totalTimeTimer.elapsedMs() << "ms"
              << std::endl;

    // ==========================================
    // 10. Performance Comparison
    // ==========================================
    printSection("10. Performance Comparison");

    // Compare the performance of different timing methods
    const int TIMING_ITERATIONS = 100000;

    // Using ElapsedTimer
    atom::utils::ElapsedTimer perfTimer(true);

    for (int i = 0; i < TIMING_ITERATIONS; ++i) {
        atom::utils::ElapsedTimer t(true);
        volatile int64_t elapsed = t.elapsedNs();  // Force evaluation
    }

    int64_t elapsedTimerTime = perfTimer.elapsedMs();
    std::cout << "Creating and using " << TIMING_ITERATIONS
              << " ElapsedTimer objects took " << elapsedTimerTime << "ms"
              << std::endl;

    // Using std::chrono directly
    perfTimer.start();

    for (int i = 0; i < TIMING_ITERATIONS; ++i) {
        auto start = std::chrono::steady_clock::now();
        volatile auto elapsed =
            std::chrono::steady_clock::now() - start;  // Force evaluation
    }

    int64_t elapsedChronoTime = perfTimer.elapsedMs();
    std::cout << "Using std::chrono directly " << TIMING_ITERATIONS
              << " times took " << elapsedChronoTime << "ms" << std::endl;

    // Compare results
    double ratio = static_cast<double>(elapsedTimerTime) / elapsedChronoTime;
    std::cout << "ElapsedTimer is " << std::fixed << std::setprecision(2)
              << ratio << " times the cost of direct chrono usage" << std::endl;

    std::cout << "\n=================================================="
              << std::endl;
    std::cout << "ElapsedTimer Example Completed" << std::endl;
    std::cout << "=================================================="
              << std::endl;

    return 0;
}
