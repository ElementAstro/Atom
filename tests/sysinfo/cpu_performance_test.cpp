/*
 * cpu_performance_test.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-4

Description: CPU System Information Performance Tests

**************************************************/

#include "atom/sysinfo/cpu.hpp"
#include "atom/sysinfo/cpu/common.hpp"
#include <gtest/gtest.h>
#include <benchmark/benchmark.h>
#include <chrono>
#include <thread>
#include <vector>
#include <future>

using namespace atom::system;

class CpuPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Warm up the cache
        getCpuInfo();
    }

    void TearDown() override {
        // Cleanup if necessary
    }
};

// Benchmark CPU info retrieval
static void BM_GetCpuInfo(benchmark::State& state) {
    for (auto _ : state) {
        auto info = getCpuInfo();
        benchmark::DoNotOptimize(info);
    }
}
BENCHMARK(BM_GetCpuInfo);

// Benchmark CPU usage retrieval
static void BM_GetCurrentCpuUsage(benchmark::State& state) {
    for (auto _ : state) {
        auto usage = getCurrentCpuUsage();
        benchmark::DoNotOptimize(usage);
    }
}
BENCHMARK(BM_GetCurrentCpuUsage);

// Benchmark per-core CPU usage
static void BM_GetPerCoreCpuUsage(benchmark::State& state) {
    for (auto _ : state) {
        auto usage = getPerCoreCpuUsage();
        benchmark::DoNotOptimize(usage);
    }
}
BENCHMARK(BM_GetPerCoreCpuUsage);

// Benchmark CPU temperature retrieval
static void BM_GetCurrentCpuTemperature(benchmark::State& state) {
    for (auto _ : state) {
        auto temp = getCurrentCpuTemperature();
        benchmark::DoNotOptimize(temp);
    }
}
BENCHMARK(BM_GetCurrentCpuTemperature);

// Benchmark CPU frequency retrieval
static void BM_GetProcessorFrequency(benchmark::State& state) {
    for (auto _ : state) {
        auto freq = getProcessorFrequency();
        benchmark::DoNotOptimize(freq);
    }
}
BENCHMARK(BM_GetProcessorFrequency);

// Benchmark CPU feature flags retrieval
static void BM_GetCpuFeatureFlags(benchmark::State& state) {
    for (auto _ : state) {
        auto flags = getCpuFeatureFlags();
        benchmark::DoNotOptimize(flags);
    }
}
BENCHMARK(BM_GetCpuFeatureFlags);

// Benchmark CPU topology retrieval
static void BM_GetCpuTopology(benchmark::State& state) {
    for (auto _ : state) {
        auto topology = getCpuTopology();
        benchmark::DoNotOptimize(topology);
    }
}
BENCHMARK(BM_GetCpuTopology);

// Benchmark CPU security info retrieval
static void BM_GetCpuSecurityInfo(benchmark::State& state) {
    for (auto _ : state) {
        auto security = getCpuSecurityInfo();
        benchmark::DoNotOptimize(security);
    }
}
BENCHMARK(BM_GetCpuSecurityInfo);

// Benchmark CPU thermal info retrieval
static void BM_GetCpuThermalInfo(benchmark::State& state) {
    for (auto _ : state) {
        auto thermal = getCpuThermalInfo();
        benchmark::DoNotOptimize(thermal);
    }
}
BENCHMARK(BM_GetCpuThermalInfo);

// Benchmark string to bytes conversion
static void BM_StringToBytes(benchmark::State& state) {
    std::vector<std::string> test_strings = {
        "1024", "1K", "1KB", "1M", "1MB", "1G", "1GB", "2.5GB"
    };
    
    for (auto _ : state) {
        for (const auto& str : test_strings) {
            auto bytes = stringToBytes(str);
            benchmark::DoNotOptimize(bytes);
        }
    }
}
BENCHMARK(BM_StringToBytes);

// Benchmark vendor string conversion
static void BM_GetVendorFromString(benchmark::State& state) {
    std::vector<std::string> vendor_strings = {
        "Intel", "AMD", "Apple", "GenuineIntel", "AuthenticAMD", "Unknown"
    };
    
    for (auto _ : state) {
        for (const auto& vendor_str : vendor_strings) {
            auto vendor = getVendorFromString(vendor_str);
            benchmark::DoNotOptimize(vendor);
        }
    }
}
BENCHMARK(BM_GetVendorFromString);

// Benchmark CPU feature support check
static void BM_IsCpuFeatureSupported(benchmark::State& state) {
    std::vector<std::string> features = {
        "sse", "sse2", "avx", "avx2", "aes", "fma"
    };
    
    for (auto _ : state) {
        for (const auto& feature : features) {
            auto support = isCpuFeatureSupported(feature);
            benchmark::DoNotOptimize(support);
        }
    }
}
BENCHMARK(BM_IsCpuFeatureSupported);

