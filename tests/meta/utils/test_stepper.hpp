#include <gtest/gtest.h>
#include "atom/meta/stepper.hpp"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <set>
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
    void verifyIntResults(const std::vector<meta::StepResult<std::any>>& results,
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
        const std::vector<meta::StepResult<std::any>>& results) {
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

    // Stats should show correct invocation and error counts.
    // Each test owns a fresh FunctionSequence, so only this test's
    // 3 invocations (1 of which failed) are counted.
    auto stats = sequence.getStats();
    EXPECT_EQ(stats.invocationCount, 3);
    EXPECT_EQ(stats.errorCount, 1);
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
    ASSERT_EQ(results.size(), 2);
    EXPECT_TRUE(results[0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0].value()), 20);

    // Second result exceeds the per-argument-set timeout
    EXPECT_TRUE(results[1].isError());
    EXPECT_TRUE(results[1].error().find("timed out") != std::string::npos);
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

    // execute()/run() invoke the LAST registered function, so register each
    // pipeline stage right before its step.
    sequence.registerFunction(addFunc);

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
    sequence.registerFunction(multiplyByFactor);
    std::vector<std::vector<std::any>> step2Args = {
        {std::any_cast<int>(step1Results[0].value()), 3}  // 15 * 3 = 45
    };

    // Execute step 2
    auto step2Results = sequence.execute(step2Args, {});
    ASSERT_EQ(step2Results.size(), 1);
    EXPECT_TRUE(step2Results[0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(step2Results[0].value()), 45);

    // Prepare step 3 arguments using step 2 result
    sequence.registerFunction(formatResult);
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

// ---------------------------------------------------------------------------
// StepResult edge cases: throw paths for value() and error()
// ---------------------------------------------------------------------------

TEST(StepResultTest, ValueThrowsOnError) {
    auto r = meta::StepResult<int>::makeError("oops");
    EXPECT_TRUE(r.isError());
    EXPECT_THROW({ (void)r.value(); }, std::runtime_error);
}

TEST(StepResultTest, ErrorThrowsOnSuccess) {
    auto r = meta::StepResult<int>::makeSuccess(42);
    EXPECT_TRUE(r.isSuccess());
    EXPECT_THROW({ (void)r.error(); }, std::runtime_error);
}

TEST(StepResultTest, ValueOrReturnsDefaultWhenError) {
    auto r = meta::StepResult<int>::makeError("bad");
    EXPECT_EQ(r.valueOr(99), 99);
}

TEST(StepResultTest, ValueOrReturnsValueWhenSuccess) {
    auto r = meta::StepResult<int>::makeSuccess(42);
    EXPECT_EQ(r.valueOr(99), 42);  // covers the isSuccess() true branch in valueOr
}

TEST(StepResultTest, DefaultConstructedIsError) {
    meta::StepResult<int> r;
    EXPECT_TRUE(r.isError());
    EXPECT_FALSE(r.isSuccess());
}

// ---------------------------------------------------------------------------
// Empty-sequence guard paths
// ---------------------------------------------------------------------------

TEST(FunctionSequenceEmptyTest, RunReturnsErrorWhenEmpty) {
    meta::FunctionSequence seq;
    std::vector<std::vector<std::any>> args = {{1}};
    auto results = seq.run(args);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].isError());
}

TEST(FunctionSequenceEmptyTest, RunAllReturnsErrorWhenEmpty) {
    meta::FunctionSequence seq;
    std::vector<std::vector<std::any>> args = {{1}};
    auto results = seq.runAll(args);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0][0].isError());
}

TEST(FunctionSequenceEmptyTest, ExecuteWithTimeoutEmptySeq) {
    meta::FunctionSequence seq;
    std::vector<std::vector<std::any>> args = {{1}};
    auto results =
        seq.executeWithTimeout(args, std::chrono::milliseconds(100));
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].isError());
}

TEST(FunctionSequenceEmptyTest, ExecuteWithCachingEmptySeq) {
    meta::FunctionSequence seq;
    std::vector<std::vector<std::any>> args = {{1}};
    auto results = seq.executeWithCaching(args);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].isError());
}

