/**
 * @file comprehensive_error_handling.cpp
 * @brief Comprehensive error handling and exception management
 *
 * This example demonstrates:
 * - All types of error conditions and exception handling
 * - Graceful degradation strategies
 * - Error recovery mechanisms
 * - Resource cleanup and RAII patterns
 * - Custom exception hierarchies
 * - Error logging and reporting
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <atomic>
#include <chrono>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "atom/image/core/exceptions.hpp"
#include "atom/image/core/image_blob.hpp"
#include "atom/image/io/image_loader.hpp"
#include "atom/image/io/image_saver.hpp"
#include "atom/image/processing/image_processor.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Custom exception hierarchy for image processing
 */
namespace atom::image::exceptions {

class ImageException : public std::runtime_error {
public:
    explicit ImageException(const std::string& message)
        : std::runtime_error(message), timestamp_(steady_clock::now()) {}

    steady_clock::time_point getTimestamp() const { return timestamp_; }

private:
    steady_clock::time_point timestamp_;
};

class InvalidImageDataException : public ImageException {
public:
    InvalidImageDataException(const std::string& message, size_t dataSize,
                              size_t expectedSize)
        : ImageException("Invalid image data: " + message),
          dataSize_(dataSize),
          expectedSize_(expectedSize) {}

    size_t getDataSize() const { return dataSize_; }
    size_t getExpectedSize() const { return expectedSize_; }

private:
    size_t dataSize_;
    size_t expectedSize_;
};

class ProcessingException : public ImageException {
public:
    ProcessingException(const std::string& operation, const std::string& reason)
        : ImageException("Processing failed in " + operation + ": " + reason),
          operation_(operation) {}

    const std::string& getOperation() const { return operation_; }

private:
    std::string operation_;
};

class ResourceException : public ImageException {
public:
    ResourceException(const std::string& resource, const std::string& reason)
        : ImageException("Resource error with " + resource + ": " + reason),
          resource_(resource) {}

    const std::string& getResource() const { return resource_; }

private:
    std::string resource_;
};

class MemoryException : public ResourceException {
public:
    MemoryException(size_t requestedSize, size_t availableSize)
        : ResourceException(
              "memory", "Requested " + std::to_string(requestedSize) +
                            " bytes, only " + std::to_string(availableSize) +
                            " available"),
          requestedSize_(requestedSize),
          availableSize_(availableSize) {}

    size_t getRequestedSize() const { return requestedSize_; }
    size_t getAvailableSize() const { return availableSize_; }

private:
    size_t requestedSize_;
    size_t availableSize_;
};

}  // namespace atom::image::exceptions

/**
 * @brief Error logger with different severity levels
 */
class ErrorLogger {
public:
    enum class Level {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3,
        CRITICAL = 4
    };

private:
    std::mutex mutex_;
    std::vector<std::pair<Level, std::string>> logs_;
    Level minLevel_ = Level::INFO;

public:
    void setMinLevel(Level level) {
        std::lock_guard<std::mutex> lock(mutex_);
        minLevel_ = level;
    }

    void log(Level level, const std::string& message) {
        if (level < minLevel_)
            return;

        std::lock_guard<std::mutex> lock(mutex_);

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        std::string levelStr;
        switch (level) {
            case Level::DEBUG:
                levelStr = "DEBUG";
                break;
            case Level::INFO:
                levelStr = "INFO";
                break;
            case Level::WARNING:
                levelStr = "WARNING";
                break;
            case Level::ERROR:
                levelStr = "ERROR";
                break;
            case Level::CRITICAL:
                levelStr = "CRITICAL";
                break;
        }

        std::string logEntry =
            "[" + std::to_string(time_t) + "] " + levelStr + ": " + message;
        logs_.emplace_back(level, logEntry);

        // Also print to console for immediate feedback
        std::cout << logEntry << "\n";
    }

    std::vector<std::string> getLogs(Level minLevel = Level::DEBUG) const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));

        std::vector<std::string> result;
        for (const auto& [level, message] : logs_) {
            if (level >= minLevel) {
                result.push_back(message);
            }
        }
        return result;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        logs_.clear();
    }
};

// Global error logger instance
static ErrorLogger g_errorLogger;

/**
 * @brief RAII resource manager for automatic cleanup
 */
template <typename Resource, typename Deleter>
class ResourceManager {
private:
    Resource resource_;
    Deleter deleter_;
    bool valid_;

public:
    ResourceManager(Resource resource, Deleter deleter)
        : resource_(resource), deleter_(deleter), valid_(true) {}

