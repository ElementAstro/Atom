#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include "../logger/logger.h"
#include "timer.h"


// Minimal mock Logger for capturing log_internal calls
class MockLogger : public modern_log::Logger {
public:
    struct LogEntry {
        modern_log::Level level;
        std::string msg;
    };
    std::vector<LogEntry> entries;

    void log_internal(modern_log::Level level, const std::string& msg) {
        entries.push_back({level, msg});
    }
};

using namespace modern_log;

TEST(ScopedTimerTest, LogsOnDestruction) {
    auto logger = std::make_shared<MockLogger>();
    {
        ScopedTimer timer(logger.get(), "test_scope", Level::debug);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(logger->entries.size(), 1u);
    EXPECT_NE(logger->entries[0].msg.find("test_scope took "),
              std::string::npos);
}

TEST(ScopedTimerTest, DisablePreventsLogging) {
    auto logger = std::make_shared<MockLogger>();
    {
        ScopedTimer timer(logger.get(), "no_log", Level::warn);
        timer.disable();
    }
    EXPECT_TRUE(logger->entries.empty());
}

TEST(ScopedTimerTest, FinishLogsOnceAndDisables) {
    auto logger = std::make_shared<MockLogger>();
    ScopedTimer timer(logger.get(), "finish_once", Level::info);
    timer.finish();
    EXPECT_EQ(logger->entries.size(), 1u);
    // Second finish should not log again
    timer.finish();
    EXPECT_EQ(logger->entries.size(), 1u);
}

TEST(ScopedTimerTest, ElapsedReturnsReasonableValue) {
    ScopedTimer timer(nullptr, "elapsed_test");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    auto elapsed = timer.elapsed();
    EXPECT_GE(elapsed.count(), 1000);  // at least 1ms
}

TEST(ScopedTimerTest, NullLoggerDoesNotCrash) {
    ScopedTimer timer(nullptr, "null_logger");
    timer.finish();  // Should not crash or throw
    // Destructor should also not crash
}

TEST(BenchmarkTest, AddMeasurementAndStats) {
    Benchmark bench("simple");
    bench.add_measurement(std::chrono::microseconds(10));
    bench.add_measurement(std::chrono::microseconds(20));
    bench.add_measurement(std::chrono::microseconds(30));
    auto stats = bench.get_stats();
    EXPECT_EQ(stats.min.count(), 10);
    EXPECT_EQ(stats.max.count(), 30);
    EXPECT_EQ(stats.avg.count(), 20);
    EXPECT_EQ(stats.median.count(), 20);
    EXPECT_NEAR(stats.std_dev, std::sqrt(66.6667), 0.1);
}

TEST(BenchmarkTest, StatsEmpty) {
    Benchmark bench("empty");
    auto stats = bench.get_stats();
    EXPECT_EQ(stats.min.count(), 0);
    EXPECT_EQ(stats.max.count(), 0);
    EXPECT_EQ(stats.avg.count(), 0);
    EXPECT_EQ(stats.median.count(), 0);
    EXPECT_EQ(stats.std_dev, 0.0);
}

TEST(BenchmarkTest, RunFunctionCollectsMeasurements) {
    Benchmark bench("run");
    int x = 0;
    bench.run([&]() { x++; }, 5);
    EXPECT_EQ(bench.get_stats().min.count() >= 0, true);
    EXPECT_EQ(bench.get_stats().max.count() >= 0, true);
    EXPECT_EQ(bench.get_stats().avg.count() >= 0, true);
    EXPECT_EQ(bench.get_stats().median.count() >= 0, true);
    EXPECT_EQ(bench.get_stats().std_dev >= 0.0, true);
    EXPECT_EQ(x, 5);
}

TEST(BenchmarkTest, ReportLogsStats) {
    auto logger = std::make_shared<MockLogger>();
    Benchmark bench("report");
    bench.add_measurement(std::chrono::microseconds(10));
    bench.add_measurement(std::chrono::microseconds(20));
    bench.report(logger.get());
    // Should log at least 6 lines (header + 5 stats)
    EXPECT_GE(logger->entries.size(), 6u);
    EXPECT_NE(logger->entries[0].msg.find("Benchmark Report for 'report':"),
              std::string::npos);
}

TEST(BenchmarkTest, ReportDoesNothingIfLoggerNullOrEmpty) {
    Benchmark bench("no_report");
    // No measurements, should not crash or log
    bench.report(nullptr);
    auto logger = std::make_shared<MockLogger>();
    bench.report(logger.get());
    EXPECT_TRUE(logger->entries.empty());
}