TEST(FunctionSequenceEmptyTest, ExecuteAllWithCachingEmptySeq) {
    meta::FunctionSequence seq;
    std::vector<std::vector<std::any>> args = {{1}};
    auto results = seq.executeAllWithCaching(args);
    ASSERT_FALSE(results.empty());
    EXPECT_TRUE(results[0][0].isError());
}

TEST(FunctionSequenceEmptyTest, ExecuteParallelEmptySeq) {
    meta::FunctionSequence seq;
    std::vector<std::vector<std::any>> args = {{1}};
    meta::FunctionSequence::ExecutionOptions opts;
    opts.policy = meta::FunctionSequence::ExecutionPolicy::Parallel;
    auto results = seq.execute(args, opts);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].isError());
}

TEST(FunctionSequenceEmptyTest, ExecuteAllParallelEmptySeq) {
    meta::FunctionSequence seq;
    std::vector<std::vector<std::any>> args = {{1}};
    meta::FunctionSequence::ExecutionOptions opts;
    opts.policy = meta::FunctionSequence::ExecutionPolicy::Parallel;
    auto results = seq.executeAll(args, opts);
    ASSERT_FALSE(results.empty());
    EXPECT_TRUE(results[0][0].isError());
}

// ---------------------------------------------------------------------------
// execute() dispatch branches not yet covered
// ---------------------------------------------------------------------------

// Branch: execute() with timeout option (sequential, non-parallel)
TEST(FunctionSequenceExecuteTest, ExecuteDispatchTimeout) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) * 2;
        });
    std::vector<std::vector<std::any>> args = {{7}};
    meta::FunctionSequence::ExecutionOptions opts;
    opts.timeout = std::chrono::milliseconds(500);
    auto results = seq.execute(args, opts);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0].value()), 14);
}

// Branch: execute() with retryCount option (sequential)
TEST(FunctionSequenceExecuteTest, ExecuteDispatchRetry) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) + 1;
        });
    std::vector<std::vector<std::any>> args = {{5}};
    meta::FunctionSequence::ExecutionOptions opts;
    opts.retryCount = 2;
    auto results = seq.execute(args, opts);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0].value()), 6);
}

// Branch: execute() with notification callback only (no timeout/retry/cache)
TEST(FunctionSequenceExecuteTest, ExecuteDispatchNotification) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) * 3;
        });
    std::vector<std::vector<std::any>> args = {{4}};
    meta::FunctionSequence::ExecutionOptions opts;
    std::vector<int> notified;
    opts.notificationCallback = [&notified](const std::any& v) {
        notified.push_back(std::any_cast<int>(v));
    };
    auto results = seq.execute(args, opts);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0].value()), 12);
    ASSERT_EQ(notified.size(), 1u);
    EXPECT_EQ(notified[0], 12);
}

// Branch: execute() plain run (no options set)
TEST(FunctionSequenceExecuteTest, ExecuteDispatchPlainRun) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) - 1;
        });
    std::vector<std::vector<std::any>> args = {{10}};
    meta::FunctionSequence::ExecutionOptions opts;  // all defaults
    auto results = seq.execute(args, opts);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0].value()), 9);
}

// ---------------------------------------------------------------------------
// executeAll() dispatch branches
// ---------------------------------------------------------------------------

// Branch: executeAll() with ParallelAsync policy
TEST(FunctionSequenceExecuteAllTest, ExecuteAllDispatchParallelAsync) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) + 10;
        });
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) * 2;
        });
    std::vector<std::vector<std::any>> args = {{5}};
    meta::FunctionSequence::ExecutionOptions opts;
    opts.policy = meta::FunctionSequence::ExecutionPolicy::ParallelAsync;
    auto results = seq.executeAll(args, opts);
    ASSERT_EQ(results.size(), 1u);
    ASSERT_EQ(results[0].size(), 2u);
    EXPECT_TRUE(results[0][0].isSuccess());
    EXPECT_TRUE(results[0][1].isSuccess());
}