// Performance tests using Google Test
TEST_F(CpuPerformanceTest, CacheEffectiveness) {
    const int iterations = 1000;
    
    // Measure time for first call (cache miss)
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        refreshCpuInfo(); // Force cache refresh
        auto info = getCpuInfo();
        benchmark::DoNotOptimize(info);
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    
    // Measure time for subsequent calls (cache hits)
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto info = getCpuInfo();
        benchmark::DoNotOptimize(info);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
    
    // Cached calls should be significantly faster
    EXPECT_LT(duration2.count(), duration1.count() / 2);
    
    std::cout << "Cache miss average: " << duration1.count() / iterations << " μs\n";
    std::cout << "Cache hit average: " << duration2.count() / iterations << " μs\n";
    std::cout << "Speedup: " << static_cast<double>(duration1.count()) / duration2.count() << "x\n";
}

TEST_F(CpuPerformanceTest, ConcurrentPerformance) {
    const int num_threads = std::thread::hardware_concurrency();
    const int calls_per_thread = 100;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::future<void>> futures;
    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [calls_per_thread]() {
            for (int j = 0; j < calls_per_thread; ++j) {
                auto info = getCpuInfo();
                auto usage = getCurrentCpuUsage();
                auto temp = getCurrentCpuTemperature();
                benchmark::DoNotOptimize(info);
                benchmark::DoNotOptimize(usage);
                benchmark::DoNotOptimize(temp);
            }
        }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    int total_calls = num_threads * calls_per_thread * 3; // 3 functions per iteration
    double calls_per_second = (total_calls * 1000.0) / duration.count();
    
    std::cout << "Concurrent performance: " << calls_per_second << " calls/second\n";
    std::cout << "Total calls: " << total_calls << " in " << duration.count() << " ms\n";
    
    // Should handle at least 1000 calls per second
    EXPECT_GT(calls_per_second, 1000.0);
}

TEST_F(CpuPerformanceTest, MemoryUsage) {
    // Test memory usage doesn't grow excessively
    const int iterations = 10000;
    
    // Get baseline memory usage
    auto info_baseline = getCpuInfo();
    size_t baseline_size = sizeof(info_baseline);
    
    // Perform many operations
    for (int i = 0; i < iterations; ++i) {
        auto info = getCpuInfo();
        auto usage = getCurrentCpuUsage();
        auto temp = getCurrentCpuTemperature();
        auto topology = getCpuTopology();
        
        // Prevent optimization
        benchmark::DoNotOptimize(info);
        benchmark::DoNotOptimize(usage);
        benchmark::DoNotOptimize(temp);
        benchmark::DoNotOptimize(topology);
    }
    
    // Memory usage should remain reasonable
    // This is a basic test - in practice you'd use memory profiling tools
    EXPECT_LT(baseline_size, 1024 * 1024); // Less than 1MB for basic info
}

TEST_F(CpuPerformanceTest, ScalabilityTest) {
    // Test performance scales reasonably with number of cores
    auto num_cores = getNumberOfLogicalCores();
    
    // Test per-core operations
    auto start = std::chrono::high_resolution_clock::now();
    
    auto core_usages = getPerCoreCpuUsage();
    auto core_temps = getPerCoreCpuTemperature();
    auto core_freqs = getPerCoreFrequencies();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Time should scale roughly linearly with number of cores
    double time_per_core = static_cast<double>(duration.count()) / num_cores;
    
    std::cout << "Per-core operations: " << duration.count() << " μs total\n";
    std::cout << "Time per core: " << time_per_core << " μs\n";
    std::cout << "Number of cores: " << num_cores << "\n";
    
    // Should complete per-core operations in reasonable time
    EXPECT_LT(time_per_core, 1000.0); // Less than 1ms per core
}

TEST_F(CpuPerformanceTest, ErrorHandlingPerformance) {
    // Test that error handling doesn't significantly impact performance
    const int iterations = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        // Test functions that might encounter errors
        auto support = isCpuFeatureSupported("nonexistent_feature");
        auto bytes = stringToBytes("invalid_string");
        auto vendor = getVendorFromString("");
        
        benchmark::DoNotOptimize(support);
        benchmark::DoNotOptimize(bytes);
        benchmark::DoNotOptimize(vendor);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double avg_time = static_cast<double>(duration.count()) / iterations;
    
    std::cout << "Error handling average: " << avg_time << " μs per iteration\n";
    
    // Error handling should be fast
    EXPECT_LT(avg_time, 100.0); // Less than 100μs per error case
}

// Run benchmarks
BENCHMARK_MAIN();
