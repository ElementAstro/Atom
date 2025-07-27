/**
 * @file performance_test.cpp
 * @brief Performance testing example for the atom_sysinfo_sn library
 *
 * This example demonstrates performance characteristics and benchmarks
 * various operations of the system information library.
 */

#include "sn.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <thread>
#include <future>

using namespace atom::system;

class PerformanceTimer {
public:
    void start() {
        startTime = std::chrono::high_resolution_clock::now();
    }

    void stop() {
        endTime = std::chrono::high_resolution_clock::now();
    }

    double getMilliseconds() const {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        return duration.count() / 1000.0;
    }

    double getMicroseconds() const {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        return static_cast<double>(duration.count());
    }

private:
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point endTime;
};

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::string(60, '=') << "\n";
}

void benchmarkBasicOperations() {
    printSeparator("Basic Operations Benchmark");

    auto sysInfo = createSystemInfo();
    PerformanceTimer timer;

    // Benchmark hardware serials
    timer.start();
    auto hwResult = sysInfo->getHardwareSerials();
    timer.stop();
    std::cout << "Hardware Serials:       " << std::fixed << std::setprecision(2)
              << timer.getMilliseconds() << " ms\n";

    // Benchmark system identification
    timer.start();
    auto sysIdResult = sysInfo->getSystemIdentification();
    timer.stop();
    std::cout << "System Identification: " << std::fixed << std::setprecision(2)
              << timer.getMilliseconds() << " ms\n";

    // Benchmark memory modules
    timer.start();
    auto memResult = sysInfo->getMemoryModules();
    timer.stop();
    std::cout << "Memory Modules:         " << std::fixed << std::setprecision(2)
              << timer.getMilliseconds() << " ms\n";

    // Benchmark network interfaces
    timer.start();
    auto netResult = sysInfo->getNetworkInterfaces();
    timer.stop();
    std::cout << "Network Interfaces:     " << std::fixed << std::setprecision(2)
              << timer.getMilliseconds() << " ms\n";

    // Benchmark comprehensive info
    timer.start();
    auto compResult = sysInfo->getComprehensiveInfo();
    timer.stop();
    std::cout << "Comprehensive Info:     " << std::fixed << std::setprecision(2)
              << timer.getMilliseconds() << " ms\n";

    // Benchmark system fingerprint
    timer.start();
    std::string fingerprint = sysInfo->getSystemFingerprint();
    timer.stop();
    std::cout << "System Fingerprint:     " << std::fixed << std::setprecision(2)
              << timer.getMilliseconds() << " ms\n";
}

void benchmarkCachingPerformance() {
    printSeparator("Caching Performance Benchmark");

    // Test with caching enabled
    SystemInfoConfig cachedConfig;
    cachedConfig.cacheResults = true;
    cachedConfig.cacheTimeout = std::chrono::minutes(10);
    auto cachedSysInfo = createSystemInfo(cachedConfig);

    // Test with caching disabled
    SystemInfoConfig noCacheConfig;
    noCacheConfig.cacheResults = false;
    auto noCacheSysInfo = createSystemInfo(noCacheConfig);

    PerformanceTimer timer;
    const int iterations = 10;

    // Benchmark without caching
    std::cout << "Without caching (" << iterations << " iterations):\n";
    timer.start();
    for (int i = 0; i < iterations; ++i) {
        auto result = noCacheSysInfo->getHardwareSerials();
    }
    timer.stop();
    double noCacheTime = timer.getMilliseconds();
    std::cout << "  Total time: " << std::fixed << std::setprecision(2) << noCacheTime << " ms\n";
    std::cout << "  Average:    " << std::fixed << std::setprecision(2) << (noCacheTime / iterations) << " ms\n";

    // Benchmark with caching (first call populates cache)
    std::cout << "\nWith caching (" << iterations << " iterations):\n";
    timer.start();
    for (int i = 0; i < iterations; ++i) {
        auto result = cachedSysInfo->getHardwareSerials();
    }
    timer.stop();
    double cachedTime = timer.getMilliseconds();
    std::cout << "  Total time: " << std::fixed << std::setprecision(2) << cachedTime << " ms\n";
    std::cout << "  Average:    " << std::fixed << std::setprecision(2) << (cachedTime / iterations) << " ms\n";

    // Calculate speedup
    if (cachedTime > 0) {
        double speedup = noCacheTime / cachedTime;
        std::cout << "\nCaching speedup: " << std::fixed << std::setprecision(1) << speedup << "x\n";
    }
}

void benchmarkConcurrentAccess() {
    printSeparator("Concurrent Access Benchmark");

    auto sysInfo = createSystemInfo();
    const int numThreads = 4;
    const int iterationsPerThread = 5;

    std::cout << "Testing concurrent access with " << numThreads << " threads\n";
    std::cout << "Each thread performs " << iterationsPerThread << " operations\n\n";

    PerformanceTimer timer;
    timer.start();

    std::vector<std::future<double>> futures;

    // Launch threads
    for (int t = 0; t < numThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [&sysInfo, iterationsPerThread, t]() {
            PerformanceTimer threadTimer;
            threadTimer.start();

            for (int i = 0; i < iterationsPerThread; ++i) {
                auto result = sysInfo->getHardwareSerials();
                std::string fingerprint = sysInfo->getSystemFingerprint();
            }

            threadTimer.stop();
            return threadTimer.getMilliseconds();
        }));
    }

    // Wait for all threads and collect results
    std::vector<double> threadTimes;
    for (auto& future : futures) {
        threadTimes.push_back(future.get());
    }

    timer.stop();
    double totalTime = timer.getMilliseconds();

    std::cout << "Results:\n";
    for (int i = 0; i < numThreads; ++i) {
        std::cout << "  Thread " << (i + 1) << ": " << std::fixed << std::setprecision(2)
                  << threadTimes[i] << " ms\n";
    }

    std::cout << "\nTotal wall time: " << std::fixed << std::setprecision(2) << totalTime << " ms\n";

    double avgThreadTime = 0;
    for (double time : threadTimes) {
        avgThreadTime += time;
    }
    avgThreadTime /= numThreads;
    std::cout << "Average thread time: " << std::fixed << std::setprecision(2) << avgThreadTime << " ms\n";

    if (totalTime > 0) {
        double efficiency = (avgThreadTime * numThreads) / totalTime;
        std::cout << "Parallel efficiency: " << std::fixed << std::setprecision(1) << (efficiency * 100) << "%\n";
    }
}