// Branch: executeAll() with timeout option
TEST(FunctionSequenceExecuteAllTest, ExecuteAllDispatchTimeout) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) + 1;
        });
    std::vector<std::vector<std::any>> args = {{3}};
    meta::FunctionSequence::ExecutionOptions opts;
    opts.timeout = std::chrono::milliseconds(500);
    auto results = seq.executeAll(args, opts);
    ASSERT_EQ(results.size(), 1u);
    ASSERT_EQ(results[0].size(), 1u);
    EXPECT_TRUE(results[0][0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0][0].value()), 4);
}

// Branch: executeAll() with retryCount option
TEST(FunctionSequenceExecuteAllTest, ExecuteAllDispatchRetry) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) * 3;
        });
    std::vector<std::vector<std::any>> args = {{2}};
    meta::FunctionSequence::ExecutionOptions opts;
    opts.retryCount = 1;
    auto results = seq.executeAll(args, opts);
    ASSERT_EQ(results.size(), 1u);
    ASSERT_EQ(results[0].size(), 1u);
    EXPECT_TRUE(results[0][0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0][0].value()), 6);
}

// Branch: executeAll() with caching option
TEST(FunctionSequenceExecuteAllTest, ExecuteAllDispatchCaching) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) + 100;
        });
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) * 10;
        });
    std::vector<std::vector<std::any>> args = {{5}, {5}};  // duplicate to hit cache
    meta::FunctionSequence::ExecutionOptions opts;
    opts.enableCaching = true;
    auto results = seq.executeAll(args, opts);
    ASSERT_EQ(results.size(), 2u);
    ASSERT_EQ(results[0].size(), 2u);
    // second batch should be cache hits
    EXPECT_TRUE(results[1][0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[1][0].value()), 105);
    EXPECT_TRUE(results[1][1].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[1][1].value()), 50);
    auto stats = seq.getStats();
    EXPECT_GT(stats.cacheHits, 0u);
}

// Branch: executeAll() plain runAll
TEST(FunctionSequenceExecuteAllTest, ExecuteAllDispatchPlain) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) + 7;
        });
    std::vector<std::vector<std::any>> args = {{1}};
    meta::FunctionSequence::ExecutionOptions opts;  // all defaults
    auto results = seq.executeAll(args, opts);
    ASSERT_EQ(results.size(), 1u);
    ASSERT_EQ(results[0].size(), 1u);
    EXPECT_TRUE(results[0][0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0][0].value()), 8);
}

// ---------------------------------------------------------------------------
// executeAllWithTimeout — not covered at all yet
// ---------------------------------------------------------------------------

TEST(FunctionSequenceTest2, ExecuteAllWithTimeoutCompletesInTime) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            int ms = std::any_cast<int>(args[0]);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            return ms * 2;
        });
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) + 1;
        });
    std::vector<std::vector<std::any>> args = {{10}};
    auto results =
        seq.executeAllWithTimeout(args, std::chrono::milliseconds(500));
    ASSERT_EQ(results.size(), 1u);
    ASSERT_EQ(results[0].size(), 2u);
    EXPECT_TRUE(results[0][0].isSuccess());
    EXPECT_TRUE(results[0][1].isSuccess());
}

TEST(FunctionSequenceTest2, ExecuteAllWithTimeoutExpires) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            int ms = std::any_cast<int>(args[0]);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            return ms;
        });
    std::vector<std::vector<std::any>> args = {{300}};
    auto results =
        seq.executeAllWithTimeout(args, std::chrono::milliseconds(30));
    ASSERT_FALSE(results.empty());
    EXPECT_TRUE(results[0][0].isError());
    EXPECT_TRUE(results[0][0].error().find("timed out") != std::string::npos);
}

// ---------------------------------------------------------------------------
// executeAllWithRetries — not covered at all yet
// ---------------------------------------------------------------------------

TEST(FunctionSequenceTest2, ExecuteAllWithRetriesSucceedsOnRetry) {
    static int callsAll = 0;
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            ++callsAll;
            int threshold = std::any_cast<int>(args[0]);
            if (callsAll <= threshold)
                throw std::runtime_error("not ready");
            return callsAll;
        });
    callsAll = 0;
    std::vector<std::vector<std::any>> args = {{2}};
    // function throws until callsAll > 2, so succeeds on 3rd attempt (2 retries)
    auto results = seq.executeAllWithRetries(args, 3);
    ASSERT_EQ(results.size(), 1u);
    ASSERT_EQ(results[0].size(), 1u);
    EXPECT_TRUE(results[0][0].isSuccess());
}

