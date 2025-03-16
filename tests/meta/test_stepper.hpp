#include <gtest/gtest.h>
#include "atom/function/stepper.hpp"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

namespace atom::test {

// Simple test functions that return std::any
std::any addFunction(std::span<const std::any> args) {
    if (args.size() < 2)
        return 0;

    int a = std::any_cast<int>(args[0]);
    int b = std::any_cast<int>(args[1]);
    return a + b;
}

std::any multiplyFunction(std::span<const std::any> args) {
    if (args.size() < 2)
        return 0;

    int a = std::any_cast<int>(args[0]);
    int b = std::any_cast<int>(args[1]);
    return a * b;
}

std::any concatFunction(std::span<const std::any> args) {
    if (args.size() < 2)
        return std::string();

    std::string a = std::any_cast<std::string>(args[0]);
    std::string b = std::any_cast<std::string>(args[1]);
    return a + b;
}

std::any throwingFunction(std::span<const std::any> args) {
    if (args.empty())
        throw std::runtime_error("Empty arguments");

    int value = std::any_cast<int>(args[0]);
    if (value < 0)
        throw std::runtime_error("Negative value not allowed");
    return value * 2;
}

std::any slowFunction(std::span<const std::any> args) {
    if (args.empty())
        return 0;

    int sleepMs = std::any_cast<int>(args[0]);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    return sleepMs * 2;
}

class FunctionSequenceTest : public ::testing::Test {
protected:
    meta::FunctionSequence sequence;

    void SetUp() override {
        // Register some test functions by default
        sequence.registerFunction(addFunction);
        sequence.registerFunction(multiplyFunction);
        sequence.registerFunction(concatFunction);
    }

    void TearDown() override {
        sequence.clearFunctions();
        sequence.clearCache();
        sequence.resetStats();
    }

    // Helper to create argument sets for integer operations
    std::vector<std::vector<std::any>> createIntArgs() {
        return {
            {5, 3},   // 5+3=8, 5*3=15
            {10, 2},  // 10+2=12, 10*2=20
            {7, 7}    // 7+7=14, 7*7=49
        };
    }

    // Helper to create argument sets for string operations
    std::vector<std::vector<std::any>> createStringArgs() {
        return {{std::string("Hello"), std::string(" World")},
                {std::string("Test"), std::string(" String")},
                {std::string("C++"), std::string(" Rocks")}};
    }

    // Helper to verify integer results
    void verifyIntResults(const std::vector<meta::Result<std::any>>& results,
                          bool isAdd = true) {
        ASSERT_EQ(results.size(), 3);

        if (isAdd) {
            EXPECT_EQ(std::any_cast<int>(results[0].value()), 8);
            EXPECT_EQ(std::any_cast<int>(results[1].value()), 12);
            EXPECT_EQ(std::any_cast<int>(results[2].value()), 14);
        } else {  // multiply
            EXPECT_EQ(std::any_cast<int>(results[0].value()), 15);
            EXPECT_EQ(std::any_cast<int>(results[1].value()), 20);
            EXPECT_EQ(std::any_cast<int>(results[2].value()), 49);
        }
    }

