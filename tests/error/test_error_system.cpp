/*
 * test_comprehensive_error_system.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive unit tests for the complete error handling system

**************************************************/

#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <thread>

#include "atom/error/error_code.hpp"
#include "atom/error/error_context.hpp"
#include "atom/error/error_formatter.hpp"
#include "atom/error/error_handler.hpp"
#include "atom/error/error_recovery.hpp"
#include "atom/error/exception.hpp"

namespace atom::error::test {

class ComprehensiveErrorSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize global error handler
        GlobalErrorHandler::getInstance().initialize();
    }

    void TearDown() override {
        // Clean up
        GlobalErrorHandler::getInstance().shutdown();
        ErrorContextManager::getInstance().clear();
    }
};

// ============================================================================
// Error Code and Metadata Tests
// ============================================================================

TEST_F(ComprehensiveErrorSystemTest, ErrorCodeMetadata) {
    // Test error metadata retrieval
    auto metadata = ErrorCodeMapper::getMetadata(100);  // File not found
    EXPECT_EQ(metadata.severity, ErrorSeverity::Error);
    EXPECT_EQ(metadata.category, ErrorCategory::IO);
    EXPECT_EQ(metadata.recovery, ErrorRecoveryStrategy::Retry);
    EXPECT_FALSE(metadata.description.empty());

    // Test severity mapping
    EXPECT_EQ(ErrorCodeMapper::getSeverity(600),
              ErrorSeverity::Critical);  // Memory allocation failed
    EXPECT_EQ(ErrorCodeMapper::getSeverity(601),
              ErrorSeverity::Fatal);  // Out of memory

    // Test recovery strategy
    EXPECT_TRUE(ErrorCodeMapper::isRetryable(100));   // File not found
    EXPECT_FALSE(ErrorCodeMapper::isRetryable(202));  // Device not supported

    // Test unknown error code
    auto unknownMetadata = ErrorCodeMapper::getMetadata(99999);
    EXPECT_EQ(unknownMetadata.category, ErrorCategory::Unknown);
}

TEST_F(ComprehensiveErrorSystemTest, ErrorSeverityConversion) {
    EXPECT_EQ(severityToString(ErrorSeverity::Error), "ERROR");
    EXPECT_EQ(severityToString(ErrorSeverity::Warning), "WARNING");
    EXPECT_EQ(severityToString(ErrorSeverity::Fatal), "FATAL");

    EXPECT_EQ(categoryToString(ErrorCategory::IO), "IO");
    EXPECT_EQ(categoryToString(ErrorCategory::Network), "NETWORK");
    EXPECT_EQ(categoryToString(ErrorCategory::Memory), "MEMORY");

    EXPECT_EQ(recoveryStrategyToString(ErrorRecoveryStrategy::Retry), "RETRY");
    EXPECT_EQ(recoveryStrategyToString(ErrorRecoveryStrategy::Fallback),
              "FALLBACK");
}

// ============================================================================
// Error Context Tests
// ============================================================================

TEST_F(ComprehensiveErrorSystemTest, ErrorContextCreation) {
    auto context = ErrorContext::create(100, "Test file not found");

    EXPECT_FALSE(context->getErrorId().empty());
    EXPECT_EQ(context->getErrorCode(), 100);
    EXPECT_EQ(context->getMessage(), "Test file not found");
    EXPECT_EQ(context->getSeverity(), ErrorSeverity::Error);
    EXPECT_EQ(context->getCategory(), ErrorCategory::IO);
    EXPECT_EQ(context->getRetryCount(), 0);
    EXPECT_GT(context->getMaxRetries(), 0);
}

TEST_F(ComprehensiveErrorSystemTest, ErrorContextUserData) {
    auto context = ErrorContext::create(100, "Test error");

    // Test user data
    context->setUserData("user_id", std::string("12345"));
    context->setUserData("request_id", 67890);

    EXPECT_TRUE(context->hasUserData("user_id"));
    EXPECT_TRUE(context->hasUserData("request_id"));
    EXPECT_FALSE(context->hasUserData("nonexistent"));

    auto userData = context->getUserData("user_id");
    EXPECT_TRUE(userData.has_value());
    EXPECT_EQ(std::any_cast<std::string>(userData), "12345");
}

