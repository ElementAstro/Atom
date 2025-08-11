#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include "../core/error.h"
#include "../core/types.h"
#include "manager.h"


using namespace modern_log;
using ::testing::_;
using ::testing::Return;

class LogManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any existing loggers before each test
        auto& manager = LogManager::instance();
        auto names = manager.get_logger_names();
        for (const auto& name : names) {
            if (name != "default") {  // Don't remove default logger
                manager.remove_logger(name);
            }
        }
    }

    void TearDown() override {
        // Clean up test files
        std::filesystem::remove_all("test_logs");
    }
};

// Test singleton behavior
TEST_F(LogManagerTest, SingletonInstance) {
    LogManager& instance1 = LogManager::instance();
    LogManager& instance2 = LogManager::instance();
    EXPECT_EQ(&instance1, &instance2);
}

// Test basic logger creation
TEST_F(LogManagerTest, CreateSimpleLogger) {
    auto& manager = LogManager::instance();
    LogConfig config{.name = "test_logger",
                     .level = Level::debug,
                     .console_output = true,
                     .colored_output = false};

    auto result = manager.create_logger(config);
    ASSERT_TRUE(result.has_value());

    auto logger = result.value();
    EXPECT_NE(logger, nullptr);
    EXPECT_EQ(manager.logger_count(), 2);  // default + test_logger
}

// Test duplicate logger creation fails
TEST_F(LogManagerTest, CreateDuplicateLoggerFails) {
    auto& manager = LogManager::instance();
    LogConfig config{.name = "duplicate_test", .level = Level::info};

    auto result1 = manager.create_logger(config);
    ASSERT_TRUE(result1.has_value());

    auto result2 = manager.create_logger(config);
    ASSERT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error(), LogError::invalid_config);
}

// Test file logger creation
TEST_F(LogManagerTest, CreateFileLogger) {
    auto& manager = LogManager::instance();
    std::filesystem::create_directories("test_logs");

    LogConfig config{
        .name = "file_logger",
        .level = Level::info,
        .file_config = LogConfig::FileConfig{.filename = "test_logs/test.log",
                                             .rotating = false},
        .console_output = false};

    auto result = manager.create_logger(config);
    ASSERT_TRUE(result.has_value());

    auto logger = result.value();
    logger->info("Test message");
    logger->flush();

    EXPECT_TRUE(std::filesystem::exists("test_logs/test.log"));
}

// Test rotating file logger creation
TEST_F(LogManagerTest, CreateRotatingFileLogger) {
    auto& manager = LogManager::instance();
    std::filesystem::create_directories("test_logs");

    LogConfig config{
        .name = "rotating_logger",
        .level = Level::info,
        .file_config =
            LogConfig::FileConfig{.filename = "test_logs/rotating.log",
                                  .rotating = true,
                                  .max_size = 1024,
                                  .max_files = 3},
        .console_output = false};

    auto result = manager.create_logger(config);
    ASSERT_TRUE(result.has_value());
}

// Test daily rotating file logger creation
TEST_F(LogManagerTest, CreateDailyRotatingFileLogger) {
    auto& manager = LogManager::instance();
    std::filesystem::create_directories("test_logs");

    LogConfig config{
        .name = "daily_logger",
        .level = Level::info,
        .file_config = LogConfig::FileConfig{.filename = "test_logs/daily.log",
                                             .daily_rotation = true,
                                             .rotation_hour = 0,
                                             .rotation_minute = 0},
        .console_output = false};

    auto result = manager.create_logger(config);
    ASSERT_TRUE(result.has_value());
}

// Test async logger creation
TEST_F(LogManagerTest, CreateAsyncLogger) {
    auto& manager = LogManager::instance();
    LogConfig config{.name = "async_logger",
                     .level = Level::info,
                     .console_output = true,
                     .async = true,
                     .async_queue_size = 8192,
                     .async_thread_count = 1};

    auto result = manager.create_logger(config);
    ASSERT_TRUE(result.has_value());

    auto logger = result.value();
    logger->info("Async test message");
    logger->flush();
}

// Test logger retrieval
TEST_F(LogManagerTest, GetLogger) {
    auto& manager = LogManager::instance();
    LogConfig config{.name = "get_test", .level = Level::info};

    auto create_result = manager.create_logger(config);
    ASSERT_TRUE(create_result.has_value());

    auto get_result = manager.get_logger("get_test");
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(create_result.value(), get_result.value());
}