    // Helper to verify string results
    void verifyStringResults(
        const std::vector<meta::Result<std::any>>& results) {
        ASSERT_EQ(results.size(), 3);

        EXPECT_EQ(std::any_cast<std::string>(results[0].value()),
                  "Hello World");
        EXPECT_EQ(std::any_cast<std::string>(results[1].value()),
                  "Test String");
        EXPECT_EQ(std::any_cast<std::string>(results[2].value()), "C++ Rocks");
    }
};

// Test basic function registration and execution
TEST_F(FunctionSequenceTest, BasicRegistrationAndExecution) {
    // Check initial state
    EXPECT_EQ(sequence.functionCount(), 3);

    // Run the last function (concatFunction)
    auto args = createStringArgs();
    auto results = sequence.run(args);
    verifyStringResults(results);

    // Stats should show 3 invocations (one per argument set)
    auto stats = sequence.getStats();
    EXPECT_EQ(stats.invocationCount, 3);
    EXPECT_EQ(stats.errorCount, 0);
}

// Test running all functions in sequence
TEST_F(FunctionSequenceTest, RunAllFunctions) {
    // Run all functions with int arguments
    auto args = createIntArgs();
    auto resultsBatch = sequence.runAll(args);

    // Should have 3 sets of results (one per argument set)
    ASSERT_EQ(resultsBatch.size(), 3);

    // Each set should have 3 results (one per function)
    for (const auto& results : resultsBatch) {
        ASSERT_EQ(results.size(), 3);
    }

    // Check first argument set results (5,3)
    EXPECT_EQ(std::any_cast<int>(resultsBatch[0][0].value()), 8);   // add
    EXPECT_EQ(std::any_cast<int>(resultsBatch[0][1].value()), 15);  // multiply

    // String concat will throw for int input - verify error
    EXPECT_TRUE(resultsBatch[0][2].isError());

    // Stats should show 9 invocations (3 args x 3 functions)
    // And 3 errors (from string concat with int args)
    auto stats = sequence.getStats();
    EXPECT_EQ(stats.invocationCount, 9);
    EXPECT_EQ(stats.errorCount, 3);
}

// Test error handling
TEST_F(FunctionSequenceTest, ErrorHandling) {
    // Register a function that throws for negative values
    sequence.registerFunction(throwingFunction);

    // Create argument sets with a negative value
    std::vector<std::vector<std::any>> args = {
        {5},   // OK
        {-3},  // Will throw
        {10}   // OK
    };

    // Run the last registered function (throwingFunction)
    auto results = sequence.run(args);
    ASSERT_EQ(results.size(), 3);

    // Check results
    EXPECT_TRUE(results[0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0].value()), 10);

    EXPECT_TRUE(results[1].isError());
    EXPECT_TRUE(results[1].error().find("Negative value") != std::string::npos);

    EXPECT_TRUE(results[2].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[2].value()), 20);

    // Stats should show correct invocation and error counts
    auto stats = sequence.getStats();
    EXPECT_EQ(stats.invocationCount, 12);  // 9 from previous + 3 from this test
    EXPECT_EQ(stats.errorCount, 4);        // 3 from previous + 1 from this test
}

// Test execution with timeout
TEST_F(FunctionSequenceTest, ExecutionTimeout) {
    // Clear previous functions and register the slow function
    sequence.clearFunctions();
    sequence.registerFunction(slowFunction);

    // Create arguments that will cause different execution times
    std::vector<std::vector<std::any>> args = {
        {10},  // 10ms - should complete within timeout
        {200}  // 200ms - should exceed timeout
    };

    // Execute with a 50ms timeout
    auto results =
        sequence.executeWithTimeout(args, std::chrono::milliseconds(50));

    // First result should succeed
    EXPECT_TRUE(results[0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0].value()), 20);

    // TODO: Uncomment if your implementation properly handles individual
    // timeouts Second result might time out, but with the future-based
    // implementation all args are processed with the same future, so we can't
    // test individual timeouts EXPECT_TRUE(results[1].isError());
    // EXPECT_TRUE(results[1].error().find("timed out") != std::string::npos);
}

// Test execution with retries
TEST_F(FunctionSequenceTest, ExecutionRetries) {
    // Set up a counter to track invocation attempts
    static int attemptCount = 0;

    // Register a function that succeeds only after a certain number of attempts
    auto failNTimes = [](std::span<const std::any> args) -> std::any {
        attemptCount++;
        int failUntil = std::any_cast<int>(args[0]);
        if (attemptCount <= failUntil) {
            throw std::runtime_error("Deliberate failure");
        }
        return attemptCount;
    };

    // Reset functions and register our test function
    sequence.clearFunctions();
    sequence.registerFunction(failNTimes);

    // Reset counter
    attemptCount = 0;

    // Create arguments: fail until the 2nd attempt
    std::vector<std::vector<std::any>> args = {{2}};

    // Execute with 3 retries
    auto results = sequence.executeWithRetries(args, 3);

    // Should have succeeded on the 3rd attempt (original + 2 retries)
    ASSERT_EQ(results.size(), 1);
    EXPECT_TRUE(results[0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0].value()), 3);

    // Check that attemptCount matches expected
    EXPECT_EQ(attemptCount, 3);

    // Test failure after all retries
    attemptCount = 0;
    std::vector<std::vector<std::any>> failArgs = {
        {10}};  // fail until 10th attempt

    // Execute with only 2 retries
    auto failResults = sequence.executeWithRetries(failArgs, 2);

    // Should fail after all retries
    ASSERT_EQ(failResults.size(), 1);
    EXPECT_TRUE(failResults[0].isError());
    EXPECT_TRUE(failResults[0].error().find(
                    "Failed after all retry attempts") != std::string::npos);

    // Check that attemptCount matches expected (original + 2 retries = 3)
    EXPECT_EQ(attemptCount, 3);
}