TEST(FunctionSequenceTest2, ExecuteAllWithRetriesExhaustRetries) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            // Always returns an error result (no throw; just fails)
            int v = std::any_cast<int>(args[0]);
            (void)v;
            throw std::runtime_error("always fails");
            return std::any{};
        });
    std::vector<std::vector<std::any>> args = {{0}};
    // With 0 retries the catch fires at attempts==0==retries and returns error
    auto results = seq.executeAllWithRetries(args, 0);
    ASSERT_FALSE(results.empty());
    // Either got an error-wrapped result or the catch-return path
    bool anyError = false;
    for (const auto& batch : results)
        for (const auto& r : batch)
            if (r.isError()) anyError = true;
    EXPECT_TRUE(anyError);
}

// Cover the "not success after retries" loop-exit path in executeAllWithRetries
TEST(FunctionSequenceTest2, ExecuteAllWithRetriesResultStillFailingAfterLoop) {
    meta::FunctionSequence seq;
    // Function that always returns but always produces an "error" result
    // We do this by making run() internalize the error, not throw.
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            throw std::runtime_error("persistent error");
            return std::any{};
        });
    std::vector<std::vector<std::any>> args = {{0}};
    // 1 retry: attempts goes 0→catch(attempts==0, retries==1 no return)
    // then attempts++ → 1, loop condition: attempts<=retries (1<=1) → retry
    // second time: catch(attempts==1==retries) → returns error
    auto results = seq.executeAllWithRetries(args, 1);
    ASSERT_FALSE(results.empty());
    bool anyError = false;
    for (const auto& batch : results)
        for (const auto& r : batch)
            if (r.isError()) anyError = true;
    EXPECT_TRUE(anyError);
}

// ---------------------------------------------------------------------------
// executeWithCaching exception path
// ---------------------------------------------------------------------------

TEST(FunctionSequenceTest2, ExecuteWithCachingExceptionPath) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            throw std::runtime_error("cache exception");
            return std::any{};
        });
    std::vector<std::vector<std::any>> args = {{1}};
    auto results = seq.executeWithCaching(args);
    ASSERT_FALSE(results.empty());
    EXPECT_TRUE(results.back().isError());
    EXPECT_TRUE(results.back().error().find("Exception caught") !=
                std::string::npos);
}

// ---------------------------------------------------------------------------
// executeAllWithCaching — full coverage including cache hits and exception
// ---------------------------------------------------------------------------

TEST(FunctionSequenceTest2, ExecuteAllWithCachingHitAndMiss) {
    meta::FunctionSequence seq;
    seq.resetStats();
    seq.clearCache();
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) * 5;
        });
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) + 100;
        });
    // Two identical arg sets → second batch entirely from cache
    std::vector<std::vector<std::any>> args = {{3}, {3}};
    auto results = seq.executeAllWithCaching(args);
    ASSERT_EQ(results.size(), 2u);
    ASSERT_EQ(results[0].size(), 2u);
    ASSERT_EQ(results[1].size(), 2u);
    EXPECT_EQ(std::any_cast<int>(results[0][0].value()), 15);   // 3*5
    EXPECT_EQ(std::any_cast<int>(results[0][1].value()), 103);  // 3+100
    EXPECT_EQ(std::any_cast<int>(results[1][0].value()), 15);   // cache hit
    EXPECT_EQ(std::any_cast<int>(results[1][1].value()), 103);  // cache hit
    auto stats = seq.getStats();
    EXPECT_GE(stats.cacheHits, 2u);
}

TEST(FunctionSequenceTest2, ExecuteAllWithCachingExceptionPath) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            throw std::runtime_error("all-caching exception");
            return std::any{};
        });
    std::vector<std::vector<std::any>> args = {{1}};
    auto results = seq.executeAllWithCaching(args);
    ASSERT_FALSE(results.empty());
    EXPECT_TRUE(results[0][0].isError());
    EXPECT_TRUE(results[0][0].error().find("Exception caught") !=
                std::string::npos);
}

