#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "atom/log/mmap_logger.hpp"


using namespace atom::log;
namespace fs = std::filesystem;

void printSection(const std::string& title) {
    std::cout << "\n===== " << title << " =====\n";
}

void simulateWork(int ms = 100) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void basicUsageExample() {
    printSection("Basic Usage Example");
    try {
        MmapLogger logger("logs/basic_usage.log");
        logger.trace("This is a trace message");
        logger.debug(std::format("Debug message with value: {}", 42));
        logger.info("Information: system started successfully");
        logger.warn(std::format("Warning: resource usage at {}%", 85));
        logger.error(
            std::format("Error occurred: {}", std::string("disk space low")));
        logger.critical("Critical error: database connection failed");
        logger.flush();
        std::cout
            << "Basic usage example completed. Check logs/basic_usage.log\n";
    } catch (const std::exception& e) {
        std::cerr << "Exception in basic usage example: " << e.what() << "\n";
    }
}

void parameterCombinationsExample() {
    printSection("Parameter Combinations Example");
    try {
        MmapLogger logger1("logs/params1.log", LogLevel::INFO, 2 * 1024 * 1024);
        logger1.info("Logger with custom buffer size (2MB) and INFO level");

        MmapLogger logger2("logs/params2.log", LogLevel::DEBUG, 512 * 1024, 5);
        logger2.debug(
            std::format("Logger with custom buffer size (512KB), DEBUG level, "
                        "and 5 max files"));

        MmapLogger logger3("logs/params3.log", LogLevel::WARN);
        logger3.trace("This trace message will be ignored");
        logger3.debug("This debug message will be ignored");
        logger3.info("This info message will be ignored");
        logger3.warn("This warning message will be logged");

        std::cout << "Parameter combinations example completed. Check "
                     "logs/params*.log\n";
    } catch (const std::exception& e) {
        std::cerr << "Exception in parameter combinations example: " << e.what()
                  << "\n";
    }
}

void threadSafetyExample() {
    printSection("Thread Safety Example");
    try {
        auto logger = std::make_shared<MmapLogger>("logs/threaded.log");
        std::vector<std::thread> threads;
        for (int i = 0; i < 5; ++i) {
            threads.emplace_back([logger, i]() {
                logger->setThreadName(std::format("Worker-{}", i));
                for (int j = 0; j < 10; ++j) {
                    logger->info(std::format("Thread {} - Message {}", i, j));
                    simulateWork(std::rand() % 50);
                }
            });
        }
        for (auto& t : threads)
            t.join();
        logger->flush();
        std::cout
            << "Thread safety example completed. Check logs/threaded.log\n";
    } catch (const std::exception& e) {
        std::cerr << "Exception in thread safety example: " << e.what() << "\n";
    }
}

void systemLoggingExample() {
    printSection("System Logging Example");
    try {
        MmapLogger logger("logs/syslog.log");
        logger.enableSystemLogging(true);
        logger.info("This message goes to both file and system log");
        logger.error("This error is logged to the system event log");
        logger.enableSystemLogging(false);
        logger.info("This message only goes to file, not system log");
        std::cout << "System logging example completed. Check logs/syslog.log "
                     "and system logs\n";
    } catch (const std::exception& e) {
        std::cerr << "Exception in system logging example: " << e.what()
                  << "\n";
    }
}

void logRotationExample() {
    printSection("Log Rotation Example");
    try {
        MmapLogger logger("logs/rotation.log", LogLevel::INFO, 4096, 3);
        for (int i = 0; i < 2000; ++i) {
            logger.info(
                std::format("Log message {}: This is a somewhat long message "
                            "to fill up the buffer quickly",
                            i));
        }
        logger.flush();
        std::cout << "Log rotation example completed.\n";
        std::cout << "Check for multiple files: rotation.log, rotation.1.log, "
                     "rotation.2.log, rotation.3.log\n";
    } catch (const std::exception& e) {
        std::cerr << "Exception in log rotation example: " << e.what() << "\n";
    }
}

void errorHandlingExample() {
    printSection("Error Handling Example");
    try {
        std::cout << "Attempting to create logger with invalid path...\n";
        MmapLogger invalid_logger("/nonexistent/directory/log.txt");
        std::cout << "This should not be printed\n";
    } catch (const std::exception& e) {
        std::cout << "Expected exception caught: " << e.what() << "\n";
    }

    try {
        std::cout << "Attempting to create logger with tiny buffer...\n";
        MmapLogger tiny_logger("logs/tiny.log", LogLevel::INFO, 10);
        std::cout << "Writing a message that exceeds buffer size...\n";
        tiny_logger.info(
            "This message is likely larger than the tiny buffer we allocated");
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << "\n";
    }
}

void performanceBenchmark() {
    printSection("Performance Benchmark");
    try {
        const int NUM_MESSAGES = 100000;
        MmapLogger logger("logs/benchmark.log", LogLevel::INFO,
                          10 * 1024 * 1024);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_MESSAGES; ++i) {
            logger.info(std::format("Benchmark message {}", i));
        }
        logger.flush();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        double msgs_per_sec = NUM_MESSAGES / (duration.count() / 1000.0);
        std::cout << "Logged " << NUM_MESSAGES << " messages in "
                  << duration.count() << "ms (" << msgs_per_sec
                  << " messages/second)\n";
    } catch (const std::exception& e) {
        std::cerr << "Exception in performance benchmark: " << e.what() << "\n";
    }
}

void edgeCasesExample() {
    printSection("Edge Cases Example");
    try {
        MmapLogger logger("logs/edge_cases.log");
        logger.info("");
        std::string long_message(10000, 'X');
        logger.info(std::format("Long message: {}", long_message));
        logger.info("Special chars: \n\t\r\b\\\"'{}%");
        logger.info("Unicode: 你好, 世界! Привет, мир! こんにちは世界!");
        logger.setLevel(LogLevel::ERROR);
        logger.info("This info message should not appear");
        logger.error("This error message should appear");
        logger.setLevel(LogLevel::TRACE);
        logger.trace("Trace is now enabled again");
        logger.flush();
        std::cout
            << "Edge cases example completed. Check logs/edge_cases.log\n";
    } catch (const std::exception& e) {
        std::cerr << "Exception in edge cases example: " << e.what() << "\n";
    }
}

int main() {
    std::cout << "MmapLogger Example Program\n=======================\n";
    try {
        fs::create_directories("logs");
    } catch (const std::exception& e) {
        std::cerr << "Failed to create logs directory: " << e.what() << "\n";
        return 1;
    }

    basicUsageExample();
    parameterCombinationsExample();
    threadSafetyExample();
    systemLoggingExample();
    logRotationExample();
    errorHandlingExample();
    performanceBenchmark();
    edgeCasesExample();

    std::cout << "\nAll examples completed.\nCheck the logs/ directory for "
                 "output files.\n";
    return 0;
}