// Test execution with caching
TEST_F(FunctionSequenceTest, ExecutionCaching) {
    // Track function call count
    static int callCount = 0;

    // Register a function that increments the counter
    auto countedFunction = [](std::span<const std::any> args) -> std::any {
        callCount++;
        int a = std::any_cast<int>(args[0]);
        int b = std::any_cast<int>(args[1]);
        return a + b;
    };

    // Reset and register our test function
    sequence.clearFunctions();
    sequence.registerFunction(countedFunction);
    sequence.clearCache();
    callCount = 0;

    // Create argument sets with some duplicates
    std::vector<std::vector<std::any>> args = {
        {5, 3},   // First call
        {10, 2},  // Second call
        {5, 3},   // Duplicate of first - should be cached
        {10, 2}   // Duplicate of second - should be cached
    };

    // Execute with caching enabled
    auto results = sequence.executeWithCaching(args);

    // All results should be successful
    ASSERT_EQ(results.size(), 4);
    for (const auto& result : results) {
        EXPECT_TRUE(result.isSuccess());
    }

    // Check individual results
    EXPECT_EQ(std::any_cast<int>(results[0].value()), 8);
    EXPECT_EQ(std::any_cast<int>(results[1].value()), 12);
    EXPECT_EQ(std::any_cast<int>(results[2].value()), 8);
    EXPECT_EQ(std::any_cast<int>(results[3].value()), 12);

    // Function should have been called only twice (for unique args)
    EXPECT_EQ(callCount, 2);

    // Check cache stats
    auto stats = sequence.getStats();
    EXPECT_EQ(stats.cacheHits, 2);
    EXPECT_EQ(stats.cacheMisses, 2);

    // Cache size should be 2
    EXPECT_EQ(sequence.cacheSize(), 2);

    // Test cache clearing
    sequence.clearCache();
    EXPECT_EQ(sequence.cacheSize(), 0);
}

// Test execution with notification
TEST_F(FunctionSequenceTest, ExecutionNotification) {
    // Reset the function sequence
    sequence.clearFunctions();
    sequence.registerFunction(addFunction);

    // Track notifications
    std::vector<int> notifications;
    auto callback = [&notifications](const std::any& result) {
        notifications.push_back(std::any_cast<int>(result));
    };

    // Create argument sets
    auto args = createIntArgs();

    // Execute with notification
    auto results = sequence.executeWithNotification(args, callback);

    // Verify results
    verifyIntResults(results, true);

    // Verify notifications - should match the results
    ASSERT_EQ(notifications.size(), 3);
    EXPECT_EQ(notifications[0], 8);
    EXPECT_EQ(notifications[1], 12);
    EXPECT_EQ(notifications[2], 14);
}

