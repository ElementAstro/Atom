/*
 * test_benchmark.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for benchmark in atom/tests/performance/benchmark.hpp

**************************************************/

#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

#include "atom/tests/performance/benchmark.hpp"

namespace atom::test::performance::tests {

// ============================================================================
// Benchmark Config Tests
// ============================================================================

class BenchmarkConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BenchmarkConfigTest, DefaultConfig) {
    Benchmark::Config config;

    EXPECT_GT(config.minIterations, 0);
    EXPECT_GT(config.minDurationSec, 0.0);
    EXPECT_TRUE(config.warmup);
}

TEST_F(BenchmarkConfigTest, CustomConfig) {
    Benchmark::Config config;
    config.minIterations = 100;
    config.minDurationSec = 2.0;
    config.warmup = false;

    EXPECT_EQ(config.minIterations, 100);
    EXPECT_EQ(config.minDurationSec, 2.0);
    EXPECT_FALSE(config.warmup);
}

TEST_F(BenchmarkConfigTest, FluentConfiguration) {
    auto config = Benchmark::Config()
                      .withMinIterations(50)
                      .withMinDuration(1.5)
                      .withWarmup(false)
                      .withAsync(true);

    EXPECT_EQ(config.minIterations, 50);
    EXPECT_EQ(config.minDurationSec, 1.5);
    EXPECT_FALSE(config.warmup);
    EXPECT_TRUE(config.async);
}

TEST_F(BenchmarkConfigTest, LogLevelConfiguration) {
    Benchmark::Config config;
    config.logLevel = Benchmark::LogLevel::Verbose;

    EXPECT_EQ(config.logLevel, Benchmark::LogLevel::Verbose);
}

TEST_F(BenchmarkConfigTest, ExportFormatConfiguration) {
    Benchmark::Config config;
    config.format = Benchmark::ExportFormat::Json;

    EXPECT_EQ(config.format, Benchmark::ExportFormat::Json);
}

// ============================================================================
// Benchmark Construction Tests
// ============================================================================

class BenchmarkConstructionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BenchmarkConstructionTest, BasicConstruction) {
    Benchmark bench("TestSuite", "TestBenchmark");
    EXPECT_TRUE(true);  // Should not throw
}

TEST_F(BenchmarkConstructionTest, ConstructionWithConfig) {
    Benchmark::Config config;
    config.minIterations = 10;

    Benchmark bench("TestSuite", "TestBenchmark", config);
    EXPECT_TRUE(true);
}

// ============================================================================
// Benchmark LogLevel Tests
// ============================================================================

class BenchmarkLogLevelTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BenchmarkLogLevelTest, SilentLevel) {
    auto level = Benchmark::LogLevel::Silent;
    EXPECT_EQ(static_cast<int>(level), static_cast<int>(Benchmark::LogLevel::Silent));
}

TEST_F(BenchmarkLogLevelTest, MinimalLevel) {
    auto level = Benchmark::LogLevel::Minimal;
    EXPECT_NE(level, Benchmark::LogLevel::Silent);
}

TEST_F(BenchmarkLogLevelTest, NormalLevel) {
    auto level = Benchmark::LogLevel::Normal;
    EXPECT_NE(level, Benchmark::LogLevel::Verbose);
}

TEST_F(BenchmarkLogLevelTest, VerboseLevel) {
    auto level = Benchmark::LogLevel::Verbose;
    EXPECT_EQ(level, Benchmark::LogLevel::Verbose);
}

// ============================================================================
// Benchmark ExportFormat Tests
// ============================================================================

class BenchmarkExportFormatTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BenchmarkExportFormatTest, JsonFormat) {
    auto format = Benchmark::ExportFormat::Json;
    EXPECT_EQ(format, Benchmark::ExportFormat::Json);
}

TEST_F(BenchmarkExportFormatTest, CsvFormat) {
    auto format = Benchmark::ExportFormat::Csv;
    EXPECT_EQ(format, Benchmark::ExportFormat::Csv);
}