    ~ResourceManager() {
        if (valid_) {
            try {
                deleter_(resource_);
            } catch (...) {
                // Don't throw in destructor
                g_errorLogger.log(ErrorLogger::Level::ERROR,
                                  "Exception during resource cleanup");
            }
        }
    }

    // Move constructor
    ResourceManager(ResourceManager&& other) noexcept
        : resource_(std::move(other.resource_)),
          deleter_(std::move(other.deleter_)),
          valid_(other.valid_) {
        other.valid_ = false;
    }

    // Disable copy
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager& operator=(ResourceManager&&) = delete;

    Resource& get() { return resource_; }
    const Resource& get() const { return resource_; }

    void release() { valid_ = false; }
};

/**
 * @brief Error-prone operations for testing error handling
 */
class ErrorProneOperations {
public:
    /**
     * @brief Simulate memory allocation that might fail
     */
    static std::unique_ptr<uint8_t[]> allocateMemory(size_t size,
                                                     bool shouldFail = false) {
        g_errorLogger.log(
            ErrorLogger::Level::DEBUG,
            "Attempting to allocate " + std::to_string(size) + " bytes");

        if (shouldFail || size > 1024 * 1024 * 100) {  // Fail for > 100MB
            throw exceptions::MemoryException(
                size, 1024 * 1024 * 50);  // 50MB available
        }

        try {
            auto ptr = std::make_unique<uint8_t[]>(size);
            g_errorLogger.log(ErrorLogger::Level::DEBUG,
                              "Memory allocation successful");
            return ptr;
        } catch (const std::bad_alloc& e) {
            throw exceptions::MemoryException(size, 0);
        }
    }

    /**
     * @brief Simulate file operations that might fail
     */
    static std::string readFile(const std::string& filename,
                                bool shouldFail = false) {
        g_errorLogger.log(ErrorLogger::Level::DEBUG,
                          "Reading file: " + filename);

        if (shouldFail) {
            throw exceptions::ResourceException("file",
                                                "Simulated file read failure");
        }

        std::ifstream file(filename);
        if (!file.is_open()) {
            throw exceptions::ResourceException(
                "file", "Cannot open file: " + filename);
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        g_errorLogger.log(ErrorLogger::Level::DEBUG,
                          "File read successful, " +
                              std::to_string(content.size()) + " bytes");
        return content;
    }

    /**
     * @brief Simulate image processing that might fail
     */
    static blob processImage(const blob& input, const std::string& operation,
                             bool shouldFail = false) {
        g_errorLogger.log(ErrorLogger::Level::DEBUG,
                          "Processing image with operation: " + operation);

        if (shouldFail) {
            throw exceptions::ProcessingException(
                operation, "Simulated processing failure");
        }

        if (input.isEmpty()) {
            throw exceptions::InvalidImageDataException("Empty input data", 0,
                                                        1);
        }

        if (input.size() < 10) {
            throw exceptions::InvalidImageDataException("Data too small",
                                                        input.size(), 10);
        }

        // Simulate processing by copying data
        std::vector<uint8_t> processed(input.size());
        std::memcpy(processed.data(), input.data(), input.size());

        // Apply simple transformation
        for (auto& byte : processed) {
            byte = static_cast<uint8_t>((byte + 10) % 256);
        }

        g_errorLogger.log(ErrorLogger::Level::DEBUG,
                          "Image processing successful");
        return blob(processed.data(), processed.size());
    }
};

/**
 * @brief Demonstrate basic exception handling
 */
void demonstrateBasicExceptionHandling() {
    std::cout << "\n=== Basic Exception Handling ===\n";

    g_errorLogger.log(ErrorLogger::Level::INFO,
                      "Starting basic exception handling demo");

    // Test 1: Memory allocation errors
    std::cout << "Testing memory allocation errors:\n";

    std::vector<size_t> testSizes = {
        1024, 1024 * 1024, 1024 * 1024 * 200};  // Last one should fail

    for (size_t size : testSizes) {
        try {
            auto memory = ErrorProneOperations::allocateMemory(size);
            std::cout << "  Successfully allocated " << size << " bytes\n";

        } catch (const exceptions::MemoryException& e) {
            std::cout << "  Memory allocation failed: " << e.what() << "\n";
            std::cout << "    Requested: " << e.getRequestedSize()
                      << " bytes\n";
            std::cout << "    Available: " << e.getAvailableSize()
                      << " bytes\n";

        } catch (const std::exception& e) {
            std::cout << "  Unexpected error: " << e.what() << "\n";
        }
    }

    // Test 2: File operation errors
    std::cout << "\nTesting file operation errors:\n";

    std::vector<std::string> testFiles = {"existing_file.txt",
                                          "nonexistent_file.txt"};

    for (const auto& filename : testFiles) {
        try {
            // Create a test file for the first case
            if (filename == "existing_file.txt") {
                std::ofstream testFile(filename);
                testFile << "Test content for error handling demo";
                testFile.close();
            }

            auto content = ErrorProneOperations::readFile(filename);
            std::cout << "  Successfully read " << filename << " ("
                      << content.size() << " bytes)\n";

        } catch (const exceptions::ResourceException& e) {
            std::cout << "  File operation failed: " << e.what() << "\n";
            std::cout << "    Resource: " << e.getResource() << "\n";

        } catch (const std::exception& e) {
            std::cout << "  Unexpected error: " << e.what() << "\n";
        }
    }

    // Test 3: Image processing errors
    std::cout << "\nTesting image processing errors:\n";

    std::vector<std::pair<std::vector<uint8_t>, std::string>> testCases = {
        {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}, "Valid data"},
        {{1, 2, 3}, "Too small"},
        {{}, "Empty data"}};

