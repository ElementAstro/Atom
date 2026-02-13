#include "atom/extra/spdlog/modern_log.h"

#include <chrono>
#include <format>
#include <future>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace modern_log;
using namespace std::chrono_literals;

int main() {
    try {
        std::cout << "=== Modern Spdlog Advanced Features Example ==="
                  << std::endl;

        // 1. Custom logger creation and management
        std::cout << "\n1. Custom Logger Creation and Management:" << std::endl;
        {
            // Create custom loggers for different components
            auto& manager = LogManager::instance();

            // Create a file logger
            LogConfig file_config{
                .name = "file_logger",
                .level = Level::info,
                .file_config = LogConfig::FileConfig{.filename = "test.log"},
                .console_output = false};
            auto file_logger_result = manager.create_logger(file_config);
            if (file_logger_result) {
                file_logger_result.value()->info(
                    "This message goes to file logger");
            }

            // Create a console logger
            LogConfig console_config{.name = "console_logger",
                                     .level = Level::info,
                                     .file_config = {},
                                     .console_output = true};
            auto console_logger_result = manager.create_logger(console_config);
            if (console_logger_result) {
                console_logger_result.value()->info(
                    "This message goes to console logger");
            }

            // Create a network logger (using general type)
            LogConfig network_config{.name = "network_logger",
                                     .level = Level::info,
                                     .file_config = {},
                                     .console_output = true};
            auto network_logger_result = manager.create_logger(network_config);
            if (network_logger_result) {
                network_logger_result.value()->info(
                    "This message goes to network logger");
            }

            // Get logger by name
            auto retrieved_logger_result = manager.get_logger("file_logger");
            if (retrieved_logger_result) {
                retrieved_logger_result.value()->info(
                    "Retrieved logger by name");
            }

            std::cout << "Custom logger management completed" << std::endl;
        }

        // 2. Level-based filtering (using built-in level control)
        std::cout << "\n2. Level-based Filtering:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Demonstrate level filtering by changing logger level
            logger.set_level(Level::warn);

            // These should be filtered out (below warn level)
            logger.trace("This trace message should be filtered");
            logger.debug("This debug message should be filtered");
            logger.info("This info message should be filtered");

            // These should pass through
            logger.warn("This warning message should pass");
            logger.error("This error message should pass");
            logger.critical("This critical message should pass");

            // Reset to info level
            logger.set_level(Level::info);
            logger.info("Level reset - this message should pass");

            std::cout << "Level-based filtering completed" << std::endl;
        }

        // 3. Manual sampling demonstration
        std::cout << "\n3. Manual Sampling Demonstration:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Manual rate-based sampling (1 in every 3 messages)
            std::cout << "Manual rate sampling (1 in 3):" << std::endl;
            for (int i = 0; i < 10; ++i) {
                if (i % 3 == 0) {  // Manual sampling logic
                    logger.info("Sampled message logged");
                }
            }

            // Manual time-based sampling demonstration
            std::cout << "Manual time-based sampling demonstration:"
                      << std::endl;
            auto last_log_time = std::chrono::steady_clock::now();
            for (int i = 0; i < 10; ++i) {
                auto now = std::chrono::steady_clock::now();
                if (now - last_log_time >= 100ms) {
                    logger.info("Time sampled message logged");
                    last_log_time = now;
                }
                std::this_thread::sleep_for(50ms);
            }

            std::cout << "Manual sampling demonstration completed" << std::endl;
        }

        // 4. Performance timing and statistics
        std::cout << "\n4. Performance Timing and Statistics:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Demonstrate timing functionality
            {
                auto timer = logger.time_scope("example_operation");
                std::this_thread::sleep_for(100ms);
                logger.info("Operation in progress...");
                std::this_thread::sleep_for(50ms);
            }

            // Demonstrate different log levels for monitoring
            logger.info("System running normally");
            logger.warn("Minor issue detected");
            logger.error("Error occurred, handling gracefully");

            // Get logger statistics
            try {
                const auto& stats = logger.get_stats();
                std::cout << "Logger statistics retrieved successfully"
                          << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Statistics not available: " << e.what()
                          << std::endl;
            }

            std::cout << "Performance timing and statistics completed"
                      << std::endl;
        }

        // 5. Structured data logging
        std::cout << "\n5. Structured Data Logging:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Create structured data
            StructuredData user_data;
            user_data.add("user_id", 12345);
            user_data.add("username", "john_doe");
            user_data.add("email", "john@example.com");
            user_data.add("age", 30);
            user_data.add("is_premium", true);
            user_data.add("last_login", "2024-01-15T10:30:00Z");

            // Log structured data
            logger.log_structured(Level::info, user_data);

            // Nested structured data
            StructuredData order_data;
            order_data.add("order_id", "ORD-001");
            order_data.add("total_amount", 99.99);
            order_data.add("currency", "USD");

            StructuredData items_data;
            items_data.add("item_1", "Product A");
            items_data.add("item_2", "Product B");
            order_data.add("items", items_data);

            logger.log_structured(Level::info, order_data);

            // Convert to different formats
            std::cout << "Structured data as JSON: " << user_data.to_json()
                      << std::endl;
            // Skip XML conversion as to_xml method doesn't exist
            std::cout << "XML conversion would be shown here if implemented"
                      << std::endl;

            std::cout << "Structured data logging completed" << std::endl;
        }

        // 6. Multi-threaded logging
        std::cout << "\n6. Multi-threaded Logging:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            std::vector<std::future<void>> futures;
            const int num_threads = 4;
            const int messages_per_thread = 10;

            // Launch multiple threads that log concurrently
            for (int t = 0; t < num_threads; ++t) {
                futures.push_back(std::async(
                    std::launch::async, [&logger, t, messages_per_thread]() {
                        for (int i = 0; i < messages_per_thread; ++i) {
                            LogContext ctx;
                            ctx.with_field("thread_id", std::to_string(t));
                            ctx.with_field("message_id", std::to_string(i));

                            logger.log_with_context(
                                Level::info, ctx, "Thread {} message {}", t, i);

                            // Simulate some work
                            std::this_thread::sleep_for(10ms);
                        }
                    }));
            }

            // Wait for all threads to complete
            for (auto& future : futures) {
                future.wait();
            }

            std::cout << "Multi-threaded logging completed" << std::endl;
        }

        // 7. Log archiving and rotation (skipped - LogArchiver not implemented)
        std::cout << "\n7. Log Archiving and Rotation:" << std::endl;
        {
            std::cout << "LogArchiver functionality would be demonstrated here "
                         "if implemented"
                      << std::endl;
            std::cout << "This would include:" << std::endl;
            std::cout << "  - Automatic log rotation based on size"
                      << std::endl;
            std::cout << "  - Compression of archived logs" << std::endl;
            std::cout << "  - Retention policies" << std::endl;
            std::cout << "Log archiving and rotation skipped" << std::endl;
        }

        // 8. Performance benchmarking
        std::cout << "\n8. Performance Benchmarking:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            const int num_messages = 10000;
            std::cout << "Benchmarking " << num_messages << " log messages..."
                      << std::endl;

            // Benchmark simple logging
            auto start = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < num_messages; ++i) {
                logger.info("Benchmark message {}", i);
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            std::cout << "Simple logging performance:" << std::endl;
            std::cout << "  Total time: " << duration.count() << " μs"
                      << std::endl;
            std::cout << "  Messages per second: "
                      << (num_messages * 1000000.0 / duration.count())
                      << std::endl;
            std::cout << "  Average time per message: "
                      << (duration.count() / num_messages) << " μs"
                      << std::endl;

            // Benchmark structured logging
            start = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < num_messages / 10;
                 ++i) {  // Fewer messages for structured logging
                StructuredData data;
                data.add("iteration", i);
                data.add("timestamp", std::chrono::system_clock::now()
                                          .time_since_epoch()
                                          .count());
                logger.log_structured(Level::info, data);
            }

            end = std::chrono::high_resolution_clock::now();
            auto structured_duration =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            std::cout << "Structured logging performance:" << std::endl;
            std::cout << "  Total time: " << structured_duration.count()
                      << " μs" << std::endl;
            std::cout << "  Messages per second: "
                      << ((num_messages / 10) * 1000000.0 /
                          structured_duration.count())
                      << std::endl;

            std::cout << "Performance benchmarking completed" << std::endl;
        }

        // 9. Error handling and recovery
        std::cout << "\n9. Error Handling and Recovery:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Test error handling
            try {
                // Simulate a logging error
                throw std::runtime_error("Simulated logging error");
            } catch (const std::exception& e) {
                logger.error("Caught logging error: {}", e.what());
            }

            // Test recovery mechanisms
            logger.info("Testing logger recovery after error");

            // Simulate network logger failure and fallback
            try {
                auto& manager = LogManager::instance();
                LogConfig network_config{.name = "failing_network",
                                         .level = Level::info,
                                         .file_config = {},
                                         .console_output = true};
                auto network_logger_result =
                    manager.create_logger(network_config);
                auto network_logger = network_logger_result.value();

                // This might fail in a real scenario
                network_logger->info("This might fail if network is down");

            } catch (const std::exception& e) {
                logger.warn(
                    "Network logger failed, falling back to local logging: {}",
                    e.what());
            }

            std::cout << "Error handling and recovery completed" << std::endl;
        }

        std::cout
            << "\n=== Modern Spdlog Advanced Features Example Completed ==="
            << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