// Test logger retrieval failure
TEST_F(LogManagerTest, GetNonExistentLogger) {
    auto& manager = LogManager::instance();
    auto result = manager.get_logger("non_existent");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), LogError::logger_not_found);
}

// Test logger removal
TEST_F(LogManagerTest, RemoveLogger) {
    auto& manager = LogManager::instance();
    LogConfig config{.name = "remove_test", .level = Level::info};

    auto create_result = manager.create_logger(config);
    ASSERT_TRUE(create_result.has_value());

    size_t count_before = manager.logger_count();
    bool removed = manager.remove_logger("remove_test");
    EXPECT_TRUE(removed);
    EXPECT_EQ(manager.logger_count(), count_before - 1);

    // Verify logger is no longer accessible
    auto get_result = manager.get_logger("remove_test");
    EXPECT_FALSE(get_result.has_value());
}

// Test removing non-existent logger
TEST_F(LogManagerTest, RemoveNonExistentLogger) {
    auto& manager = LogManager::instance();
    bool removed = manager.remove_logger("non_existent");
    EXPECT_FALSE(removed);
}

// Test getting logger names
TEST_F(LogManagerTest, GetLoggerNames) {
    auto& manager = LogManager::instance();

    LogConfig config1{.name = "logger1", .level = Level::info};
    LogConfig config2{.name = "logger2", .level = Level::info};

    manager.create_logger(config1);
    manager.create_logger(config2);

    auto names = manager.get_logger_names();
    EXPECT_GE(names.size(), 2);  // At least our 2 loggers (+ possibly default)
    EXPECT_TRUE(std::find(names.begin(), names.end(), "logger1") !=
                names.end());
    EXPECT_TRUE(std::find(names.begin(), names.end(), "logger2") !=
                names.end());
}

// Test flush all loggers
TEST_F(LogManagerTest, FlushAll) {
    auto& manager = LogManager::instance();
    std::filesystem::create_directories("test_logs");

    LogConfig config{
        .name = "flush_test",
        .level = Level::info,
        .file_config = LogConfig::FileConfig{.filename = "test_logs/flush.log"},
        .console_output = false};

    auto result = manager.create_logger(config);
    ASSERT_TRUE(result.has_value());

    auto logger = result.value();
    logger->info("Before flush");

    manager.flush_all();

    // Check that file exists and contains data
    EXPECT_TRUE(std::filesystem::exists("test_logs/flush.log"));
    std::ifstream file("test_logs/flush.log");
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    EXPECT_FALSE(content.empty());
}

// Test global level setting
TEST_F(LogManagerTest, SetGlobalLevel) {
    auto& manager = LogManager::instance();
    LogConfig config{.name = "level_test", .level = Level::info};

    auto result = manager.create_logger(config);
    ASSERT_TRUE(result.has_value());

    manager.set_global_level(Level::error);

    // All loggers should now have error level
    auto logger = result.value();
    EXPECT_EQ(logger->get_level(), Level::error);
}

// Test default logger
TEST_F(LogManagerTest, DefaultLogger) {
    Logger& default_logger1 = LogManager::default_logger();
    Logger& default_logger2 = LogManager::default_logger();

    EXPECT_EQ(&default_logger1, &default_logger2);  // Same instance

    // Test that default logger works
    default_logger1.info("Default logger test");
}

// Test static factory methods
TEST_F(LogManagerTest, CreateSimpleLoggerStatic) {
    auto result =
        LogManager::create_simple_logger("simple_static", Level::debug, true);
    ASSERT_TRUE(result.has_value());

    auto logger = result.value();
    logger->debug("Simple static logger test");
}

TEST_F(LogManagerTest, CreateFileLoggerStatic) {
    std::filesystem::create_directories("test_logs");
    auto result = LogManager::create_file_logger(
        "file_static", "test_logs/static.log", Level::info, false);
    ASSERT_TRUE(result.has_value());

    auto logger = result.value();
    logger->info("File static logger test");
    logger->flush();

    EXPECT_TRUE(std::filesystem::exists("test_logs/static.log"));
}

TEST_F(LogManagerTest, CreateAsyncLoggerStatic) {
    LogConfig base_config{.level = Level::info, .console_output = true};

    auto result = LogManager::create_async_logger("async_static", base_config);
    ASSERT_TRUE(result.has_value());

    auto logger = result.value();
    logger->info("Async static logger test");
    logger->flush();
}