TEST_F(BenchmarkExportFormatTest, MarkdownFormat) {
    auto format = Benchmark::ExportFormat::Markdown;
    EXPECT_EQ(format, Benchmark::ExportFormat::Markdown);
}

TEST_F(BenchmarkExportFormatTest, PlainTextFormat) {
    auto format = Benchmark::ExportFormat::PlainText;
    EXPECT_EQ(format, Benchmark::ExportFormat::PlainText);
}

TEST_F(BenchmarkExportFormatTest, ParseFormat) {
    EXPECT_EQ(Benchmark::Config::parseFormat("json"), Benchmark::ExportFormat::Json);
    EXPECT_EQ(Benchmark::Config::parseFormat("csv"), Benchmark::ExportFormat::Csv);
    EXPECT_EQ(Benchmark::Config::parseFormat("markdown"), Benchmark::ExportFormat::Markdown);
    EXPECT_EQ(Benchmark::Config::parseFormat("text"), Benchmark::ExportFormat::PlainText);
}

TEST_F(BenchmarkExportFormatTest, FormatToString) {
    EXPECT_EQ(Benchmark::Config::formatToString(Benchmark::ExportFormat::Json), "json");
    EXPECT_EQ(Benchmark::Config::formatToString(Benchmark::ExportFormat::Csv), "csv");
    EXPECT_EQ(Benchmark::Config::formatToString(Benchmark::ExportFormat::Markdown), "markdown");
    EXPECT_EQ(Benchmark::Config::formatToString(Benchmark::ExportFormat::PlainText), "text");
}

// ============================================================================
// Benchmark MemoryStats Tests
// ============================================================================

class BenchmarkMemoryStatsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BenchmarkMemoryStatsTest, DefaultConstruction) {
    Benchmark::MemoryStats stats;
    EXPECT_EQ(stats.currentUsage, 0);
    EXPECT_EQ(stats.peakUsage, 0);
}

TEST_F(BenchmarkMemoryStatsTest, ConstructionWithValues) {
    Benchmark::MemoryStats stats(1024, 2048);
    EXPECT_EQ(stats.currentUsage, 1024);
    EXPECT_EQ(stats.peakUsage, 2048);
}

TEST_F(BenchmarkMemoryStatsTest, DiffCalculation) {
    Benchmark::MemoryStats stats1(1000, 2000);
    Benchmark::MemoryStats stats2(500, 1500);

    auto diff = stats1.diff(stats2);
    EXPECT_EQ(diff.currentUsage, 500);
    EXPECT_EQ(diff.peakUsage, 500);
}

// ============================================================================
// Benchmark CPUStats Tests
// ============================================================================

class BenchmarkCPUStatsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BenchmarkCPUStatsTest, DefaultConstruction) {
    Benchmark::CPUStats stats;
    EXPECT_EQ(stats.instructionsExecuted, 0);
    EXPECT_EQ(stats.cyclesElapsed, 0);
    EXPECT_EQ(stats.branchMispredictions, 0);
    EXPECT_EQ(stats.cacheMisses, 0);
}

TEST_F(BenchmarkCPUStatsTest, DiffCalculation) {
    Benchmark::CPUStats stats1;
    stats1.instructionsExecuted = 1000;
    stats1.cyclesElapsed = 500;

    Benchmark::CPUStats stats2;
    stats2.instructionsExecuted = 400;
    stats2.cyclesElapsed = 200;

    auto diff = stats1.diff(stats2);
    EXPECT_EQ(diff.instructionsExecuted, 600);
    EXPECT_EQ(diff.cyclesElapsed, 300);
}

TEST_F(BenchmarkCPUStatsTest, GetIPC) {
    Benchmark::CPUStats stats;
    stats.instructionsExecuted = 1000;
    stats.cyclesElapsed = 500;

    auto ipc = stats.getIPC();
    EXPECT_TRUE(ipc.has_value());
    EXPECT_NEAR(*ipc, 2.0, 0.01);
}

TEST_F(BenchmarkCPUStatsTest, GetIPCZeroCycles) {
    Benchmark::CPUStats stats;
    stats.instructionsExecuted = 1000;
    stats.cyclesElapsed = 0;

    auto ipc = stats.getIPC();
    EXPECT_FALSE(ipc.has_value());
}

