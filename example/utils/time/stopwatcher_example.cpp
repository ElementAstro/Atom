/**
 * @file stopwatcher_example.cpp
 * @brief Examples for atom::utils StopWatcher
 */

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "atom/utils/time/stopwatcher.hpp"

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void demonstrateBasicUsage() {
    printSection("1. Basic StopWatcher Usage");

    StopWatcher sw("BasicTimer");

    std::cout << "Starting stopwatch..." << std::endl;
    auto startResult = sw.start();
    if (!startResult) {
        std::cout << "Failed to start" << std::endl;
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto stopResult = sw.stop();
    if (stopResult) {
        std::cout << "Elapsed time:" << std::endl;
        std::cout << "  Milliseconds: " << sw.elapsedMilliseconds() << " ms"
                  << std::endl;
        std::cout << "  Seconds: " << sw.elapsedSeconds() << " s" << std::endl;
        std::cout << "  Formatted: " << sw.elapsedFormatted() << std::endl;
    }
}

void demonstratePauseResume() {
    printSection("2. Pause and Resume");

    StopWatcher sw("PauseResumeTimer");

    sw.start();
    std::cout << "Running for 50ms..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    sw.pause();
    std::cout << "Paused. Time so far: " << sw.elapsedMilliseconds() << " ms"
              << std::endl;

    std::cout << "Sleeping 100ms while paused..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Time after sleep (should be same): "
              << sw.elapsedMilliseconds() << " ms" << std::endl;

    sw.resume();
    std::cout << "Resumed. Running for another 50ms..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    sw.stop();
    std::cout << "Final time: " << sw.elapsedMilliseconds() << " ms"
              << std::endl;
    std::cout << "(Should be ~100ms, not ~200ms)" << std::endl;
}

void demonstrateLapTimes() {
    printSection("3. Lap Times");

    StopWatcher sw("LapTimer");
    sw.start();

    std::cout << "Recording lap times:" << std::endl;
    for (int i = 1; i <= 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20 * i));
        sw.lap();
        std::cout << "  Lap " << i << ": " << sw.elapsedMilliseconds() << " ms"
                  << std::endl;
    }

    sw.stop();

    auto lapTimes = sw.getLapTimes();
    std::cout << "\nLap times recorded: " << lapTimes.size() << std::endl;

    double avgLap = sw.getAverageLapTime();
    std::cout << "Average lap time: " << avgLap << " ms" << std::endl;
}

void demonstrateLapStatistics() {
    printSection("4. Lap Statistics");

    StopWatcher sw("StatsTimer");
    sw.start();

    std::vector<int> delays = {10, 20, 15, 25, 30, 18, 22, 28, 12, 35};
    for (int delay : delays) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        sw.lap();
    }

    sw.stop();

    auto stats = sw.getLapStatistics();
    std::cout << "Lap Statistics:" << std::endl;
    std::cout << "  Count: " << stats.count << std::endl;
    std::cout << "  Min: " << stats.min << " ms" << std::endl;
    std::cout << "  Max: " << stats.max << " ms" << std::endl;
    std::cout << "  Average: " << stats.average << " ms" << std::endl;
    std::cout << "  Std Dev: " << stats.standardDev << " ms" << std::endl;
}

void demonstrateStates() {
    printSection("5. StopWatcher States");

    StopWatcher sw("StateTimer");

    auto printState = [&sw]() {
        auto state = sw.getState();
        std::string stateName;
        switch (state) {
            case StopWatcherState::Idle:
                stateName = "Idle";
                break;
            case StopWatcherState::Running:
                stateName = "Running";
                break;
            case StopWatcherState::Paused:
                stateName = "Paused";
                break;
            case StopWatcherState::Stopped:
                stateName = "Stopped";
                break;
        }
        std::cout << "  Current state: " << stateName << std::endl;
    };

    std::cout << "Initial:" << std::endl;
    printState();

    sw.start();
    std::cout << "\nAfter start():" << std::endl;
    printState();

    sw.pause();
    std::cout << "\nAfter pause():" << std::endl;
    printState();

    sw.resume();
    std::cout << "\nAfter resume():" << std::endl;
    printState();

    sw.stop();
    std::cout << "\nAfter stop():" << std::endl;
    printState();

    sw.reset();
    std::cout << "\nAfter reset():" << std::endl;
    printState();
}

void demonstrateCallbacks() {
    printSection("6. Interval Callbacks");

    StopWatcher sw("CallbackTimer");

    sw.setIntervalCallback(std::chrono::milliseconds(50), []() {
        std::cout << "  [Callback] 50ms interval triggered" << std::endl;
    });

    std::cout << "Running with 50ms interval callback for 200ms..."
              << std::endl;
    sw.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    sw.stop();

    std::cout << "Stopped. Total time: " << sw.elapsedMilliseconds() << " ms"
              << std::endl;
}

void demonstrateBenchmarking() {
    printSection("7. Benchmarking Example");

    std::cout << "Benchmarking vector operations:" << std::endl;

    const int iterations = 1000;
    const int size = 10000;

    StopWatcher sw("Benchmark");

    sw.start();
    for (int i = 0; i < iterations; ++i) {
        std::vector<int> vec(size);
        for (int j = 0; j < size; ++j) {
            vec[j] = j;
        }
    }
    sw.stop();

    double totalMs = sw.elapsedMilliseconds();
    double avgMs = totalMs / iterations;

    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "  Vector size: " << size << std::endl;
    std::cout << "  Total time: " << totalMs << " ms" << std::endl;
    std::cout << "  Average per iteration: " << avgMs << " ms" << std::endl;
}

void demonstrateMultipleTimers() {
    printSection("8. Multiple Timers");

    StopWatcher total("TotalTimer");
    StopWatcher phase1("Phase1Timer");
    StopWatcher phase2("Phase2Timer");

    total.start();

    phase1.start();
    std::cout << "Phase 1 running..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    phase1.stop();

    phase2.start();
    std::cout << "Phase 2 running..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(75));
    phase2.stop();

    total.stop();

    std::cout << "\nTiming results:" << std::endl;
    std::cout << "  Phase 1: " << phase1.elapsedMilliseconds() << " ms"
              << std::endl;
    std::cout << "  Phase 2: " << phase2.elapsedMilliseconds() << " ms"
              << std::endl;
    std::cout << "  Total: " << total.elapsedMilliseconds() << " ms"
              << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  StopWatcher Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateBasicUsage();
        demonstratePauseResume();
        demonstrateLapTimes();
        demonstrateLapStatistics();
        demonstrateStates();
        demonstrateCallbacks();
        demonstrateBenchmarking();
        demonstrateMultipleTimers();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All StopWatcher examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