TEST_F(ComprehensiveErrorSystemTest, ErrorContextSystemInfo) {
    auto context = ErrorContext::create(100, "Test error");

    // System info should be automatically populated
    EXPECT_FALSE(context->getSystemInfo("pid").empty());
    EXPECT_FALSE(context->getSystemInfo("thread_id").empty());

    // Test custom system info
    context->setSystemInfo("custom_field", "custom_value");
    EXPECT_EQ(context->getSystemInfo("custom_field"), "custom_value");
}

TEST_F(ComprehensiveErrorSystemTest, ErrorContextTags) {
    auto context = ErrorContext::create(100, "Test error");

    context->addTag("critical");
    context->addTag("user-facing");
    context->addTag("critical");  // Duplicate should be ignored

    const auto& tags = context->getTags();
    EXPECT_EQ(tags.size(), 2);
    EXPECT_TRUE(context->hasTag("critical"));
    EXPECT_TRUE(context->hasTag("user-facing"));
    EXPECT_FALSE(context->hasTag("nonexistent"));
}

TEST_F(ComprehensiveErrorSystemTest, ErrorContextCorrelation) {
    auto context1 = ErrorContext::create(100, "Parent error");
    auto context2 = ErrorContext::createWithCorrelation(200, "correlation-123",
                                                        "Child error");

    context2->setParentErrorId(context1->getErrorId());
    context1->addChildErrorId(context2->getErrorId());

    EXPECT_EQ(context2->getCorrelationId(), "correlation-123");
    EXPECT_EQ(context2->getParentErrorId(), context1->getErrorId());

    const auto& childIds = context1->getChildErrorIds();
    EXPECT_EQ(childIds.size(), 1);
    EXPECT_EQ(childIds[0], context2->getErrorId());
}

TEST_F(ComprehensiveErrorSystemTest, ErrorContextRetry) {
    auto context = ErrorContext::create(100, "Retryable error");

    EXPECT_TRUE(context->canRetry());
    EXPECT_EQ(context->getRetryCount(), 0);

    context->incrementRetryCount();
    EXPECT_EQ(context->getRetryCount(), 1);
    EXPECT_TRUE(context->canRetry());

    // Exhaust retries
    for (int i = 1; i < context->getMaxRetries(); ++i) {
        context->incrementRetryCount();
    }
    EXPECT_FALSE(context->canRetry());
}

// ============================================================================
// Error Context Manager Tests
// ============================================================================

TEST_F(ComprehensiveErrorSystemTest, ErrorContextManager) {
    auto& manager = ErrorContextManager::getInstance();

    auto context1 = ErrorContext::create(100, "Error 1");
    auto context2 =
        ErrorContext::createWithCorrelation(200, "corr-123", "Error 2");
    auto context3 =
        ErrorContext::createWithCorrelation(300, "corr-123", "Error 3");

    // Test context retrieval
    auto retrieved = manager.getContext(context1->getErrorId());
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->getErrorId(), context1->getErrorId());

    // Test correlation-based retrieval
    auto correlated = manager.getContextsByCorrelation("corr-123");
    EXPECT_EQ(correlated.size(), 2);

    // Test statistics
    auto stats = manager.getStatistics();
    EXPECT_GE(stats["total_contexts"], 3);

    // Test cleanup
    manager.clear();
    auto clearedStats = manager.getStatistics();
    EXPECT_EQ(clearedStats["total_contexts"], 0);
}

// ============================================================================
// Error Reporter Tests
// ============================================================================

TEST_F(ComprehensiveErrorSystemTest, ErrorReporter) {
    ErrorReporter reporter;

    std::vector<std::shared_ptr<ErrorContext>> reportedErrors;

    // Add handler to collect reported errors
    reporter.addHandler(
        "test_handler",
        [&reportedErrors](std::shared_ptr<ErrorContext> context) {
            reportedErrors.push_back(context);
        });

    reporter.start();

    // Report some errors
    auto context1 = ErrorContext::create(100, "Error 1");
    auto context2 = ErrorContext::create(200, "Error 2");

    reporter.reportError(context1);
    reporter.reportError(context2);

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    reporter.stop();

    // Check that errors were processed
    EXPECT_GE(reportedErrors.size(), 2);

    auto stats = reporter.getStatistics();
    EXPECT_GE(stats["total_errors"], 2);
    EXPECT_GE(stats["processed_errors"], 2);
}

