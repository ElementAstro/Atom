#include "atom/extra/spdlog/modern_log.h"

#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

using namespace modern_log;
using namespace std::chrono_literals;

int main() {
    try {
        std::cout << "=== Modern Spdlog Basic Usage Example ===" << std::endl;

        // 1. Basic logging with different levels
        std::cout << "\n1. Basic Logging with Different Levels:" << std::endl;
        {
            // Get the default logger
            auto& logger = LogManager::default_logger();

            // Basic logging at different levels
            logger.trace(
                "This is a trace message - very detailed debugging info");
            logger.debug("This is a debug message - debugging information");
            logger.info("This is an info message - general information");
            logger.warn("This is a warning message - something might be wrong");
            logger.error("This is an error message - something went wrong");
            logger.critical(
                "This is a critical message - system is in critical state");

            std::cout << "Basic logging messages sent to default logger"
                      << std::endl;
        }

        // 2. Structured logging with context
        std::cout << "\n2. Structured Logging with Context:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Create a logging context
            LogContext ctx;
            ctx.add("user_id", "12345");
            ctx.add("session_id", "abc-def-ghi");
            ctx.add("request_id", "req-789");
            ctx.add("operation", "user_login");

            // Log with context
            logger.log_with_context(Level::info, ctx, "User login attempt");
            logger.log_with_context(
                Level::info, ctx, "Login successful for user: {}", "john_doe");

            // Add more context and log
            ctx.add("ip_address", "192.168.1.100");
            ctx.add("user_agent", "Mozilla/5.0");
            logger.log_with_context(Level::debug, ctx,
                                    "Login details captured");

            std::cout << "Structured logging with context completed"
                      << std::endl;
        }

        // 3. Formatted logging with modern C++ formatting
        std::cout << "\n3. Formatted Logging:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Various formatting examples
            std::string username = "alice";
            int user_id = 42;
            double balance = 1234.56;
            bool is_premium = true;

            logger.info("User {} (ID: {}) has balance: ${:.2f}", username,
                        user_id, balance);
            logger.info("Premium status: {}", is_premium ? "Yes" : "No");

            // Logging with containers
            std::vector<std::string> permissions = {"read", "write", "admin"};
            logger.info("User permissions: {}", fmt::join(permissions, ", "));

            // Logging with maps
            std::map<std::string, int> stats = {{"login_count", 15},
                                                {"failed_attempts", 2},
                                                {"session_duration", 3600}};

            for (const auto& [key, value] : stats) {
                logger.debug("Stat {}: {}", key, value);
            }

            std::cout << "Formatted logging examples completed" << std::endl;
        }

        // 4. Conditional logging
        std::cout << "\n4. Conditional Logging:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            bool debug_mode = true;
            int error_count = 5;
            std::string environment = "development";

            // Log only if condition is met
            logger.log_if(Level::debug, debug_mode, "Debug mode is enabled");
            logger.log_if(Level::warn, error_count > 3,
                          "High error count detected: {}", error_count);
            logger.log_if(Level::info, environment == "production",
                          "Running in production mode");
            logger.log_if(Level::info, environment == "development",
                          "Running in development mode");

            std::cout << "Conditional logging completed" << std::endl;
        }

        // 5. Batch logging
        std::cout << "\n5. Batch Logging:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Create a batch of log entries
            std::vector<LogEntry> batch;

            for (int i = 0; i < 5; ++i) {
                LogEntry entry;
                entry.level = Level::info;
                entry.message = "Batch message " + std::to_string(i);
                entry.context.add("batch_id", "batch_001");
                entry.context.add("item_number", std::to_string(i));
                batch.push_back(std::move(entry));
            }

            // Log the entire batch
            logger.log_batch(batch);

            std::cout << "Batch logging of " << batch.size()
                      << " entries completed" << std::endl;
        }

        // 6. Range logging
        std::cout << "\n6. Range Logging:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Log a range of values
            std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
            logger.log_range(Level::info, numbers, "Processing numbers");

            // Log a range of strings
            std::vector<std::string> files = {"config.txt", "data.json",
                                              "log.txt"};
            logger.log_range(Level::debug, files, "Processing files");

            // Log a range with custom formatter
            std::vector<std::pair<std::string, int>> items = {
                {"apple", 5}, {"banana", 3}, {"orange", 8}};

            logger.log_range(
                Level::info, items, "Inventory items", [](const auto& item) {
                    return fmt::format("{}:{}", item.first, item.second);
                });

            std::cout << "Range logging completed" << std::endl;
        }

        // 7. Exception logging
        std::cout << "\n7. Exception Logging:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            try {
                // Simulate an operation that throws
                throw std::runtime_error(
                    "Simulated error for logging demonstration");
            } catch (const std::exception& e) {
                logger.log_exception(Level::error, e, "Operation failed");
            }

            try {
                // Another exception type
                throw std::invalid_argument("Invalid parameter provided");
            } catch (const std::exception& e) {
                LogContext ctx;
                ctx.add("function", "process_data");
                ctx.add("parameter", "invalid_value");
                logger.log_exception(Level::error, e,
                                     "Parameter validation failed", ctx);
            }

            std::cout << "Exception logging completed" << std::endl;
        }

        // 8. Performance timing
        std::cout << "\n8. Performance Timing:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Time a scope automatically
            {
                auto timer = logger.time_scope("database_operation");

                // Simulate some work
                std::this_thread::sleep_for(100ms);
                logger.info("Performing database query...");
                std::this_thread::sleep_for(50ms);
                logger.info("Processing results...");

                // Timer automatically logs when it goes out of scope
            }

            // Manual timing
            auto start_time = std::chrono::high_resolution_clock::now();

            // Simulate work
            std::this_thread::sleep_for(75ms);

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time);

            logger.info("Manual operation took {} ms", duration.count());

            std::cout << "Performance timing completed" << std::endl;
        }

        // 9. Logger statistics
        std::cout << "\n9. Logger Statistics:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Generate some log activity
            for (int i = 0; i < 10; ++i) {
                logger.info("Statistics test message {}", i);
                if (i % 3 == 0) {
                    logger.warn("Warning message {}", i);
                }
                if (i % 5 == 0) {
                    logger.error("Error message {}", i);
                }
            }

            // Get and display statistics
            auto stats = logger.get_stats();
            std::cout << "Logger Statistics:" << std::endl;
            std::cout << "  Total messages: " << stats.total_messages
                      << std::endl;
            std::cout << "  Messages by level:" << std::endl;
            std::cout << "    Trace: " << stats.messages_by_level[Level::trace]
                      << std::endl;
            std::cout << "    Debug: " << stats.messages_by_level[Level::debug]
                      << std::endl;
            std::cout << "    Info: " << stats.messages_by_level[Level::info]
                      << std::endl;
            std::cout << "    Warn: " << stats.messages_by_level[Level::warn]
                      << std::endl;
            std::cout << "    Error: " << stats.messages_by_level[Level::error]
                      << std::endl;
            std::cout << "    Critical: "
                      << stats.messages_by_level[Level::critical] << std::endl;
            std::cout << "  Average message size: "
                      << stats.average_message_size << " bytes" << std::endl;
            std::cout << "  Total bytes logged: " << stats.total_bytes
                      << " bytes" << std::endl;
        }

        // 10. Using convenience macros
        std::cout << "\n10. Using Convenience Macros:" << std::endl;
        {
            // Use the convenience macros for quick logging
            LOG_TRACE("This is a trace message using macro");
            LOG_DEBUG("This is a debug message using macro");
            LOG_INFO("This is an info message using macro: {}", "formatted");
            LOG_WARN("This is a warning message using macro");
            LOG_ERROR("This is an error message using macro");
            LOG_CRITICAL("This is a critical message using macro");

            // Time scope macro
            {
                LOG_TIME_SCOPE("macro_timed_operation");
                std::this_thread::sleep_for(50ms);
                LOG_INFO("Work done inside timed scope");
            }

            // Context macro
            LogContext macro_ctx;
            macro_ctx.add("macro_example", "true");
            LOG_WITH_CONTEXT(macro_ctx).info(
                "Message with context using macro");

            std::cout << "Convenience macros demonstration completed"
                      << std::endl;
        }

        std::cout << "\n=== Modern Spdlog Basic Usage Example Completed ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