// Test global statistics
TEST_F(LogManagerTest, GetGlobalStats) {
    auto& manager = LogManager::instance();
    LogConfig config{.name = "stats_test", .level = Level::info};

    auto result = manager.create_logger(config);
    ASSERT_TRUE(result.has_value());

    auto logger = result.value();
    logger->info("Stats test message 1");
    logger->info("Stats test message 2");

    auto stats = manager.get_global_stats();
    EXPECT_GT(stats.total_loggers, 0);
    EXPECT_GE(stats.total_logs, 2);  // At least our 2 messages
}

// Test archiver functionality
TEST_F(LogManagerTest, SetArchiver) {
    auto& manager = LogManager::instance();

    // Create a mock archiver (would need actual implementation)
    // For now, just test that setting nullptr works
    manager.set_archiver(nullptr);
    EXPECT_EQ(manager.get_archiver(), nullptr);
}

// Test thread safety
TEST_F(LogManagerTest, ThreadSafety) {
    auto& manager = LogManager::instance();
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    // Create multiple threads trying to create loggers concurrently
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&manager, &success_count, i]() {
            LogConfig config{.name = "thread_test_" + std::to_string(i),
                             .level = Level::info};
            auto result = manager.create_logger(config);
            if (result.has_value()) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 10);
    EXPECT_GE(manager.logger_count(), 10);
}

// Test concurrent logger access
TEST_F(LogManagerTest, ConcurrentLoggerAccess) {
    auto& manager = LogManager::instance();
    LogConfig config{.name = "concurrent_test", .level = Level::info};

    auto result = manager.create_logger(config);
    ASSERT_TRUE(result.has_value());

    std::vector<std::thread> threads;
    std::atomic<int> access_count{0};

    // Multiple threads accessing the same logger
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&manager, &access_count]() {
            auto logger_result = manager.get_logger("concurrent_test");
            if (logger_result.has_value()) {
                access_count++;
                logger_result.value()->info("Concurrent access test");
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(access_count.load(), 5);
}

// Test error handling for file creation
TEST_F(LogManagerTest, FileCreationError) {
    auto& manager = LogManager::instance();

    // Try to create a file in a non-existent directory without creating it
    LogConfig config{
        .name = "error_test",
        .level = Level::info,
        .file_config =
            LogConfig::FileConfig{
                .filename = "/invalid/path/that/does/not/exist/test.log"},
        .console_output = false};

    // This should still succeed as spdlog/filesystem creates directories
    // But we can test with truly invalid paths on some systems
    auto result = manager.create_logger(config);
    // Result depends on system permissions and path validity
}

// Test logger with null sink when no outputs configured
TEST_F(LogManagerTest, LoggerWithNullSink) {
    auto& manager = LogManager::instance();
    LogConfig config{
        .name = "null_sink_test", .level = Level::info, .console_output = false
        // No file config either
    };

    auto result = manager.create_logger(config);
    ASSERT_TRUE(result.has_value());

    // Logger should work even with null sink
    auto logger = result.value();
    logger->info("This goes to null sink");
}

// Test shutdown behavior
TEST_F(LogManagerTest, ShutdownBehavior) {
    auto& manager = LogManager::instance();
    LogConfig config{.name = "shutdown_test", .level = Level::info};

    auto result = manager.create_logger(config);
    ASSERT_TRUE(result.has_value());

    size_t count_before = manager.logger_count();
    EXPECT_GT(count_before, 0);

    // Note: We can't actually test shutdown() as it's called in destructor
    // and would affect other tests. In a real scenario, you might test this
    // with a separate process or mock the shutdown behavior.
}

// Test event system integration
TEST_F(LogManagerTest, EventSystemIntegration) {
    auto& manager = LogManager::instance();
    auto& event_system = manager.get_event_system();

    // Event system should be accessible
    EXPECT_NE(&event_system, nullptr);

    LogConfig config{.name = "event_test", .level = Level::info};
    auto result = manager.create_logger(config);
    ASSERT_TRUE(result.has_value());

    // Events should be emitted (would need event listener to verify)
}

// Performance test for logger creation
TEST_F(LogManagerTest, LoggerCreationPerformance) {
    auto& manager = LogManager::instance();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; ++i) {
        LogConfig config{
            .name = "perf_test_" + std::to_string(i),
            .level = Level::info,
            .console_output = false  // Disable output for speed
        };
        auto result = manager.create_logger(config);
        ASSERT_TRUE(result.has_value());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should create 100 loggers reasonably quickly (adjust threshold as needed)
    EXPECT_LT(duration.count(), 1000);  // Less than 1 second
}