// ---------------------------------------------------------------------------
// executeParallel: worker catch (exception in parallel worker),
// notification callback in parallel worker (both cache-hit and non-cache paths)
// ---------------------------------------------------------------------------

TEST(FunctionSequenceTest2, ExecuteParallelWorkerCatchBranch) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            throw std::runtime_error("parallel worker exception");
            return std::any{};
        });
    std::vector<std::vector<std::any>> args = {{1}, {2}};
    meta::FunctionSequence::ExecutionOptions opts;
    opts.policy = meta::FunctionSequence::ExecutionPolicy::Parallel;
    auto results = seq.execute(args, opts);
    ASSERT_EQ(results.size(), 2u);
    for (const auto& r : results) {
        EXPECT_TRUE(r.isError());
        EXPECT_TRUE(r.error().find("parallel execution") != std::string::npos);
    }
}

TEST(FunctionSequenceTest2, ExecuteParallelWithCachingAndNotification) {
    meta::FunctionSequence seq;
    seq.clearCache();
    seq.resetStats();
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) + 50;
        });
    // Two identical sets → second gets served from cache inside worker
    std::vector<std::vector<std::any>> args = {{7}, {7}};
    meta::FunctionSequence::ExecutionOptions opts;
    opts.policy = meta::FunctionSequence::ExecutionPolicy::Parallel;
    opts.enableCaching = true;
    std::vector<int> notified;
    opts.notificationCallback = [&notified](const std::any& v) {
        notified.push_back(std::any_cast<int>(v));
    };
    auto results = seq.execute(args, opts);
    ASSERT_EQ(results.size(), 2u);
    for (const auto& r : results) {
        EXPECT_TRUE(r.isSuccess());
        EXPECT_EQ(std::any_cast<int>(r.value()), 57);
    }
    // At least one notification from the cache-hit path
    EXPECT_GE(notified.size(), 1u);
    auto stats = seq.getStats();
    EXPECT_GE(stats.cacheHits, 1u);
}

// ---------------------------------------------------------------------------
// executeAllParallel worker catch branch
// ---------------------------------------------------------------------------

TEST(FunctionSequenceTest2, ExecuteAllParallelWorkerCatchBranch) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            throw std::runtime_error("all-parallel worker exception");
            return std::any{};
        });
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) + 1;
        });
    std::vector<std::vector<std::any>> args = {{1}, {2}};
    meta::FunctionSequence::ExecutionOptions opts;
    opts.policy = meta::FunctionSequence::ExecutionPolicy::Parallel;
    auto results = seq.executeAll(args, opts);
    ASSERT_EQ(results.size(), 2u);
    // First function always throws
    for (const auto& batchRow : results) {
        EXPECT_TRUE(batchRow[0].isError());
    }
}

// ---------------------------------------------------------------------------
// executeParallelAsync (direct call, not via execute)
// ---------------------------------------------------------------------------

TEST(FunctionSequenceTest2, ExecuteParallelAsyncDirect) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) * 4;
        });
    std::vector<std::vector<std::any>> args = {{3}, {5}};
    meta::FunctionSequence::ExecutionOptions opts;
    auto future = seq.executeParallelAsync(std::span(args), opts);
    auto results = future.get();
    ASSERT_EQ(results.size(), 2u);
    EXPECT_TRUE(results[0].isSuccess());
    EXPECT_TRUE(results[1].isSuccess());
    // Values are 12 and 20
    std::set<int> values{std::any_cast<int>(results[0].value()),
                         std::any_cast<int>(results[1].value())};
    EXPECT_TRUE(values.count(12));
    EXPECT_TRUE(values.count(20));
}

// ---------------------------------------------------------------------------
// executeAllParallelAsync (direct call)
// ---------------------------------------------------------------------------

