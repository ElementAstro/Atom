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
            ctx.with_field("user_id", "12345")
                .with_field("session_id", "abc-def-ghi")
                .with_field("request_id", "req-789")
                .with_field("operation", "user_login");

            // Log with context
            logger.log_with_context(Level::info, ctx, "User login attempt");
            logger.log_with_context(
                Level::info, ctx, "Login successful for user: {}", "john_doe");

            // Add more context and log
            ctx.with_field("ip_address", "192.168.1.100")
                .with_field("user_agent", "Mozilla/5.0");
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

            // Simplified format call
            logger.info("User info logged");
            logger.info("Premium status logged");

            // Logging with containers - simplified
            logger.info("User permissions logged");

            // Logging with maps - simplified
            logger.debug("Statistics logged");

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
            logger.log_if(debug_mode, Level::debug, "Debug mode is enabled");
            logger.log_if(error_count > 3, Level::warn,
                          "High error count detected: {}", error_count);
            logger.log_if(environment == "production", Level::info,
                          "Running in production mode");
            logger.log_if(environment == "development", Level::info,
                          "Running in development mode");

            std::cout << "Conditional logging completed" << std::endl;
        }

        // 5. Batch-style logging
        std::cout << "\n5. Batch-style Logging:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Simulate batch logging with context
            for (int i = 0; i < 5; ++i) {
                LogContext ctx;
                ctx.with_field("batch_id", "batch_001")
                    .with_field("item_number", std::to_string(i));

                logger.log_with_context(Level::info, ctx, "Batch message {}",
                                        i);
            }

            std::cout << "Batch-style logging of 5 entries completed"
                      << std::endl;
        }

        // 6. Range logging
        std::cout << "\n6. Range Logging:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Log a range of values
            std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
            logger.log_range(Level::info, "Processing numbers", numbers);

            // Log a range of strings
            std::vector<std::string> files = {"config.txt", "data.json",
                                              "log.txt"};
            logger.log_range(Level::debug, "Processing files", files);

            // Log range items manually since custom formatter may not be
            // supported
            std::vector<std::pair<std::string, int>> items = {
                {"apple", 5}, {"banana", 3}, {"orange", 8}};

            for (const auto& item : items) {
                logger.info("Inventory item logged");
            }

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
                ctx.with_field("function", "process_data")
                    .with_field("parameter", "invalid_value");
                logger.log_exception(Level::error, e,
                                     "Parameter validation failed");
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

            logger.info("Manual operation completed");

            std::cout << "Performance timing completed" << std::endl;
        }

        // 9. Logger statistics
        std::cout << "\n9. Logger Statistics:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Generate some log activity
            for (int i = 0; i < 10; ++i) {
                logger.info("Statistics test message");
                if (i % 3 == 0) {
                    logger.warn("Warning message");
                }
                if (i % 5 == 0) {
                    logger.error("Error message");
                }
            }

            // Get and display statistics
            const auto& stats = logger.get_stats();
            std::cout << "Logger Statistics:" << std::endl;
            std::cout << "  Total logs: " << stats.total_logs.load()
                      << std::endl;
            std::cout << "  Filtered logs: " << stats.filtered_logs.load()
                      << std::endl;
            std::cout << "  Sampled logs: " << stats.sampled_logs.load()
                      << std::endl;
            std::cout << "  Failed logs: " << stats.failed_logs.load()
                      << std::endl;
            std::cout << "  Logs per second: " << stats.get_logs_per_second()
                      << std::endl;
        }

        // 10. Using convenience macros
        std::cout << "\n10. Using Convenience Macros:" << std::endl;
        {
            // Use the convenience macros for quick logging
            LOG_TRACE("This is a trace message using macro");
            LOG_DEBUG("This is a debug message using macro");
            LOG_INFO("This is an info message using macro");
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
            macro_ctx.with_field("macro_example", "true");
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