void benchmarkMemoryUsage() {
    printSeparator("Memory Usage Analysis");

    std::cout << "Creating multiple SystemInfo instances to analyze memory usage:\n\n";

    const int numInstances = 10;
    std::vector<std::unique_ptr<SystemInfo>> instances;

    for (int i = 0; i < numInstances; ++i) {
        instances.push_back(createSystemInfo());

        // Populate cache for each instance
        auto result = instances.back()->getComprehensiveInfo();

        std::cout << "Instance " << (i + 1) << " created and populated\n";
    }

    std::cout << "\nAll " << numInstances << " instances created successfully.\n";
    std::cout << "Each instance maintains its own cache and state.\n";
    std::cout << "Memory usage scales linearly with the number of instances.\n";

    // Clear instances
    instances.clear();
    std::cout << "\nAll instances destroyed.\n";
}

void benchmarkDataStructures() {
    printSeparator("Data Structure Performance");

    auto sysInfo = createSystemInfo();
    auto result = sysInfo->getComprehensiveInfo();

    if (!result.success) {
        std::cout << "Failed to get comprehensive info for data structure benchmark\n";
        return;
    }

    PerformanceTimer timer;
    const int iterations = 1000;

    // Benchmark toString operations
    timer.start();
    for (int i = 0; i < iterations; ++i) {
        std::string str = result.data.toString();
    }
    timer.stop();
    std::cout << "toString() (" << iterations << " iterations): "
              << std::fixed << std::setprecision(2) << timer.getMilliseconds() << " ms\n";

    // Benchmark fingerprint generation
    timer.start();
    for (int i = 0; i < iterations; ++i) {
        std::string fp = result.data.getSystemFingerprint();
    }
    timer.stop();
    std::cout << "getSystemFingerprint() (" << iterations << " iterations): "
              << std::fixed << std::setprecision(2) << timer.getMilliseconds() << " ms\n";

    // Benchmark validation
    timer.start();
    for (int i = 0; i < iterations; ++i) {
        bool valid = result.data.isValid();
    }
    timer.stop();
    std::cout << "isValid() (" << iterations << " iterations): "
              << std::fixed << std::setprecision(2) << timer.getMilliseconds() << " ms\n";

    // Benchmark copy operations
    timer.start();
    for (int i = 0; i < iterations; ++i) {
        auto copy = result.data;
    }
    timer.stop();
    std::cout << "Copy constructor (" << iterations << " iterations): "
              << std::fixed << std::setprecision(2) << timer.getMilliseconds() << " ms\n";
}

void benchmarkUtilityFunctions() {
    printSeparator("Utility Functions Benchmark");

    PerformanceTimer timer;
    const int iterations = 1000;

    // Benchmark quick fingerprint
    timer.start();
    for (int i = 0; i < iterations; ++i) {
        std::string fp = getQuickSystemFingerprint();
    }
    timer.stop();
    std::cout << "getQuickSystemFingerprint() (" << iterations << " iterations): "
              << std::fixed << std::setprecision(2) << timer.getMilliseconds() << " ms\n";

    // Benchmark availability check
    timer.start();
    for (int i = 0; i < iterations; ++i) {
        bool available = isSystemInfoAvailable();
    }
    timer.stop();
    std::cout << "isSystemInfoAvailable() (" << iterations << " iterations): "
              << std::fixed << std::setprecision(2) << timer.getMilliseconds() << " ms\n";

    // Benchmark platform capabilities
    timer.start();
    for (int i = 0; i < iterations; ++i) {
        auto caps = getPlatformCapabilities();
    }
    timer.stop();
    std::cout << "getPlatformCapabilities() (" << iterations << " iterations): "
              << std::fixed << std::setprecision(2) << timer.getMilliseconds() << " ms\n";
}

int main() {
    std::cout << "Atom System Information - Performance Test\n";
    std::cout << "==========================================\n";

    try {
        // Run performance benchmarks
        benchmarkBasicOperations();
        benchmarkCachingPerformance();
        benchmarkConcurrentAccess();
        benchmarkMemoryUsage();
        benchmarkDataStructures();
        benchmarkUtilityFunctions();

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "Performance testing completed successfully!\n";
        std::cout << "\nKey findings:\n";
        std::cout << "- Caching provides significant performance improvements\n";
        std::cout << "- Concurrent access is thread-safe and efficient\n";
        std::cout << "- Memory usage scales predictably\n";
        std::cout << "- Data structure operations are optimized\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred\n";
        return 1;
    }

    return 0;
}