    for (const auto& [data, description] : testCases) {
        try {
            blob input(data.data(), data.size());
            auto result =
                ErrorProneOperations::processImage(input, "test_operation");
            std::cout << "  Successfully processed " << description << " -> "
                      << result.size() << " bytes\n";

        } catch (const exceptions::InvalidImageDataException& e) {
            std::cout << "  Invalid image data (" << description
                      << "): " << e.what() << "\n";
            std::cout << "    Data size: " << e.getDataSize() << "\n";
            std::cout << "    Expected size: " << e.getExpectedSize() << "\n";

        } catch (const exceptions::ProcessingException& e) {
            std::cout << "  Processing failed (" << description
                      << "): " << e.what() << "\n";
            std::cout << "    Operation: " << e.getOperation() << "\n";

        } catch (const std::exception& e) {
            std::cout << "  Unexpected error: " << e.what() << "\n";
        }
    }

    // Cleanup
    std::remove("existing_file.txt");

    g_errorLogger.log(ErrorLogger::Level::INFO,
                      "Basic exception handling demo completed");
}

/**
 * @brief Demonstrate RAII and resource management
 */
void demonstrateRAII() {
    std::cout << "\n=== RAII and Resource Management ===\n";

    g_errorLogger.log(ErrorLogger::Level::INFO, "Starting RAII demonstration");

    // Test 1: Automatic memory cleanup
    std::cout << "Testing automatic memory cleanup:\n";

    try {
        {
            // Create resource manager for memory
            auto memory = ErrorProneOperations::allocateMemory(1024);
            ResourceManager memoryManager(memory.get(), [](uint8_t* ptr) {
                g_errorLogger.log(ErrorLogger::Level::DEBUG,
                                  "Memory cleanup executed");
                // Note: unique_ptr will handle actual deletion
            });

            std::cout << "  Memory allocated and managed\n";

            // Simulate some work that might throw
            if (true) {  // Change to false to test normal path
                throw std::runtime_error("Simulated error during processing");
            }

            std::cout << "  Processing completed normally\n";

        }  // ResourceManager destructor called here

        std::cout << "  Scope exited, resources cleaned up\n";

    } catch (const std::exception& e) {
        std::cout << "  Exception caught: " << e.what() << "\n";
        std::cout << "  Resources were automatically cleaned up\n";
    }

    // Test 2: File handle management
    std::cout << "\nTesting file handle management:\n";

    try {
        {
            std::string filename = "raii_test_file.txt";

            // Create file
            std::ofstream createFile(filename);
            createFile << "RAII test content";
            createFile.close();

            // Manage file with RAII
            std::ifstream* file = new std::ifstream(filename);
            ResourceManager fileManager(file, [filename](std::ifstream* f) {
                g_errorLogger.log(ErrorLogger::Level::DEBUG,
                                  "File handle cleanup executed");
                f->close();
                delete f;
                std::remove(filename.c_str());
            });

            if (!fileManager.get()->is_open()) {
                throw exceptions::ResourceException("file",
                                                    "Cannot open " + filename);
            }

            std::cout << "  File opened and managed\n";

            // Simulate error
            throw std::runtime_error("Simulated file processing error");

        }  // File automatically closed and deleted

    } catch (const std::exception& e) {
        std::cout << "  Exception caught: " << e.what() << "\n";
        std::cout << "  File was automatically closed and cleaned up\n";
    }

    g_errorLogger.log(ErrorLogger::Level::INFO, "RAII demonstration completed");
}