TEST(FunctionSequenceTest2, ExecuteAllParallelAsyncDirect) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) + 2;
        });
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) * 2;
        });
    std::vector<std::vector<std::any>> args = {{4}};
    meta::FunctionSequence::ExecutionOptions opts;
    auto future = seq.executeAllParallelAsync(std::span(args), opts);
    auto results = future.get();
    ASSERT_EQ(results.size(), 1u);
    ASSERT_EQ(results[0].size(), 2u);
    EXPECT_TRUE(results[0][0].isSuccess());
    EXPECT_TRUE(results[0][1].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0][0].value()), 6);   // 4+2
    EXPECT_EQ(std::any_cast<int>(results[0][1].value()), 8);   // 4*2
}

// ---------------------------------------------------------------------------
// getAverageExecutionTime zero-invocation branch
// ---------------------------------------------------------------------------

TEST(FunctionSequenceTest2, AverageExecTimeZeroWhenNoInvocations) {
    meta::FunctionSequence seq;
    seq.resetStats();
    EXPECT_EQ(seq.getAverageExecutionTime(), 0.0);
}

// ---------------------------------------------------------------------------
// pruneCache / setMaxCacheSize eviction path
// ---------------------------------------------------------------------------

TEST(FunctionSequenceTest2, PruneCacheEvictor) {
    meta::FunctionSequence seq;
    seq.clearCache();
    // Register a function that uses int args so cache keys vary
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]);
        });
    // Fill cache with 5 distinct entries
    for (int i = 0; i < 5; ++i) {
        std::vector<std::vector<std::any>> args = {{i}};
        (void)seq.executeWithCaching(args);
    }
    EXPECT_EQ(seq.cacheSize(), 5u);

    // Now shrink max size to 2 → pruneCache should fire and evict 3 entries
    seq.setMaxCacheSize(2);
    EXPECT_LE(seq.cacheSize(), 2u);
}

// ---------------------------------------------------------------------------
// hashArgument branches: unsigned int, long long, size_t, double, float,
// bool, std::string, std::string_view  (exercised via executeWithCaching)
// ---------------------------------------------------------------------------

TEST(FunctionSequenceTest2, HashArgumentUnsignedInt) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any { return 1; });
    unsigned int v = 42u;
    std::vector<std::vector<std::any>> args = {{v}};
    auto r1 = seq.executeWithCaching(args);
    auto r2 = seq.executeWithCaching(args);  // cache hit
    EXPECT_TRUE(r1[0].isSuccess());
    EXPECT_TRUE(r2[0].isSuccess());
    EXPECT_GE(seq.getStats().cacheHits, 1u);
}

TEST(FunctionSequenceTest2, HashArgumentLongLong) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any { return 2; });
    long long v = 999LL;
    std::vector<std::vector<std::any>> args = {{v}};
    auto r1 = seq.executeWithCaching(args);
    auto r2 = seq.executeWithCaching(args);
    EXPECT_TRUE(r1[0].isSuccess());
    EXPECT_GE(seq.getStats().cacheHits, 1u);
}

TEST(FunctionSequenceTest2, HashArgumentSizeT) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any { return 3; });
    std::size_t v = 77u;
    std::vector<std::vector<std::any>> args = {{v}};
    auto r1 = seq.executeWithCaching(args);
    auto r2 = seq.executeWithCaching(args);
    EXPECT_TRUE(r1[0].isSuccess());
    EXPECT_GE(seq.getStats().cacheHits, 1u);
}

TEST(FunctionSequenceTest2, HashArgumentDouble) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any { return 4; });
    double v = 3.14;
    std::vector<std::vector<std::any>> args = {{v}};
    auto r1 = seq.executeWithCaching(args);
    auto r2 = seq.executeWithCaching(args);
    EXPECT_TRUE(r1[0].isSuccess());
    EXPECT_GE(seq.getStats().cacheHits, 1u);
}

TEST(FunctionSequenceTest2, HashArgumentFloat) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any { return 5; });
    float v = 2.71f;
    std::vector<std::vector<std::any>> args = {{v}};
    auto r1 = seq.executeWithCaching(args);
    auto r2 = seq.executeWithCaching(args);
    EXPECT_TRUE(r1[0].isSuccess());
    EXPECT_GE(seq.getStats().cacheHits, 1u);
}

