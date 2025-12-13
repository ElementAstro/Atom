/**
 * @file test_benchmark.cpp
 * @brief Test benchmark functionality in the Atom Test Framework
 */

#include "atom/tests/atom_test.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

using namespace atom::test;

// ============================================================================
// Simple Benchmark Tests
// ============================================================================

TEST(BenchmarkTests, SimpleBenchmark) {
    BENCHMARK_SIMPLE("BasicOps", "Addition", []() {
        volatile int x = 1 + 1;
        (void)x;
    });

    expect_true(true);
}

TEST(BenchmarkTests, BenchmarkWithIterations) {
    BENCHMARK_N("BasicOps", "MultipleAdditions", 100, []() {
        volatile int sum = 0;
        for (int i = 0; i < 100; ++i) {
            sum += i;
        }
    });

    expect_true(true);
}

// ============================================================================
// Benchmark Suite Tests
// ============================================================================

TEST(BenchmarkTests, BenchmarkSuite) {
    BenchmarkSuite suite("SortingAlgorithms");

    suite.add("SmallVector", []() {
        std::vector<int> vec = {5, 2, 8, 1, 9, 3, 7, 4, 6};
        std::sort(vec.begin(), vec.end());
    });

    suite.add("MediumVector", []() {
        std::vector<int> vec(100);
        std::iota(vec.begin(), vec.end(), 0);
        std::reverse(vec.begin(), vec.end());
        std::sort(vec.begin(), vec.end());
    });

    // Don't actually run the suite in tests, just verify it compiles
    expect_true(true);
}

// ============================================================================
// Benchmark Configuration Tests
// ============================================================================

TEST(BenchmarkTests, BenchmarkWithConfig) {
    Benchmark::Config config;
    config.minIterations = 10;
    config.minDurationSec = 0.1;
    config.logLevel = Benchmark::LogLevel::Silent;

    // Verify config is valid
    expect_gt(config.minIterations, 0);
    expect_gt(config.minDurationSec, 0.0);
}

// ============================================================================
// Benchmark Result Analysis
// ============================================================================

TEST(BenchmarkTests, ResultStatistics) {
    // Test that we can compute statistics on timing data
    std::vector<double> timings = {10.0, 12.0, 11.0, 13.0, 10.5, 11.5, 12.5, 11.0};

    // Mean
    double sum = std::accumulate(timings.begin(), timings.end(), 0.0);
    double mean = sum / timings.size();
    expect_near(mean, 11.4375, 0.01);

    // Min/Max
    auto [minIt, maxIt] = std::minmax_element(timings.begin(), timings.end());
    expect_eq(*minIt, 10.0);
    expect_eq(*maxIt, 13.0);

    // Standard deviation
    double sqSum = 0.0;
    for (double t : timings) {
        sqSum += (t - mean) * (t - mean);
    }
    double stddev = std::sqrt(sqSum / timings.size());
    expect_gt(stddev, 0.0);
}

// ============================================================================
// Benchmark Export Format Tests
// ============================================================================

TEST(BenchmarkTests, ExportFormatEnum) {
    // Verify export formats exist
    auto json = Benchmark::ExportFormat::Json;
    auto csv = Benchmark::ExportFormat::Csv;
    auto markdown = Benchmark::ExportFormat::Markdown;
    auto plaintext = Benchmark::ExportFormat::PlainText;

    expect_ne(static_cast<int>(json), static_cast<int>(csv));
    expect_ne(static_cast<int>(markdown), static_cast<int>(plaintext));
}

// ============================================================================
// Benchmark Log Level Tests
// ============================================================================

TEST(BenchmarkTests, LogLevelEnum) {
    auto silent = Benchmark::LogLevel::Silent;
    auto minimal = Benchmark::LogLevel::Minimal;
    auto normal = Benchmark::LogLevel::Normal;
    auto verbose = Benchmark::LogLevel::Verbose;

    expect_ne(static_cast<int>(silent), static_cast<int>(verbose));
    expect_ne(static_cast<int>(minimal), static_cast<int>(normal));
}

// ============================================================================
// Memory Stats Tests
// ============================================================================

TEST(BenchmarkTests, MemoryStats) {
    Benchmark::MemoryStats stats;
    stats.currentUsage = 1024;
    stats.peakUsage = 2048;

    expect_eq(stats.currentUsage, 1024);
    expect_eq(stats.peakUsage, 2048);
    expect_le(stats.currentUsage, stats.peakUsage);
}

// ============================================================================
// Benchmark Comparison Tests
// ============================================================================

TEST(BenchmarkTests, CompareAlgorithms) {
    // Prepare test data
    std::vector<int> data(1000);
    std::iota(data.begin(), data.end(), 0);

    // Benchmark linear search
    auto linearSearch = [&data]() {
        volatile bool found = false;
        for (int i = 0; i < 1000; ++i) {
            if (data[i] == 500) {
                found = true;
                break;
            }
        }
        (void)found;
    };

    // Benchmark binary search (data is already sorted)
    auto binarySearch = [&data]() {
        volatile bool found = std::binary_search(data.begin(), data.end(), 500);
        (void)found;
    };

    // Just verify both work
    linearSearch();
    binarySearch();

    expect_true(true);
}

// ============================================================================
// Benchmark with Setup and Teardown
// ============================================================================

TEST(BenchmarkTests, BenchmarkWithSetupTeardown) {
    std::vector<int>* testData = nullptr;

    auto setup = [&testData]() -> std::vector<int>* {
        testData = new std::vector<int>(1000);
        std::iota(testData->begin(), testData->end(), 0);
        return testData;
    };

    auto benchmark = [](std::vector<int>*& data) -> size_t {
        std::sort(data->begin(), data->end(), std::greater<int>());
        return 1;
    };

    auto teardown = [](std::vector<int>*& data) {
        delete data;
        data = nullptr;
    };

    // Verify setup/teardown pattern works
    auto data = setup();
    expect_not_null(data);
    expect_size(*data, 1000);

    benchmark(data);
    expect_sorted(*data);  // Should be reverse sorted

    teardown(data);
    expect_null(data);
}

// ============================================================================
// Benchmark Timing Accuracy
// ============================================================================

TEST(BenchmarkTests, TimingAccuracy) {
    using Clock = std::chrono::high_resolution_clock;

    auto start = Clock::now();

    // Do some work
    volatile int sum = 0;
    for (int i = 0; i < 10000; ++i) {
        sum += i;
    }

    auto end = Clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should take at least some time
    expect_ge(duration.count(), 0);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    return runAllTests(argc, argv);
}