/**
 * @brief Demonstrate graceful degradation
 */
void demonstrateGracefulDegradation() {
    std::cout << "\n=== Graceful Degradation ===\n";

    g_errorLogger.log(ErrorLogger::Level::INFO,
                      "Starting graceful degradation demo");

    // Simulate a complex operation with multiple fallback strategies
    auto processWithFallback = [](const blob& input, int strategy) -> blob {
        std::vector<std::string> strategies = {
            "high_quality_processing", "medium_quality_processing",
            "low_quality_processing", "minimal_processing"};

        for (size_t i = static_cast<size_t>(strategy); i < strategies.size();
             ++i) {
            try {
                g_errorLogger.log(ErrorLogger::Level::INFO,
                                  "Attempting " + strategies[i]);

                // Simulate different failure rates
                bool shouldFail =
                    (i == 0 &&
                     strategy == 0) ||  // First strategy fails initially
                    (i == 1 && strategy <= 1);  // Second strategy also fails

                return ErrorProneOperations::processImage(input, strategies[i],
                                                          shouldFail);

            } catch (const exceptions::ProcessingException& e) {
                g_errorLogger.log(
                    ErrorLogger::Level::WARNING,
                    "Strategy " + strategies[i] + " failed: " + e.what());

                if (i == strategies.size() - 1) {
                    // Last strategy failed, re-throw
                    throw;
                }
                // Continue to next strategy
            }
        }

        // Should never reach here
        throw std::runtime_error("All strategies exhausted");
    };

    // Test graceful degradation
    std::vector<uint8_t> testData = {1, 2,  3,  4,  5,  6,  7, 8,
                                     9, 10, 11, 12, 13, 14, 15};
    blob input(testData.data(), testData.size());

    try {
        auto result = processWithFallback(input, 0);
        std::cout << "Processing succeeded with fallback strategy\n";
        std::cout << "Result size: " << result.size() << " bytes\n";

    } catch (const std::exception& e) {
        std::cout << "All fallback strategies failed: " << e.what() << "\n";
    }

    // Test partial success scenario
    std::cout << "\nTesting partial success with error recovery:\n";

    std::vector<blob> inputBatch;
    for (int i = 0; i < 5; ++i) {
        std::vector<uint8_t> data(10 + i, static_cast<uint8_t>(i));
        inputBatch.emplace_back(data.data(), data.size());
    }

    std::vector<blob> results;
    std::vector<std::string> errors;

    for (size_t i = 0; i < inputBatch.size(); ++i) {
        try {
            // Simulate some items failing
            bool shouldFail = (i == 2);  // Third item fails
            auto result = ErrorProneOperations::processImage(
                inputBatch[i], "batch_process", shouldFail);
            results.push_back(result);

        } catch (const std::exception& e) {
            errors.push_back("Item " + std::to_string(i) + ": " + e.what());

            // Add empty result to maintain indexing
            results.emplace_back();
        }
    }

    std::cout << "Batch processing completed:\n";
    std::cout << "  Successful items: " << (results.size() - errors.size())
              << "/" << inputBatch.size() << "\n";
    std::cout << "  Errors encountered: " << errors.size() << "\n";

    for (const auto& error : errors) {
        std::cout << "    " << error << "\n";
    }

    g_errorLogger.log(ErrorLogger::Level::INFO,
                      "Graceful degradation demo completed");
}

/**
 * @brief Demonstrate error recovery mechanisms
 */