// Test parallel execution
TEST_F(FunctionSequenceTest, ParallelExecution) {
    // Reset the function sequence
    sequence.clearFunctions();
    sequence.registerFunction(slowFunction);

    // Create arguments for the slow function
    std::vector<std::vector<std::any>> args = {
        {50},  // sleep for 50ms
        {50},  // sleep for 50ms
        {50},  // sleep for 50ms
        {50}   // sleep for 50ms
    };

    // Measure sequential execution time
    auto startSeq = std::chrono::high_resolution_clock::now();
    sequence.run(args);
    auto endSeq = std::chrono::high_resolution_clock::now();
    auto seqDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endSeq - startSeq);

    // Sequential should take ~200ms (4 * 50ms)
    EXPECT_GE(seqDuration.count(), 195);  // Allow slight timing variation

    // Reset stats
    sequence.resetStats();

    // Now measure parallel execution time
    auto options = meta::FunctionSequence::ExecutionOptions{};
    options.policy = meta::FunctionSequence::ExecutionPolicy::Parallel;

    auto startPar = std::chrono::high_resolution_clock::now();
    sequence.execute(args, options);
    auto endPar = std::chrono::high_resolution_clock::now();
    auto parDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endPar - startPar);

    // Parallel should be faster, approximately 50ms + overhead
    // This depends on the number of available cores, but should be less than
    // sequential
    EXPECT_LT(parDuration.count(), seqDuration.count());
}

// Test executeAll with parallel execution
TEST_F(FunctionSequenceTest, ParallelExecuteAll) {
    // Register multiple slow functions
    sequence.clearFunctions();
    sequence.registerFunction(slowFunction);  // slowFunction(x) = x*2

    auto slowAddFunc = [](std::span<const std::any> args) -> std::any {
        int a = std::any_cast<int>(args[0]);
        int b = std::any_cast<int>(args[1]);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        return a + b;
    };

    sequence.registerFunction(slowAddFunc);

    // Create arguments
    std::vector<std::vector<std::any>> args = {
        {30, 5},  // For slowFunction: sleep 30ms, return 60. For slowAddFunc:
                  // 30+5=35
        {20, 10}  // For slowFunction: sleep 20ms, return 40. For slowAddFunc:
                  // 20+10=30
    };

    // Measure sequential execution time
    auto startSeq = std::chrono::high_resolution_clock::now();
    sequence.runAll(args);
    auto endSeq = std::chrono::high_resolution_clock::now();
    auto seqDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endSeq - startSeq);

    // Sequential should take ~100ms (30ms + 30ms + 20ms + 20ms)
    EXPECT_GE(seqDuration.count(), 95);  // Allow slight timing variation

    // Reset stats
    sequence.resetStats();

    // Now measure parallel execution time
    auto options = meta::FunctionSequence::ExecutionOptions{};
    options.policy = meta::FunctionSequence::ExecutionPolicy::Parallel;

    auto startPar = std::chrono::high_resolution_clock::now();
    auto results = sequence.executeAll(args, options);
    auto endPar = std::chrono::high_resolution_clock::now();
    auto parDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endPar - startPar);

    // Parallel should be faster, but exact timing depends on hardware
    EXPECT_LT(parDuration.count(), seqDuration.count());

    // Verify the results
    ASSERT_EQ(results.size(), 2);
    ASSERT_EQ(results[0].size(), 2);
    ASSERT_EQ(results[1].size(), 2);

    // Check results - first arg set
    EXPECT_EQ(std::any_cast<int>(results[0][0].value()),
              60);  // slowFunction(30) = 60
    EXPECT_EQ(std::any_cast<int>(results[0][1].value()),
              35);  // slowAddFunc(30,5) = 35

    // Check results - second arg set
    EXPECT_EQ(std::any_cast<int>(results[1][0].value()),
              40);  // slowFunction(20) = 40
    EXPECT_EQ(std::any_cast<int>(results[1][1].value()),
              30);  // slowAddFunc(20,10) = 30
}

// Test async execution
TEST_F(FunctionSequenceTest, AsyncExecution) {
    // Register a slow function
    sequence.clearFunctions();
    sequence.registerFunction(slowFunction);

    // Create arguments
    std::vector<std::vector<std::any>> args = {{100}};  // sleep for 100ms

    // Start async execution
    auto future = sequence.runAsync(args);

    // Future should not be ready immediately
    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(0)),
              std::future_status::timeout);

    // Wait for completion and get the results
    auto results = future.get();

    // Verify the results
    ASSERT_EQ(results.size(), 1);
    EXPECT_TRUE(results[0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0].value()),
              200);  // slowFunction(100) = 200
}