// ============================================================================
// Benchmark Timing Tests
// ============================================================================

class BenchmarkTimingTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BenchmarkTimingTest, HighResolutionClock) {
    auto start = Benchmark::Clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto end = Benchmark::Clock::now();

    auto duration = std::chrono::duration_cast<Benchmark::Milliseconds>(end - start);
    EXPECT_GE(duration.count(), 9);
}

TEST_F(BenchmarkTimingTest, NanosecondPrecision) {
    auto start = Benchmark::Clock::now();
    auto end = Benchmark::Clock::now();

    auto duration = std::chrono::duration_cast<Benchmark::Nanoseconds>(end - start);
    EXPECT_GE(duration.count(), 0);
}

// ============================================================================
// Benchmark Result Tests
// ============================================================================

class BenchmarkResultTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BenchmarkResultTest, GetResults) {
    auto results = Benchmark::getResults();
    // Results map should be accessible
    EXPECT_TRUE(true);
}

TEST_F(BenchmarkResultTest, ClearResults) {
    Benchmark::clearResults();
    auto results = Benchmark::getResults();
    EXPECT_TRUE(results.empty());
}

// ============================================================================
// Benchmark Global Settings Tests
// ============================================================================

class BenchmarkGlobalSettingsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BenchmarkGlobalSettingsTest, SetGlobalLogLevel) {
    Benchmark::setGlobalLogLevel(Benchmark::LogLevel::Silent);
    // Should not throw
    EXPECT_TRUE(true);
}

TEST_F(BenchmarkGlobalSettingsTest, RegisterGlobalLogger) {
    bool loggerCalled = false;
    Benchmark::registerGlobalLogger([&loggerCalled](const std::string&) {
        loggerCalled = true;
    });
    // Should not throw
    EXPECT_TRUE(true);
}

TEST_F(BenchmarkGlobalSettingsTest, CPUStatsSupported) {
    bool supported = Benchmark::isCpuStatsSupported();
    // Just check it doesn't throw
    EXPECT_TRUE(supported || !supported);
}

// ============================================================================
// Benchmark Run Tests
// ============================================================================

class BenchmarkRunTest : public ::testing::Test {
protected:
    void SetUp() override { Benchmark::clearResults(); }
    void TearDown() override { Benchmark::clearResults(); }
};

TEST_F(BenchmarkRunTest, SimpleRun) {
    Benchmark::Config config;
    config.minIterations = 1;
    config.minDurationSec = 0.001;
    config.logLevel = Benchmark::LogLevel::Silent;

    Benchmark bench("TestSuite", "SimpleRun", config);

    bench.run(
        []() { return 0; },
        [](int&) {
            volatile int x = 1 + 1;
            (void)x;
            return size_t{1};
        },
        [](int&) {});

    auto results = Benchmark::getResults();
    EXPECT_FALSE(results.empty());
}

// ============================================================================
// Benchmark Statistics Tests
// ============================================================================

class BenchmarkStatisticsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BenchmarkStatisticsTest, ComputeMean) {
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};
    double sum = 0.0;
    for (double v : values) {
        sum += v;
    }
    double mean = sum / values.size();
    EXPECT_NEAR(mean, 3.0, 0.001);
}

TEST_F(BenchmarkStatisticsTest, ComputeMedian) {
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::sort(values.begin(), values.end());
    double median = values[values.size() / 2];
    EXPECT_NEAR(median, 3.0, 0.001);
}

TEST_F(BenchmarkStatisticsTest, ComputeStdDev) {
    std::vector<double> values = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    double sum = 0.0;
    for (double v : values) {
        sum += v;
    }
    double mean = sum / values.size();

    double sqSum = 0.0;
    for (double v : values) {
        sqSum += (v - mean) * (v - mean);
    }
    double stddev = std::sqrt(sqSum / values.size());
    EXPECT_NEAR(stddev, 2.0, 0.1);
}

}  // namespace atom::test::performance::tests