TEST(FunctionSequenceTest2, HashArgumentBool) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any { return 6; });
    bool v = true;
    std::vector<std::vector<std::any>> args = {{v}};
    auto r1 = seq.executeWithCaching(args);
    auto r2 = seq.executeWithCaching(args);
    EXPECT_TRUE(r1[0].isSuccess());
    EXPECT_GE(seq.getStats().cacheHits, 1u);
}

TEST(FunctionSequenceTest2, HashArgumentString) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any { return 7; });
    std::string v = "hello";
    std::vector<std::vector<std::any>> args = {{v}};
    auto r1 = seq.executeWithCaching(args);
    auto r2 = seq.executeWithCaching(args);
    EXPECT_TRUE(r1[0].isSuccess());
    EXPECT_GE(seq.getStats().cacheHits, 1u);
}

TEST(FunctionSequenceTest2, HashArgumentStringView) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any { return 8; });
    std::string_view v = "world";
    std::vector<std::vector<std::any>> args = {{v}};
    auto r1 = seq.executeWithCaching(args);
    auto r2 = seq.executeWithCaching(args);
    EXPECT_TRUE(r1[0].isSuccess());
    EXPECT_GE(seq.getStats().cacheHits, 1u);
}

// ---------------------------------------------------------------------------
// generateCacheKey with functionIndex (executeAllWithCaching path)
// covered implicitly but ensure the "func<N>_" prefix path is hit
// ---------------------------------------------------------------------------

TEST(FunctionSequenceTest2, GenerateCacheKeyWithFunctionIndex) {
    meta::FunctionSequence seq;
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]);
        });
    seq.registerFunction(
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) + 1;
        });
    // Same args, two functions: cache keys differ (include func index)
    std::vector<std::vector<std::any>> args = {{5}, {5}};  // duplicate
    auto results = seq.executeAllWithCaching(args);
    ASSERT_EQ(results.size(), 2u);
    // Second round should come from cache
    auto stats = seq.getStats();
    EXPECT_GE(stats.cacheHits, 2u);
}

// ---------------------------------------------------------------------------
// StepperBuilder, buildStepper, addNamedStep, withCacheSize
// ---------------------------------------------------------------------------

TEST(StepperBuilderTest, BuilderAddStepAndBuild) {
    auto stepper =
        meta::buildStepper()
            .addStep([](std::vector<std::any> args) -> std::any {
                return std::any_cast<int>(args[0]) * 2;
            })
            .build();
    ASSERT_NE(stepper, nullptr);
    EXPECT_EQ(stepper->functionCount(), 1u);
    std::vector<std::vector<std::any>> args = {{6}};
    auto results = stepper->run(args);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0].value()), 12);
}

TEST(StepperBuilderTest, BuilderAddNamedStep) {
    auto stepper =
        meta::buildStepper()
            .addNamedStep("double",
                          [](std::vector<std::any> args) -> std::any {
                              return std::any_cast<int>(args[0]) * 2;
                          })
            .withCacheSize(50)
            .build();
    ASSERT_NE(stepper, nullptr);
    EXPECT_EQ(stepper->functionCount(), 1u);
    std::vector<std::vector<std::any>> args = {{7}};
    auto results = stepper->run(args);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].isSuccess());
    EXPECT_EQ(std::any_cast<int>(results[0].value()), 14);
}

// ---------------------------------------------------------------------------
// RetryStep / makeRetryStep
// ---------------------------------------------------------------------------

TEST(RetryStepTest, SucceedsOnFirstAttempt) {
    auto step = meta::makeRetryStep(
        [](std::vector<std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) + 1;
        },
        3, std::chrono::milliseconds{0});
    auto result = step({std::any{5}});
    EXPECT_EQ(std::any_cast<int>(result), 6);
}

TEST(RetryStepTest, RetriesAndEventuallyFails) {
    // Always throws — should exhaust retries and return empty any{}
    int calls = 0;
    auto step = meta::makeRetryStep(
        [&calls](std::vector<std::any>) -> std::any {
            ++calls;
            throw std::runtime_error("always fail");
            return std::any{};
        },
        2, std::chrono::milliseconds{0});
    auto result = step({});
    EXPECT_EQ(calls, 2);       // max_retries_ = 2
    EXPECT_FALSE(result.has_value());
}

