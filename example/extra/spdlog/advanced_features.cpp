#include "atom/extra/spdlog/modern_log.h"

#include <chrono>
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
            auto file_logger =
                manager.create_logger("file_logger", LogType::file);
            file_logger.info("This message goes to file logger");

            // Create a console logger
            auto console_logger =
                manager.create_logger("console_logger", LogType::console);
            console_logger.info("This message goes to console logger");

            // Create a network logger
            auto network_logger =
                manager.create_logger("network_logger", LogType::network);
            network_logger.info("This message goes to network logger");

            // Get logger by name
            auto retrieved_logger = manager.get_logger("file_logger");
            if (retrieved_logger) {
                retrieved_logger->info("Retrieved logger by name");
            }

            std::cout << "Custom logger management completed" << std::endl;
        }

        // 2. Advanced filtering
        std::cout << "\n2. Advanced Filtering:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Create a level filter
            auto level_filter = std::make_unique<LevelFilter>(Level::warn);
            logger.set_filter(std::move(level_filter));

            // These should be filtered out (below warn level)
            logger.trace("This trace message should be filtered");
            logger.debug("This debug message should be filtered");
            logger.info("This info message should be filtered");

            // These should pass through
            logger.warn("This warning message should pass");
            logger.error("This error message should pass");
            logger.critical("This critical message should pass");

            // Create a pattern filter
            auto pattern_filter = std::make_unique<PatternFilter>("user_.*");
            logger.set_filter(std::move(pattern_filter));

            logger.info("user_login: This should pass");
            logger.info("user_logout: This should pass");
            logger.info("system_startup: This should be filtered");

            // Create a context filter
            auto context_filter =
                std::make_unique<ContextFilter>("environment", "production");
            logger.set_filter(std::move(context_filter));

            LogContext prod_ctx;
            prod_ctx.add("environment", "production");
            logger.log_with_context(Level::info, prod_ctx,
                                    "Production message should pass");

            LogContext dev_ctx;
            dev_ctx.add("environment", "development");
            logger.log_with_context(Level::info, dev_ctx,
                                    "Development message should be filtered");

            // Remove filter
            logger.remove_filter();
            logger.info("Filter removed - this message should pass");

            std::cout << "Advanced filtering completed" << std::endl;
        }

        // 3. Log sampling strategies
        std::cout << "\n3. Log Sampling Strategies:" << std::endl;
        {
            auto& logger = LogManager::default_logger();

            // Rate-based sampling (1 in every 3 messages)
            auto rate_sampler = std::make_unique<RateSampler>(3);
            logger.set_sampler(std::move(rate_sampler));

            std::cout << "Rate sampling (1 in 3):" << std::endl;
            for (int i = 0; i < 10; ++i) {
                logger.info("Rate sampled message {}", i);
            }

            // Time-based sampling (1 message per 100ms)
            auto time_sampler = std::make_unique<TimeSampler>(100ms);
            logger.set_sampler(std::move(time_sampler));

            std::cout << "Time sampling (1 per 100ms):" << std::endl;
            for (int i = 0; i < 10; ++i) {
                logger.info("Time sampled message {}", i);
                std::this_thread::sleep_for(50ms);
            }

            // Adaptive sampling (adjusts based on load)
            auto adaptive_sampler =
                std::make_unique<AdaptiveSampler>(100, 1000);
            logger.set_sampler(std::move(adaptive_sampler));

            std::cout << "Adaptive sampling:" << std::endl;
            for (int i = 0; i < 20; ++i) {
                logger.info("Adaptive sampled message {}", i);
            }

            // Remove sampler
            logger.remove_sampler();
            logger.info("Sampler removed - all messages should pass");

            std::cout << "Log sampling strategies completed" << std::endl;
        }

        // 4. Event system integration
        std::cout << "\n4. Event System Integration:" << std::endl;
        {
            auto& event_system = LogEventSystem::instance();

            // Register event handlers
            event_system.register_handler(
                LogEventType::message_logged, [](const LogEvent& event) {
                    std::cout << "[EVENT] Message logged: " << event.message
                              << std::endl;
                });

            event_system.register_handler(
                LogEventType::error_occurred, [](const LogEvent& event) {
                    std::cout << "[EVENT] Error occurred: " << event.message
                              << std::endl;
                });

            event_system.register_handler(
                LogEventType::performance_threshold, [](const LogEvent& event) {
                    std::cout << "[EVENT] Performance threshold exceeded: "
                              << event.message << std::endl;
                });

            auto& logger = LogManager::default_logger();

            // These will trigger events
            logger.info("This will trigger a message_logged event");
            logger.error(
                "This will trigger both message_logged and error_occurred "
                "events");

            // Simulate performance threshold
            {
                auto timer = logger.time_scope("slow_operation");
                std::this_thread::sleep_for(200ms);  // Simulate slow operation
            }

            std::cout << "Event system integration completed" << std::endl;
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
            logger.log_structured(Level::info, "User profile", user_data);

            // Nested structured data
            StructuredData order_data;
            order_data.add("order_id", "ORD-001");
            order_data.add("total_amount", 99.99);
            order_data.add("currency", "USD");

            StructuredData items_data;
            items_data.add("item_1", "Product A");
            items_data.add("item_2", "Product B");
            order_data.add("items", items_data);

            logger.log_structured(Level::info, "Order created", order_data);

            // Convert to different formats
            std::cout << "Structured data as JSON: " << user_data.to_json()
                      << std::endl;
            std::cout << "Structured data as XML: " << user_data.to_xml()
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
                            ctx.add("thread_id", std::to_string(t));
                            ctx.add("message_id", std::to_string(i));

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

        // 7. Log archiving and rotation
        std::cout << "\n7. Log Archiving and Rotation:" << std::endl;
        {
            auto& archiver = LogArchiver::instance();

            // Configure archiving
            ArchiveConfig config;
            config.max_file_size = 1024 * 1024;  // 1MB
            config.max_files = 5;
            config.compression_enabled = true;
            config.archive_directory = "logs/archive";

            archiver.configure(config);

            // Simulate log rotation
            for (int i = 0; i < 100; ++i) {
                std::string log_content =
                    "Log entry " + std::to_string(i) +
                    " with some content to fill up the log file";
                archiver.add_log_entry("application.log", log_content);
            }

            // Force archive
            archiver.archive_logs();

            // Get archive statistics
            auto stats = archiver.get_stats();
            std::cout << "Archive statistics:" << std::endl;
            std::cout << "  Total archived files: "
                      << stats.total_archived_files << std::endl;
            std::cout << "  Total compressed size: "
                      << stats.total_compressed_size << " bytes" << std::endl;
            std::cout << "  Compression ratio: " << stats.compression_ratio
                      << std::endl;

            std::cout << "Log archiving and rotation completed" << std::endl;
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
                logger.log_structured(Level::info, "Benchmark structured",
                                      data);
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
                throw LogError("Simulated logging error");
            } catch (const LogError& e) {
                logger.error("Caught logging error: {}", e.what());
            }

            // Test recovery mechanisms
            logger.info("Testing logger recovery after error");

            // Simulate network logger failure and fallback
            try {
                auto& manager = LogManager::instance();
                auto network_logger =
                    manager.create_logger("failing_network", LogType::network);

                // This might fail in a real scenario
                network_logger.info("This might fail if network is down");

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
