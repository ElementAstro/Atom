// filepath: atom/extra/spdlog/core/test_types.h

#include <gtest/gtest.h>
#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include "types.h"


using namespace modern_log;

TEST(LevelEnumTest, LevelValuesAreCorrect) {
    EXPECT_EQ(static_cast<int>(Level::trace), 0);
    EXPECT_EQ(static_cast<int>(Level::debug), 1);
    EXPECT_EQ(static_cast<int>(Level::info), 2);
    EXPECT_EQ(static_cast<int>(Level::warn), 3);
    EXPECT_EQ(static_cast<int>(Level::error), 4);
    EXPECT_EQ(static_cast<int>(Level::critical), 5);
    EXPECT_EQ(static_cast<int>(Level::off), 6);
}

TEST(LogTypeEnumTest, LogTypeValuesAreDistinct) {
    EXPECT_NE(LogType::general, LogType::security);
    EXPECT_NE(LogType::performance, LogType::business);
    EXPECT_NE(LogType::audit, LogType::system);
    EXPECT_NE(LogType::network, LogType::database);
}

TEST(LogEventEnumTest, LogEventValuesAreDistinct) {
    EXPECT_NE(LogEvent::logger_created, LogEvent::logger_destroyed);
    EXPECT_NE(LogEvent::level_changed, LogEvent::sink_added);
    EXPECT_NE(LogEvent::sink_removed, LogEvent::error_occurred);
    EXPECT_NE(LogEvent::rotation_occurred, LogEvent::flush_triggered);
    EXPECT_NE(LogEvent::archive_completed, LogEvent::logger_created);
}

TEST(SamplingStrategyEnumTest, SamplingStrategyValuesAreDistinct) {
    EXPECT_NE(SamplingStrategy::none, SamplingStrategy::uniform);
    EXPECT_NE(SamplingStrategy::adaptive, SamplingStrategy::burst);
}

TEST(LogConfigTest, DefaultValues) {
    LogConfig config;
    EXPECT_EQ(config.name, "default");
    EXPECT_EQ(config.level, Level::info);
    EXPECT_EQ(config.pattern, "[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
    EXPECT_FALSE(config.async);
    EXPECT_EQ(config.async_queue_size, 8192u);
    EXPECT_EQ(config.async_thread_count, 1u);
    EXPECT_FALSE(config.file_config.has_value());
    EXPECT_TRUE(config.console_output);
    EXPECT_TRUE(config.colored_output);
}

TEST(LogConfigTest, FileConfigDefaults) {
    LogConfig::FileConfig file_cfg;
    EXPECT_EQ(file_cfg.filename, "");
    EXPECT_FALSE(file_cfg.rotating);
    EXPECT_EQ(file_cfg.max_size, 1048576u * 5);
    EXPECT_EQ(file_cfg.max_files, 3u);
    EXPECT_FALSE(file_cfg.daily_rotation);
    EXPECT_EQ(file_cfg.rotation_hour, 0);
    EXPECT_EQ(file_cfg.rotation_minute, 0);
}

TEST(LogConfigTest, FileConfigAssignment) {
    LogConfig config;
    LogConfig::FileConfig file_cfg;
    file_cfg.filename = "mylog.txt";
    file_cfg.rotating = true;
    file_cfg.max_size = 1024 * 1024;
    file_cfg.max_files = 10;
    file_cfg.daily_rotation = true;
    file_cfg.rotation_hour = 2;
    file_cfg.rotation_minute = 30;
    config.file_config = file_cfg;

    ASSERT_TRUE(config.file_config.has_value());
    EXPECT_EQ(config.file_config->filename, "mylog.txt");
    EXPECT_TRUE(config.file_config->rotating);
    EXPECT_EQ(config.file_config->max_size, 1024u * 1024);
    EXPECT_EQ(config.file_config->max_files, 10u);
    EXPECT_TRUE(config.file_config->daily_rotation);
    EXPECT_EQ(config.file_config->rotation_hour, 2);
    EXPECT_EQ(config.file_config->rotation_minute, 30);
}

TEST(LogStatsTest, DefaultValues) {
    LogStats stats;
    EXPECT_EQ(stats.total_logs.load(), 0u);
    EXPECT_EQ(stats.filtered_logs.load(), 0u);
    EXPECT_EQ(stats.sampled_logs.load(), 0u);
    EXPECT_EQ(stats.failed_logs.load(), 0u);
}

TEST(LogStatsTest, IncrementingCounters) {
    LogStats stats;
    stats.total_logs++;
    stats.filtered_logs += 2;
    stats.sampled_logs += 3;
    stats.failed_logs += 4;
    EXPECT_EQ(stats.total_logs.load(), 1u);
    EXPECT_EQ(stats.filtered_logs.load(), 2u);
    EXPECT_EQ(stats.sampled_logs.load(), 3u);
    EXPECT_EQ(stats.failed_logs.load(), 4u);
}

TEST(LogStatsTest, LogsPerSecondCalculation) {
    LogStats stats;
    stats.total_logs = 100;
    stats.start_time =
        std::chrono::steady_clock::now() - std::chrono::seconds(2);
    double lps = stats.get_logs_per_second();
    EXPECT_GT(lps, 0.0);
    EXPECT_LE(lps, 100.0);
}

TEST(LogStatsTest, LogsPerSecondZeroIfNoTimeElapsed) {
    LogStats stats;
    stats.total_logs = 50;
    stats.start_time = std::chrono::steady_clock::now();
    EXPECT_EQ(stats.get_logs_per_second(), 0.0);
}

TEST(LogConfigTest, ConsoleAndColorOutputFlags) {
    LogConfig config;
    config.console_output = false;
    config.colored_output = false;
    EXPECT_FALSE(config.console_output);
    EXPECT_FALSE(config.colored_output);
}

TEST(LogConfigTest, AsyncConfig) {
    LogConfig config;
    config.async = true;
    config.async_queue_size = 4096;
    config.async_thread_count = 4;
    EXPECT_TRUE(config.async);
    EXPECT_EQ(config.async_queue_size, 4096u);
    EXPECT_EQ(config.async_thread_count, 4u);
}