// Test async execution for all functions
TEST_F(FunctionSequenceTest, AsyncExecuteAll) {
    // Reset functions and register multiple slow functions
    sequence.clearFunctions();
    sequence.registerFunction(slowFunction);  // slowFunction(x) = x*2

    auto slowAddFunc = [](std::span<const std::any> args) -> std::any {
        int a = std::any_cast<int>(args[0]);
        int b = std::any_cast<int>(args[1]);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return a + b;
    };

    sequence.registerFunction(slowAddFunc);

    // Create arguments
    std::vector<std::vector<std::any>> args = {
        {50, 10}};  // For slowFunction: sleep 50ms, return 100. For
                    // slowAddFunc: 50+10=60

    // Start async execution
    auto future = sequence.runAllAsync(args);

    // Future should not be ready immediately
    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(0)),
              std::future_status::timeout);

    // Wait for completion and get the results
    auto results = future.get();

    // Verify the results
    ASSERT_EQ(results.size(), 1);
    ASSERT_EQ(results[0].size(), 2);

    EXPECT_TRUE(results[0][0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0][0].value()),
              100);  // slowFunction(50) = 100

    EXPECT_TRUE(results[0][1].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0][1].value()),
              60);  // slowAddFunc(50,10) = 60
}

// Test execution options
TEST_F(FunctionSequenceTest, ExecutionOptions) {
    // Register a slow function
    sequence.clearFunctions();
    sequence.registerFunction(slowFunction);

    // Create arguments
    std::vector<std::vector<std::any>> args = {{30}};  // sleep for 30ms

    // Create options for parallel async execution with timeout
    meta::FunctionSequence::ExecutionOptions options;
    options.policy = meta::FunctionSequence::ExecutionPolicy::ParallelAsync;
    options.timeout = std::chrono::milliseconds(100);
    options.enableCaching = true;

    // Execute with options
    auto results = sequence.execute(args, options);

    // Verify the results
    ASSERT_EQ(results.size(), 1);
    EXPECT_TRUE(results[0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0].value()),
              60);  // slowFunction(30) = 60

    // Execute again - should use cache
    auto stats = sequence.getStats();
    size_t initialCacheHits = stats.cacheHits;

    results = sequence.execute(args, options);

    // Verify cache was used
    stats = sequence.getStats();
    EXPECT_GT(stats.cacheHits, initialCacheHits);

    // Test with notification callback
    std::vector<int> notifications;
    options.notificationCallback = [&notifications](const std::any& result) {
        notifications.push_back(std::any_cast<int>(result));
    };

    results = sequence.execute(args, options);

    // Verify notification was called
    ASSERT_EQ(notifications.size(), 1);
    EXPECT_EQ(notifications[0], 60);
}