void demonstrateErrorRecovery() {
    std::cout << "\n=== Error Recovery Mechanisms ===\n";

    g_errorLogger.log(ErrorLogger::Level::INFO, "Starting error recovery demo");

    // Test 1: Retry mechanism with exponential backoff
    std::cout << "Testing retry mechanism with exponential backoff:\n";

    auto retryWithBackoff = [](std::function<void()> operation,
                               int maxRetries = 3) -> bool {
        for (int attempt = 0; attempt < maxRetries; ++attempt) {
            try {
                operation();
                return true;  // Success

            } catch (const std::exception& e) {
                g_errorLogger.log(ErrorLogger::Level::WARNING,
                                  "Attempt " + std::to_string(attempt + 1) +
                                      " failed: " + e.what());

                if (attempt == maxRetries - 1) {
                    g_errorLogger.log(ErrorLogger::Level::ERROR,
                                      "All retry attempts exhausted");
                    throw;  // Re-throw last exception
                }

                // Exponential backoff
                int delayMs = 100 * (1 << attempt);  // 100ms, 200ms, 400ms, ...
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
        }
        return false;
    };

    // Simulate operation that fails first few times then succeeds
    std::atomic<int> attemptCount{0};

    auto flakyOperation = [&attemptCount]() {
        int current = attemptCount.fetch_add(1);
        if (current < 2) {  // Fail first 2 attempts
            throw std::runtime_error("Simulated transient failure #" +
                                     std::to_string(current + 1));
        }
        g_errorLogger.log(
            ErrorLogger::Level::INFO,
            "Operation succeeded on attempt " + std::to_string(current + 1));
    };

    try {
        bool success = retryWithBackoff(flakyOperation);
        std::cout << "  Retry mechanism " << (success ? "succeeded" : "failed")
                  << "\n";

    } catch (const std::exception& e) {
        std::cout << "  Retry mechanism failed: " << e.what() << "\n";
    }

    // Test 2: Circuit breaker pattern
    std::cout << "\nTesting circuit breaker pattern:\n";

    // Move CircuitBreaker outside function scope
    class CircuitBreaker {
    private:
        enum class State { CLOSED, OPEN, HALF_OPEN };

        State state_ = State::CLOSED;
        int failureCount_ = 0;
        int failureThreshold_ = 3;
        steady_clock::time_point lastFailureTime_;
        std::chrono::milliseconds timeout_{5000};  // 5 seconds

    public:
        template <typename F>
        auto execute(F&& func) -> decltype(func()) {
            if (state_ == State::OPEN) {
                if (steady_clock::now() - lastFailureTime_ > timeout_) {
                    state_ = State::HALF_OPEN;
                    g_errorLogger.log(ErrorLogger::Level::INFO,
                                      "Circuit breaker: OPEN -> HALF_OPEN");
                } else {
                    throw std::runtime_error("Circuit breaker is OPEN");
                }
            }

            try {
                auto result = func();

                if (state_ == State::HALF_OPEN) {
                    state_ = State::CLOSED;
                    failureCount_ = 0;
                    g_errorLogger.log(ErrorLogger::Level::INFO,
                                      "Circuit breaker: HALF_OPEN -> CLOSED");
                }

                return result;

            } catch (...) {
                failureCount_++;
                lastFailureTime_ = steady_clock::now();

                if (failureCount_ >= failureThreshold_) {
                    state_ = State::OPEN;
                    g_errorLogger.log(ErrorLogger::Level::WARNING,
                                      "Circuit breaker: CLOSED -> OPEN");
                }

                throw;
            }
        }

        State getState() const { return state_; }
    };

    CircuitBreaker breaker;

    // Simulate multiple failures to trip the breaker
    for (int i = 0; i < 5; ++i) {
        try {
            breaker.execute([]() {
                throw std::runtime_error("Simulated service failure");
                return 42;
            });

        } catch (const std::exception& e) {
            std::cout << "  Attempt " << (i + 1) << ": " << e.what() << "\n";
        }
    }

    g_errorLogger.log(ErrorLogger::Level::INFO,
                      "Error recovery demo completed");
}

int main() {
    std::cout << "=== Atom Image Comprehensive Error Handling Demo ===\n";
    std::cout << "This example demonstrates comprehensive error handling and "
                 "exception management\n";

    // Set logging level
    g_errorLogger.setMinLevel(ErrorLogger::Level::DEBUG);

    // Run all demonstrations
    demonstrateBasicExceptionHandling();
    demonstrateRAII();
    demonstrateGracefulDegradation();
    demonstrateErrorRecovery();

    std::cout << "\n=== Error handling demo completed ===\n";
    std::cout << "\nKey capabilities demonstrated:\n";
    std::cout
        << "- Custom exception hierarchies with detailed error information\n";
    std::cout << "- RAII patterns for automatic resource cleanup\n";
    std::cout << "- Graceful degradation with fallback strategies\n";
    std::cout << "- Error recovery mechanisms (retry, circuit breaker)\n";
    std::cout << "- Comprehensive error logging and reporting\n";
    std::cout << "- Exception safety and resource management\n";

    // Show error log summary
    auto errorLogs = g_errorLogger.getLogs(ErrorLogger::Level::WARNING);
    if (!errorLogs.empty()) {
        std::cout << "\nError log summary (" << errorLogs.size()
                  << " warnings/errors):\n";
        for (const auto& log : errorLogs) {
            std::cout << "  " << log << "\n";
        }
    }

    return 0;
}