TEST_F(ComprehensiveErrorSystemTest, ErrorReporterFiltering) {
    ErrorReporter reporter;

    std::vector<std::shared_ptr<ErrorContext>> reportedErrors;

    // Add filter to only allow critical errors
    reporter.addFilter(
        "severity_filter", [](std::shared_ptr<ErrorContext> context) {
            return context->getSeverity() >= ErrorSeverity::Critical;
        });

    reporter.addHandler(
        "test_handler",
        [&reportedErrors](std::shared_ptr<ErrorContext> context) {
            reportedErrors.push_back(context);
        });

    reporter.start();

    // Report errors of different severities
    auto warningContext =
        ErrorContext::create(700, "Warning error");  // User input error
    auto criticalContext = ErrorContext::create(
        600, "Critical error");  // Memory allocation failed

    reporter.reportError(warningContext);
    reporter.reportError(criticalContext);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    reporter.stop();

    // Only critical error should be processed
    EXPECT_EQ(reportedErrors.size(), 1);
    EXPECT_EQ(reportedErrors[0]->getErrorCode(), 600);

    auto stats = reporter.getStatistics();
    EXPECT_EQ(stats["filtered_errors"], 1);
}

// ============================================================================
// Error Recovery Tests
// ============================================================================

TEST_F(ComprehensiveErrorSystemTest, FixedIntervalRetryPolicy) {
    auto policy = std::make_unique<FixedIntervalRetryPolicy>(
        3, std::chrono::milliseconds(100));
    auto context = ErrorContext::create(100, "Retryable error");

    EXPECT_TRUE(policy->shouldRetry(context));
    EXPECT_EQ(policy->getRetryDelay(0), std::chrono::milliseconds(100));
    EXPECT_EQ(policy->getRetryDelay(1), std::chrono::milliseconds(100));
    EXPECT_EQ(policy->getRetryDelay(2), std::chrono::milliseconds(100));

    // Exhaust retries
    for (int i = 0; i < 3; ++i) {
        context->incrementRetryCount();
    }
    EXPECT_FALSE(policy->shouldRetry(context));
}

TEST_F(ComprehensiveErrorSystemTest, ExponentialBackoffRetryPolicy) {
    auto policy = std::make_unique<ExponentialBackoffRetryPolicy>(
        3, std::chrono::milliseconds(100));
    auto context = ErrorContext::create(100, "Retryable error");

    EXPECT_TRUE(policy->shouldRetry(context));
    EXPECT_EQ(policy->getRetryDelay(0), std::chrono::milliseconds(100));
    EXPECT_EQ(policy->getRetryDelay(1), std::chrono::milliseconds(200));
    EXPECT_EQ(policy->getRetryDelay(2), std::chrono::milliseconds(400));
}

TEST_F(ComprehensiveErrorSystemTest, CircuitBreaker) {
    CircuitBreaker breaker(
        2, std::chrono::milliseconds(100));  // 2 failures, 100ms timeout

    EXPECT_EQ(breaker.getState(), CircuitBreakerState::Closed);

    // Record failures to open circuit
    breaker.recordFailure();
    EXPECT_EQ(breaker.getState(), CircuitBreakerState::Closed);

    breaker.recordFailure();
    EXPECT_EQ(breaker.getState(), CircuitBreakerState::Open);

    // Test that circuit opens
    EXPECT_THROW(breaker.execute([]() { return 42; }), std::runtime_error);

    // Wait for timeout and test half-open state
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Should transition to half-open and allow one call
    EXPECT_NO_THROW(breaker.execute([]() { return 42; }));
    EXPECT_EQ(breaker.getState(), CircuitBreakerState::Closed);
}

}  // namespace atom::error::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
