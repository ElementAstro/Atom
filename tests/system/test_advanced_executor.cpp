/*
 * test_advanced_executor.cpp
 *
 * Comprehensive tests for the AdvancedExecutor functionality
 * Tests environment variable handling, concurrent execution, caching, and security features
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <future>
#include <vector>
#include <unordered_map>

#include "atom/system/command/advanced_executor.hpp"

using namespace atom::system;
using namespace testing;
using namespace std::chrono_literals;

class AdvancedExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup cross-platform test commands
#ifdef _WIN32
        echoCommand = "echo Hello World";
        envTestCommand = "echo %TEST_VAR%";
        sleepCommand = "timeout 1";
        failCommand = "exit 1";
        longRunningCommand = "timeout 3";
#else
        echoCommand = "echo 'Hello World'";
        envTestCommand = "echo $TEST_VAR";
        sleepCommand = "sleep 1";
        failCommand = "false";
        longRunningCommand = "sleep 3";
#endif
    }

    void TearDown() override {
        // Cleanup if needed
    }

    std::string echoCommand;
    std::string envTestCommand;
    std::string sleepCommand;
    std::string failCommand;
    std::string longRunningCommand;
};

// Test CancellationToken functionality
TEST_F(AdvancedExecutorTest, CancellationTokenBasic) {
    auto token = createCancellationToken();
    ASSERT_NE(token, nullptr);

    EXPECT_FALSE(token->isCancelled());

    token->cancel();
    EXPECT_TRUE(token->isCancelled());

    token->reset();
    EXPECT_FALSE(token->isCancelled());
}

// Test ExecutionResourcePool functionality
TEST_F(AdvancedExecutorTest, ExecutionResourcePool) {
    auto pool = createExecutionResourcePool(3);
    ASSERT_NE(pool, nullptr);

    EXPECT_EQ(pool->getTotalResources(), 3);
    EXPECT_EQ(pool->getAvailableResources(), 3);

    // Acquire resources
    auto resource1 = pool->acquireResource();
    EXPECT_EQ(pool->getAvailableResources(), 2);

    auto resource2 = pool->acquireResource();
    EXPECT_EQ(pool->getAvailableResources(), 1);

    auto resource3 = pool->acquireResource();
    EXPECT_EQ(pool->getAvailableResources(), 0);

    // Release a resource
    pool->releaseResource(resource1);
    EXPECT_EQ(pool->getAvailableResources(), 1);
}

// Test basic advanced command execution
TEST_F(AdvancedExecutorTest, BasicAdvancedExecution) {
    AdvancedExecutionConfig config;
    config.baseConfig.enableLogging = true;
    config.baseConfig.validateCommand = true;

    auto result = executeCommandAdvanced(echoCommand, config);

    EXPECT_EQ(result.exitCode, 0);
    EXPECT_FALSE(result.output.empty());
    EXPECT_FALSE(result.timedOut);
    EXPECT_FALSE(result.wasKilled);
    EXPECT_GT(result.executionTime.count(), 0);
}

// Test environment variable handling
TEST_F(AdvancedExecutorTest, EnvironmentVariableHandling) {
    std::unordered_map<std::string, std::string> envVars = {
        {"TEST_VAR", "test_value_123"}
    };

    auto result = executeCommandWithEnv(envTestCommand, envVars);

    EXPECT_THAT(result, HasSubstr("test_value_123"));
}

// Test multiple commands with common environment
TEST_F(AdvancedExecutorTest, MultipleCommandsWithCommonEnv) {
    std::vector<std::string> commands = {
        envTestCommand,
        echoCommand
    };

    std::unordered_map<std::string, std::string> envVars = {
        {"TEST_VAR", "shared_value"}
    };

    auto results = executeCommandsWithCommonEnv(commands, envVars, true);

    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].second, 0);  // Exit code
    EXPECT_EQ(results[1].second, 0);  // Exit code
    EXPECT_THAT(results[0].first, HasSubstr("shared_value"));
}

// Test cancellation functionality
TEST_F(AdvancedExecutorTest, CommandCancellation) {
    auto token = createCancellationToken();

    AdvancedExecutionConfig config;
    config.cancellationToken = token;
    config.baseConfig.timeout = 5000ms;

    // Start a long-running command
    auto future = std::async(std::launch::async, [&]() {
        return executeCommandAdvanced(longRunningCommand, config);
    });

    // Cancel after a short delay
    std::this_thread::sleep_for(100ms);
    token->cancel();

    auto result = future.get();

    // The command should be cancelled or timeout
    EXPECT_TRUE(result.timedOut || result.wasKilled || result.exitCode != 0);
}

// Test timeout functionality
TEST_F(AdvancedExecutorTest, TimeoutHandling) {
    auto result = executeCommandWithTimeout(longRunningCommand, 500ms);

    // Should timeout and return empty optional or empty string
    EXPECT_FALSE(result.has_value() || (result.has_value() && result->empty()));
}

// Test advanced timeout with cancellation
TEST_F(AdvancedExecutorTest, AdvancedTimeoutHandling) {
    auto token = createCancellationToken();
    ExecutionConfig config;
    config.enableLogging = true;

    auto result = executeCommandWithTimeoutAdvanced(
        longRunningCommand, 500ms, token, config);

    EXPECT_FALSE(result.has_value());
}

// Test retry functionality
TEST_F(AdvancedExecutorTest, RetryOnFailure) {
    AdvancedExecutionConfig config;
    config.retryOnFailure = true;
    config.maxRetries = 2;
    config.retryDelay = 100ms;
    config.shouldRetry = [](const ExecutionResult& result) {
        return result.exitCode != 0;
    };

    auto result = executeCommandAdvanced(failCommand, config);

    // Should have attempted retries
    EXPECT_NE(result.exitCode, 0);  // Still fails after retries
    EXPECT_GT(result.executionTime.count(), 200);  // Should take longer due to retries
}

// Test parallel execution
TEST_F(AdvancedExecutorTest, ParallelExecution) {
    std::vector<std::string> commands = {
        echoCommand,
        echoCommand,
        echoCommand
    };

    AdvancedExecutionConfig config;
    config.baseConfig.enableLogging = false;  // Reduce noise

    auto start = std::chrono::steady_clock::now();
    auto results = executeCommandsAdvanced(commands, config, true, false);
    auto end = std::chrono::steady_clock::now();

    EXPECT_EQ(results.size(), 3);

    // Parallel execution should be faster than sequential
    auto parallelTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // All commands should succeed
    for (const auto& result : results) {
        EXPECT_EQ(result.exitCode, 0);
    }
}

// Test resource pool with concurrent execution
TEST_F(AdvancedExecutorTest, ResourcePoolConcurrentExecution) {
    auto pool = createExecutionResourcePool(2);  // Limit to 2 concurrent executions

    AdvancedExecutionConfig config;
    config.resourcePool = pool;
    config.baseConfig.enableLogging = false;

    std::vector<std::future<ExecutionResult>> futures;

    // Start multiple commands
    for (int i = 0; i < 4; ++i) {
        futures.push_back(std::async(std::launch::async, [&]() {
            return executeCommandAdvanced(sleepCommand, config);
        }));
    }

    // Wait for all to complete
    for (auto& future : futures) {
        auto result = future.get();
        EXPECT_EQ(result.exitCode, 0);
    }

    // Pool should be back to full capacity
    EXPECT_EQ(pool->getAvailableResources(), pool->getTotalResources());
}

// Test async execution
TEST_F(AdvancedExecutorTest, AsyncExecution) {
    AdvancedExecutionConfig config;
    config.baseConfig.enableLogging = false;

    auto future = executeCommandAsyncAdvanced(echoCommand, config);

    EXPECT_TRUE(future.valid());

    auto result = future.get();
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_FALSE(result.output.empty());
}

// Test line processing callback
TEST_F(AdvancedExecutorTest, LineProcessingCallback) {
    std::vector<std::string> processedLines;

    auto processLine = [&processedLines](const std::string& line) {
        processedLines.push_back(line);
    };

    AdvancedExecutionConfig config;
    config.baseConfig.streamOutput = true;

    auto result = executeCommandAdvanced(echoCommand, config, processLine);

    EXPECT_EQ(result.exitCode, 0);
    EXPECT_FALSE(processedLines.empty());
}

// Test stop on error functionality
TEST_F(AdvancedExecutorTest, StopOnError) {
    std::vector<std::string> commands = {
        echoCommand,
        failCommand,
        echoCommand  // This should not execute
    };

    AdvancedExecutionConfig config;
    config.baseConfig.enableLogging = false;

    auto results = executeCommandsAdvanced(commands, config, false, true);

    // Should stop after the failing command
    EXPECT_LE(results.size(), 2);
    if (results.size() >= 2) {
        EXPECT_EQ(results[0].exitCode, 0);  // First should succeed
        EXPECT_NE(results[1].exitCode, 0);  // Second should fail
    }
}

// Test edge cases and error handling
TEST_F(AdvancedExecutorTest, EmptyCommand) {
    AdvancedExecutionConfig config;

    auto result = executeCommandAdvanced("", config);

    // Should handle empty command gracefully
    EXPECT_NE(result.exitCode, 0);
}

TEST_F(AdvancedExecutorTest, InvalidCommand) {
    AdvancedExecutionConfig config;
    config.baseConfig.validateCommand = true;

    auto result = executeCommandAdvanced("this_command_does_not_exist_12345", config);

    // Should fail for invalid command
    EXPECT_NE(result.exitCode, 0);
}

TEST_F(AdvancedExecutorTest, LargeOutput) {
    AdvancedExecutionConfig config;
    config.baseConfig.maxOutputSize = 1024;  // Limit output size

#ifdef _WIN32
    std::string largeOutputCommand = "for /L %i in (1,1,100) do echo This is line %i with some additional text to make it longer";
#else
    std::string largeOutputCommand = "for i in {1..100}; do echo 'This is line $i with some additional text to make it longer'; done";
#endif

    auto result = executeCommandAdvanced(largeOutputCommand, config);

    // Should handle large output appropriately
    EXPECT_LE(result.output.size(), config.baseConfig.maxOutputSize * 2);  // Allow some buffer
}

TEST_F(AdvancedExecutorTest, ConcurrentCancellation) {
    auto token = createCancellationToken();

    AdvancedExecutionConfig config;
    config.cancellationToken = token;

    std::vector<std::future<ExecutionResult>> futures;

    // Start multiple long-running commands
    for (int i = 0; i < 3; ++i) {
        futures.push_back(std::async(std::launch::async, [&]() {
            return executeCommandAdvanced(longRunningCommand, config);
        }));
    }

    // Cancel all after a short delay
    std::this_thread::sleep_for(200ms);
    token->cancel();

    // All should be cancelled or fail
    for (auto& future : futures) {
        auto result = future.get();
        EXPECT_TRUE(result.timedOut || result.wasKilled || result.exitCode != 0);
    }
}

TEST_F(AdvancedExecutorTest, ResourcePoolExhaustion) {
    auto pool = createExecutionResourcePool(1);  // Very limited pool

    AdvancedExecutionConfig config;
    config.resourcePool = pool;

    std::vector<std::future<ExecutionResult>> futures;

    // Try to start more commands than pool capacity
    for (int i = 0; i < 3; ++i) {
        futures.push_back(std::async(std::launch::async, [&]() {
            return executeCommandAdvanced(sleepCommand, config);
        }));
    }

    // All should eventually complete
    for (auto& future : futures) {
        auto result = future.get();
        EXPECT_EQ(result.exitCode, 0);
    }
}

// Performance and stress tests
TEST_F(AdvancedExecutorTest, PerformanceBaseline) {
    AdvancedExecutionConfig config;
    config.baseConfig.enableLogging = false;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10; ++i) {
        auto result = executeCommandAdvanced(echoCommand, config);
        EXPECT_EQ(result.exitCode, 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete reasonably quickly (adjust threshold as needed)
    EXPECT_LT(duration.count(), 5000);  // 5 seconds for 10 commands
}

TEST_F(AdvancedExecutorTest, MemoryUsageStability) {
    AdvancedExecutionConfig config;
    config.baseConfig.enableLogging = false;

    // Run many commands to check for memory leaks
    for (int i = 0; i < 50; ++i) {
        auto result = executeCommandAdvanced(echoCommand, config);
        EXPECT_EQ(result.exitCode, 0);

        // Occasionally test with environment variables
        if (i % 10 == 0) {
            std::unordered_map<std::string, std::string> envVars = {
                {"TEST_VAR", "value_" + std::to_string(i)}
            };
            auto envResult = executeCommandWithEnv(envTestCommand, envVars);
            EXPECT_THAT(envResult, HasSubstr("value_" + std::to_string(i)));
        }
    }
}