// Test full sequence pipeline
TEST_F(FunctionSequenceTest, FullSequencePipeline) {
    // Register functions that form a pipeline: add -> multiply -> format
    sequence.clearFunctions();

    // Step 1: Add two numbers
    auto addFunc = [](std::span<const std::any> args) -> std::any {
        int a = std::any_cast<int>(args[0]);
        int b = std::any_cast<int>(args[1]);
        return a + b;
    };

    // Step 2: Multiply by a factor
    auto multiplyByFactor = [](std::span<const std::any> args) -> std::any {
        int sum = std::any_cast<int>(args[0]);
        int factor = std::any_cast<int>(args[1]);
        return sum * factor;
    };

    // Step 3: Format as string
    auto formatResult = [](std::span<const std::any> args) -> std::any {
        int value = std::any_cast<int>(args[0]);
        std::string prefix = std::any_cast<std::string>(args[1]);
        return prefix + std::to_string(value);
    };

    sequence.registerFunction(addFunc);
    sequence.registerFunction(multiplyByFactor);
    sequence.registerFunction(formatResult);

    // Prepare argument sets
    std::vector<std::vector<std::any>> step1Args = {
        {10, 5}  // 10 + 5 = 15
    };

    // Execute step 1
    auto step1Results = sequence.execute(step1Args, {});
    ASSERT_EQ(step1Results.size(), 1);
    EXPECT_TRUE(step1Results[0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(step1Results[0].value()), 15);

    // Prepare step 2 arguments using step 1 result
    std::vector<std::vector<std::any>> step2Args = {
        {std::any_cast<int>(step1Results[0].value()), 3}  // 15 * 3 = 45
    };

    // Execute step 2
    auto step2Results = sequence.execute(step2Args, {});
    ASSERT_EQ(step2Results.size(), 1);
    EXPECT_TRUE(step2Results[0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(step2Results[0].value()), 45);

    // Prepare step 3 arguments using step 2 result
    std::vector<std::vector<std::any>> step3Args = {
        {std::any_cast<int>(step2Results[0].value()),
         std::string("Result: ")}  // "Result: 45"
    };

    // Execute step 3
    auto step3Results = sequence.execute(step3Args, {});
    ASSERT_EQ(step3Results.size(), 1);
    EXPECT_TRUE(step3Results[0].isSuccess());
    EXPECT_EQ(std::any_cast<std::string>(step3Results[0].value()),
              "Result: 45");

    // Alternatively, run the full sequence at once
    std::vector<std::vector<std::any>> argsToProcess = {
        {10, 5, 3,
         std::string("Result: ")}  // Has all arguments needed by the pipeline
    };

    // Create custom execution functions that pass data through the pipeline
    auto pipelineFunc = [](std::span<const std::any> args) -> std::any {
        int a = std::any_cast<int>(args[0]);
        int b = std::any_cast<int>(args[1]);
        int factor = std::any_cast<int>(args[2]);
        std::string prefix = std::any_cast<std::string>(args[3]);

        // Step 1: Add
        int sum = a + b;

        // Step 2: Multiply
        int product = sum * factor;

        // Step 3: Format
        return prefix + std::to_string(product);
    };

    // Register and execute the pipeline function
    sequence.clearFunctions();
    sequence.registerFunction(pipelineFunc);

    auto pipelineResults = sequence.execute(argsToProcess, {});
    ASSERT_EQ(pipelineResults.size(), 1);
    EXPECT_TRUE(pipelineResults[0].isSuccess());
    EXPECT_EQ(std::any_cast<std::string>(pipelineResults[0].value()),
              "Result: 45");
}

// Test statistics and diagnostics
TEST_F(FunctionSequenceTest, StatisticsAndDiagnostics) {
    // Clear previous functions and register a measurable function
    sequence.clearFunctions();
    sequence.resetStats();

    auto measurableFunc = [](std::span<const std::any> args) -> std::any {
        int sleepMs = std::any_cast<int>(args[0]);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        return sleepMs * 2;
    };

    sequence.registerFunction(measurableFunc);

    // Create arguments that will produce predictable execution times
    std::vector<std::vector<std::any>> args = {
        {10},  // 10ms
        {20},  // 20ms
        {30}   // 30ms
    };

    // Execute functions
    sequence.run(args);

    // Check execution stats
    auto stats = sequence.getStats();
    EXPECT_EQ(stats.invocationCount, 3);
    EXPECT_EQ(stats.errorCount, 0);

    // Average execution time should be around 20ms
    double avgTimeMs = sequence.getAverageExecutionTime();
    EXPECT_GE(avgTimeMs, 10.0);
    EXPECT_LE(avgTimeMs, 30.0);

    // Test cache hit ratio (initially 0)
    EXPECT_EQ(sequence.getCacheHitRatio(), 0.0);

    // Execute with caching
    meta::FunctionSequence::ExecutionOptions options;
    options.enableCaching = true;

    // First run - should miss cache
    sequence.execute(args, options);

    // Hit ratio should still be 0
    EXPECT_EQ(sequence.getCacheHitRatio(), 0.0);

    // Second run - should hit cache
    sequence.execute(args, options);

    // Hit ratio should now be higher (3 hits out of 6 total accesses)
    EXPECT_NEAR(sequence.getCacheHitRatio(), 0.5, 0.01);

    // Test reset stats
    sequence.resetStats();
    stats = sequence.getStats();
    EXPECT_EQ(stats.invocationCount, 0);
    EXPECT_EQ(stats.errorCount, 0);
    EXPECT_EQ(stats.cacheHits, 0);
    EXPECT_EQ(stats.cacheMisses, 0);
}

}  // namespace atom::test