TEST(RetryStepTest, RetriesUntilSuccess) {
    int calls = 0;
    auto step = meta::makeRetryStep(
        [&calls](std::vector<std::any>) -> std::any {
            ++calls;
            if (calls < 3)
                throw std::runtime_error("not yet");
            return calls;
        },
        5, std::chrono::milliseconds{0});
    auto result = step({});
    EXPECT_EQ(std::any_cast<int>(result), 3);
    EXPECT_EQ(calls, 3);
}

// ---------------------------------------------------------------------------
// ConditionalStep / makeConditionalStep
// ---------------------------------------------------------------------------

TEST(ConditionalStepTest, ExecutesWhenConditionTrue) {
    auto step = meta::makeConditionalStep(
        [](std::vector<std::any> args) -> bool {
            return std::any_cast<int>(args[0]) > 0;
        },
        [](std::vector<std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) * 10;
        });
    auto result = step({std::any{5}});
    EXPECT_EQ(std::any_cast<int>(result), 50);
}

TEST(ConditionalStepTest, SkipsWhenConditionFalse) {
    auto step = meta::makeConditionalStep(
        [](std::vector<std::any> args) -> bool {
            return std::any_cast<int>(args[0]) > 0;
        },
        [](std::vector<std::any>) -> std::any { return 999; });
    auto result = step({std::any{-1}});
    EXPECT_FALSE(result.has_value());  // returns empty any
}

// ---------------------------------------------------------------------------
// ParallelStepper
// ---------------------------------------------------------------------------

TEST(ParallelStepperTest, ExecuteAllReturnsResults) {
    meta::ParallelStepper ps;
    ps.addStep([](std::vector<std::any> args) -> std::any {
        return std::any_cast<int>(args[0]) + 1;
    });
    ps.addStep([](std::vector<std::any> args) -> std::any {
        return std::any_cast<int>(args[0]) * 2;
    });
    EXPECT_EQ(ps.stepCount(), 2u);
    auto results = ps.executeAll({std::any{5}});
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(std::any_cast<int>(results[0]), 6);
    EXPECT_EQ(std::any_cast<int>(results[1]), 10);
}

// ---------------------------------------------------------------------------
// StepObserver
// ---------------------------------------------------------------------------

TEST(StepObserverTest, NotifiesAllCallbacks) {
    meta::StepObserver obs;
    std::vector<std::size_t> beforeSteps, afterSteps, errorSteps;

    obs.onBefore([&](std::size_t step, const std::vector<std::any>&) {
        beforeSteps.push_back(step);
    });
    obs.onAfter([&](std::size_t step, const std::any&) {
        afterSteps.push_back(step);
    });
    obs.onError([&](std::size_t step, const std::exception&) {
        errorSteps.push_back(step);
    });

    obs.notifyBefore(0, {});
    obs.notifyBefore(1, {});
    obs.notifyAfter(0, std::any{42});
    obs.notifyError(1, std::runtime_error("oops"));

    EXPECT_EQ(beforeSteps.size(), 2u);
    EXPECT_EQ(beforeSteps[0], 0u);
    EXPECT_EQ(beforeSteps[1], 1u);
    EXPECT_EQ(afterSteps.size(), 1u);
    EXPECT_EQ(afterSteps[0], 0u);
    EXPECT_EQ(errorSteps.size(), 1u);
    EXPECT_EQ(errorSteps[0], 1u);
}

// ---------------------------------------------------------------------------
// registerFunctions (span overload)
// ---------------------------------------------------------------------------

TEST(FunctionSequenceTest2, RegisterFunctionsSpan) {
    meta::FunctionSequence seq;
    std::vector<meta::FunctionSequence::FunctionType> funcs = {
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) + 1;
        },
        [](std::span<const std::any> args) -> std::any {
            return std::any_cast<int>(args[0]) + 2;
        }};
    auto ids = seq.registerFunctions(std::span(funcs));
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], 0u);
    EXPECT_EQ(ids[1], 1u);
    EXPECT_EQ(seq.functionCount(), 2u);
}

}  // namespace atom::test
