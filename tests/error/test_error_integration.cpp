/*
 * test_error_integration.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Integration tests for the complete error handling system

**************************************************/

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include "atom/error/context/error_context.hpp"
#include "atom/error/core/error_codes.hpp"
#include "atom/error/exception.hpp"
#include "atom/error/handler/error_reporter.hpp"
#include "atom/error/handler/global_handler.hpp"

namespace atom::error::test {

class ErrorIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override { GlobalErrorHandler::getInstance().initialize(); }

    void TearDown() override {
        GlobalErrorHandler::getInstance().shutdown();
        ErrorContextManager::getInstance().clear();
    }
};

// ============================================================================
// End-to-End Error Handling Tests
// ============================================================================

TEST_F(ErrorIntegrationTest, CompleteErrorFlow) {
    std::vector<std::shared_ptr<ErrorContext>> capturedErrors;
    std::mutex capturedErrorsMutex;

    // Set up global error handler
    GlobalErrorHandler::getInstance().setGlobalHandler(
        [&capturedErrors,
         &capturedErrorsMutex](std::shared_ptr<ErrorContext> context) {
            std::lock_guard<std::mutex> lock(capturedErrorsMutex);
            capturedErrors.push_back(context);
        });

    // Simulate error scenarios
    try {
        THROW_EXCEPTION("Test exception with complete flow");
    } catch (const Exception& e) {
        // Exception should be automatically reported
    }

    // Report error directly
    REPORT_ERROR(100, "Direct error report");

    // Report error with correlation
    REPORT_ERROR_WITH_CORRELATION(200, "correlation-123", "Correlated error");

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify errors were captured
    std::lock_guard<std::mutex> lock(capturedErrorsMutex);
    EXPECT_GE(capturedErrors.size(),
              2);  // At least direct report and correlated report

    // Check that correlation ID was set
    bool foundCorrelatedError = false;
    for (const auto& error : capturedErrors) {
        if (error->getCorrelationId() == "correlation-123") {
            foundCorrelatedError = true;
            EXPECT_EQ(error->getErrorCode(), 200);
            EXPECT_EQ(error->getMessage(), "Correlated error");
            break;
        }
    }
    EXPECT_TRUE(foundCorrelatedError);

    // Clear the global handler to avoid capturing dangling references across
    // tests
    GlobalErrorHandler::getInstance().setGlobalHandler(nullptr);
}

// ============================================================================
// Multi-threaded Error Handling Tests
// ============================================================================

