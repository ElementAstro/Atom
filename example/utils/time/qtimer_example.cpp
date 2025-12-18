/**
 * @file qtimer_example.cpp
 * @brief Examples for atom::utils ElapsedTimer and QTimer
 */

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include "atom/utils/time/qtimer.hpp"

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void demonstrateElapsedTimer() {
    printSection("1. ElapsedTimer Basic Usage");

    ElapsedTimer timer;

    std::cout << "Starting timer..." << std::endl;
    timer.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    std::cout << "Elapsed time:" << std::endl;
    std::cout << "  Nanoseconds: " << timer.elapsedNs() << " ns" << std::endl;
    std::cout << "  Microseconds: " << timer.elapsedUs() << " us" << std::endl;
    std::cout << "  Milliseconds: " << timer.elapsedMs() << " ms" << std::endl;
    std::cout << "  Seconds: " << timer.elapsedSec() << " s" << std::endl;
}

void demonstrateTimerValidity() {
    printSection("2. Timer Validity");

    ElapsedTimer timer;

    std::cout << "Before start:" << std::endl;
    std::cout << "  isValid(): " << (timer.isValid() ? "true" : "false")
              << std::endl;

    timer.start();
    std::cout << "\nAfter start:" << std::endl;
    std::cout << "  isValid(): " << (timer.isValid() ? "true" : "false")
              << std::endl;

    timer.invalidate();
    std::cout << "\nAfter invalidate:" << std::endl;
    std::cout << "  isValid(): " << (timer.isValid() ? "true" : "false")
              << std::endl;
}

void demonstrateAutoStart() {
    printSection("3. Auto-Start Constructor");

    ElapsedTimer timer(true);  // Start immediately

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::cout << "Timer started in constructor:" << std::endl;
    std::cout << "  Elapsed: " << timer.elapsedMs() << " ms" << std::endl;
    std::cout << "  isValid(): " << (timer.isValid() ? "true" : "false")
              << std::endl;
}

void demonstrateTemplateElapsed() {
    printSection("4. Template-Based Elapsed Time");

    ElapsedTimer timer(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "Using template elapsed<>:" << std::endl;
    std::cout << "  Nanoseconds: " << timer.elapsed<std::chrono::nanoseconds>()
              << std::endl;
    std::cout << "  Microseconds: "
              << timer.elapsed<std::chrono::microseconds>() << std::endl;
    std::cout << "  Milliseconds: "
              << timer.elapsed<std::chrono::milliseconds>() << std::endl;
    std::cout << "  Seconds: " << timer.elapsed<std::chrono::seconds>()
              << std::endl;
}

void demonstrateTimerRestart() {
    printSection("5. Timer Restart");

    ElapsedTimer timer(true);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "First measurement: " << timer.elapsedMs() << " ms"
              << std::endl;

    timer.start();  // Restart
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    std::cout << "After restart: " << timer.elapsedMs() << " ms" << std::endl;
}

void demonstrateBenchmarking() {
    printSection("6. Benchmarking with ElapsedTimer");

    const int iterations = 10000;

    ElapsedTimer timer(true);

    volatile int sum = 0;
    for (int i = 0; i < iterations; ++i) {
        sum += i;
    }

    auto elapsed = timer.elapsedNs();

    std::cout << "Benchmark results:" << std::endl;
    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "  Total time: " << elapsed << " ns" << std::endl;
    std::cout << "  Per iteration: " << (elapsed / iterations) << " ns"
              << std::endl;
}

void demonstrateQTimer() {
    printSection("7. QTimer (Periodic Timer)");

    std::cout << "QTimer usage pattern:" << std::endl;
    std::cout << R"(
    QTimer timer;

    // Set interval
    timer.setInterval(std::chrono::milliseconds(100));

    // Set callback
    timer.setCallback([]() {
        std::cout << "Timer fired!" << std::endl;
    });

    // Start timer
    timer.start();

    // ... timer fires every 100ms ...

    // Stop timer
    timer.stop();
    )" << std::endl;
}

void demonstrateSingleShot() {
    printSection("8. Single-Shot Timer");

    std::cout << "Single-shot timer usage:" << std::endl;
    std::cout << R"(
    QTimer timer;

    // Set as single-shot (fires once)
    timer.setSingleShot(true);
    timer.setInterval(std::chrono::milliseconds(500));

    timer.setCallback([]() {
        std::cout << "Single-shot timer fired!" << std::endl;
    });

    timer.start();

    // Timer fires once after 500ms, then stops automatically
    )" << std::endl;
}

void demonstrateTimerComparison() {
    printSection("9. Comparing Multiple Operations");

    struct Operation {
        std::string name;
        int iterations;
    };

    std::vector<Operation> operations = {{"Vector push_back", 10000},
                                         {"String concatenation", 1000},
                                         {"Integer arithmetic", 100000}};

    for (const auto& op : operations) {
        ElapsedTimer timer(true);

        if (op.name == "Vector push_back") {
            std::vector<int> vec;
            for (int i = 0; i < op.iterations; ++i) {
                vec.push_back(i);
            }
        } else if (op.name == "String concatenation") {
            std::string str;
            for (int i = 0; i < op.iterations; ++i) {
                str += "x";
            }
        } else {
            volatile int result = 0;
            for (int i = 0; i < op.iterations; ++i) {
                result += i * 2;
            }
        }

        std::cout << op.name << " (" << op.iterations
                  << " iterations): " << timer.elapsedUs() << " us"
                  << std::endl;
    }
}

void demonstrateTimerPrecision() {
    printSection("10. Timer Precision Test");

    std::cout << "Testing timer precision with small delays:" << std::endl;

    std::vector<int> delays = {1, 5, 10, 50, 100};

    for (int delay : delays) {
        ElapsedTimer timer(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        auto actual = timer.elapsedMs();

        double error = ((actual - delay) / static_cast<double>(delay)) * 100;
        std::cout << "  Target: " << delay << " ms, Actual: " << actual
                  << " ms, Error: " << std::fixed << std::setprecision(1)
                  << error << "%" << std::endl;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  QTimer/ElapsedTimer Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateElapsedTimer();
        demonstrateTimerValidity();
        demonstrateAutoStart();
        demonstrateTemplateElapsed();
        demonstrateTimerRestart();
        demonstrateBenchmarking();
        demonstrateQTimer();
        demonstrateSingleShot();
        demonstrateTimerComparison();
        demonstrateTimerPrecision();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All timer examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