TEST_F(ErrorIntegrationTest, ConcurrentErrorReporting) {
    const int numThreads = 10;
    const int errorsPerThread = 100;
    std::atomic<int> totalReported(0);
    std::atomic<int> totalProcessed(0);

    auto& reporter = GlobalErrorHandler::getInstance().getReporter();

    // Add handler to count processed errors
    reporter.addHandler(
        "counter", [&totalProcessed](std::shared_ptr<ErrorContext> context) {
            totalProcessed++;
        });

    std::vector<std::thread> threads;

    // Launch threads that report errors concurrently
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&totalReported, errorsPerThread, t]() {
            for (int i = 0; i < errorsPerThread; ++i) {
                std::string message = "Thread " + std::to_string(t) +
                                      " Error " + std::to_string(i);
                REPORT_ERROR(100 + (t * 10) + i, message);
                totalReported++;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Wait for processing to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_EQ(totalReported.load(), numThreads * errorsPerThread);
    EXPECT_GE(
        totalProcessed.load(),
        numThreads * errorsPerThread * 0.9);  // Allow for some processing delay

    auto stats = reporter.getStatistics();
    EXPECT_GE(stats["total_errors"], numThreads * errorsPerThread);

    // Remove the handler to prevent use-after-scope when the test ends
    reporter.removeHandler("counter");
}

// ============================================================================
// Error Recovery Integration Tests
// ============================================================================

TEST_F(ErrorIntegrationTest, ErrorRecoveryWithRetry) {
    std::atomic<int> attemptCount(0);

    // Function that fails first 2 times, then succeeds
    auto unreliableFunction = [&attemptCount]() -> int {
        int attempt = attemptCount++;
        if (attempt < 2) {
            throw std::runtime_error("Simulated failure " +
                                     std::to_string(attempt));
        }
        return 42;
    };

    // Create recovery executor with retry policy
    ErrorRecoveryExecutor<int> executor;
    executor.withRetryPolicy(RecoveryStrategyFactory::createFixedRetry(
        3, std::chrono::milliseconds(10)));

    // Execute with recovery
    int result = executor.execute(unreliableFunction);

    EXPECT_EQ(result, 42);
    EXPECT_EQ(attemptCount.load(), 3);  // Should have tried 3 times
}

TEST_F(ErrorIntegrationTest, ErrorRecoveryWithFallback) {
    // Function that always fails
    auto failingFunction = []() -> std::string {
        throw std::runtime_error("Always fails");
    };

    // Create recovery executor with fallback
    ErrorRecoveryExecutor<std::string> executor;
    executor.withFallback(
        RecoveryStrategyFactory::createDefaultFallback<std::string>(
            "fallback_value"));

    // Execute with recovery
    std::string result = executor.execute(failingFunction);

    EXPECT_EQ(result, "fallback_value");
}

TEST_F(ErrorIntegrationTest, ErrorRecoveryWithCircuitBreaker) {
    std::atomic<int> callCount(0);

    // Function that always fails
    auto failingFunction = [&callCount]() -> int {
        callCount++;
        throw std::runtime_error("Always fails");
    };

    // Create recovery executor with circuit breaker
    ErrorRecoveryExecutor<int> executor;
    auto circuitBreaker = RecoveryStrategyFactory::createCircuitBreaker(
        2, std::chrono::milliseconds(100));
    executor.withCircuitBreaker(circuitBreaker);

    // First two calls should reach the function
    EXPECT_THROW(executor.execute(failingFunction), std::runtime_error);
    EXPECT_THROW(executor.execute(failingFunction), std::runtime_error);

    // Circuit should be open now, subsequent calls should fail fast
    EXPECT_THROW(executor.execute(failingFunction), std::runtime_error);

    // Call count should be 2 (circuit breaker prevented third call)
    EXPECT_EQ(callCount.load(), 2);
    EXPECT_EQ(circuitBreaker->getState(), CircuitBreakerState::Open);
}

// ============================================================================
// Error Formatting Integration Tests
// ============================================================================

TEST_F(ErrorIntegrationTest, ErrorFormattingIntegration) {
    // Create error with rich context
    auto context = ErrorContext::create(100, "Integration test error");
    context->addTag("integration");
    context->addTag("test");
    context->setCorrelationId("integration-test-123");
    context->setUserData("user_id", std::string("test_user"));
    context->setSystemInfo("component", "integration_test");

    // Test different formatters
    auto plainFormatter =
        ErrorFormatterFactory::createFormatter(OutputFormat::Plain);
    auto jsonFormatter =
        ErrorFormatterFactory::createFormatter(OutputFormat::Json);
    auto htmlFormatter =
        ErrorFormatterFactory::createFormatter(OutputFormat::Html);

    std::string plainOutput = plainFormatter->format(context);
    std::string jsonOutput = jsonFormatter->format(context);
    std::string htmlOutput = htmlFormatter->format(context);

    // All formatters should include the basic information
    EXPECT_TRUE(plainOutput.find("Integration test error") !=
                std::string::npos);
    EXPECT_TRUE(jsonOutput.find("Integration test error") != std::string::npos);
    EXPECT_TRUE(htmlOutput.find("Integration test error") != std::string::npos);

    // Check format-specific elements
    EXPECT_TRUE(plainOutput.find("ERROR REPORT") != std::string::npos);
    EXPECT_TRUE(jsonOutput.find("\"errorCode\": 100") != std::string::npos);
    EXPECT_TRUE(htmlOutput.find("<div class=\"error-report") !=
                std::string::npos);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(ErrorIntegrationTest, ErrorReportingPerformance) {
    const int numErrors = 10000;
    auto& reporter = GlobalErrorHandler::getInstance().getReporter();

    std::atomic<int> processedCount(0);
    reporter.addHandler(
        "perf_counter",
        [&processedCount](std::shared_ptr<ErrorContext> context) {
            processedCount++;
        });

    auto startTime = std::chrono::high_resolution_clock::now();

    // Report many errors quickly
    for (int i = 0; i < numErrors; ++i) {
        REPORT_ERROR(100, "Performance test error " + std::to_string(i));
    }

    auto reportTime = std::chrono::high_resolution_clock::now();

    // Wait for processing to complete
    while (processedCount.load() < numErrors * 0.95) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto endTime = std::chrono::high_resolution_clock::now();

    auto reportDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        reportTime - startTime);
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    // Performance expectations (adjust based on system capabilities)
    EXPECT_LT(reportDuration.count(),
              1000);  // Reporting should be fast (< 1 second)
    EXPECT_LT(totalDuration.count(),
              5000);  // Total processing should be reasonable (< 5 seconds)

    std::cout << "Reported " << numErrors << " errors in "
              << reportDuration.count() << "ms" << std::endl;
    std::cout << "Processed " << processedCount.load() << " errors in "
              << totalDuration.count() << "ms" << std::endl;
}

TEST_F(ErrorIntegrationTest, ErrorFormattingPerformance) {
    const int numFormats = 1000;

    // Create a complex error context
    auto context =
        ErrorContext::create(100,
                             "Performance test error with long message and "
                             "detailed context information");
    context->addTag("performance");
    context->addTag("test");
    context->addTag("formatting");
    context->setCorrelationId("perf-test-correlation-id-123456789");
    context->setUserData("user_id", std::string("performance_test_user"));
    context->setUserData("session_id", std::string("session_123456789"));
    context->setSystemInfo("component", "performance_test_component");
    context->setSystemInfo("version", "1.0.0");
    context->setStackTrace(
        "Stack trace line 1\nStack trace line 2\nStack trace line 3");

    auto formatter = ErrorFormatterFactory::createFormatter(OutputFormat::Json);

    auto startTime = std::chrono::high_resolution_clock::now();

    // Format many times
    for (int i = 0; i < numFormats; ++i) {
        std::string formatted = formatter->format(context);
        // Prevent optimization from removing the formatting
        volatile size_t length = formatted.length();
        (void)length;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime);

    // Performance expectation: should be able to format at least 1000 errors
    // per second
    double formatsPerSecond = (numFormats * 1000000.0) / duration.count();
    EXPECT_GT(formatsPerSecond, 1000.0);

    std::cout << "Formatted " << numFormats << " errors in " << duration.count()
              << " microseconds" << std::endl;
    std::cout << "Performance: " << formatsPerSecond << " formats per second"
              << std::endl;
}

// ============================================================================
// Memory Management Tests
// ============================================================================

TEST_F(ErrorIntegrationTest, ErrorContextMemoryManagement) {
    const int numContexts = 10000;

    // Create many error contexts
    std::vector<std::shared_ptr<ErrorContext>> contexts;
    contexts.reserve(numContexts);

    for (int i = 0; i < numContexts; ++i) {
        auto context = ErrorContext::create(
            100 + i, "Memory test error " + std::to_string(i));
        context->addTag("memory_test");
        context->setCorrelationId("memory-test-" + std::to_string(i));
        contexts.push_back(context);
    }

    // Verify all contexts are valid
    EXPECT_EQ(contexts.size(), numContexts);

    // Check that context manager has all contexts
    auto& manager = ErrorContextManager::getInstance();
    auto stats = manager.getStatistics();
    EXPECT_GE(stats["total_contexts"], numContexts);

    // Clear contexts and verify cleanup
    contexts.clear();

    // Force cleanup of old contexts
    manager.cleanup(std::chrono::minutes(0));  // Clean up everything

    auto cleanedStats = manager.getStatistics();
    EXPECT_LT(cleanedStats["total_contexts"], stats["total_contexts"]);
}

}  // namespace atom::error::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
