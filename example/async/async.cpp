#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include <spdlog/spdlog.h>  // Use spdlog for logging
#include "atom/async/async.hpp"

using namespace atom::async;
using namespace std::chrono_literals;

// Helper function: Get current thread ID as string
std::string getThreadIdStr() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

// Simple task function: sleep and return a result
int simpleTask(int id, int sleepTime) {
    spdlog::info("Task #{} is starting and will sleep for {} milliseconds.", id,
                 sleepTime);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
    spdlog::info("Task #{} has completed execution.", id);
    return id * 100;
}

// Task that throws an exception
void errorTask() {
    spdlog::info("Starting a task that will intentionally throw an exception.");
    std::this_thread::sleep_for(100ms);
    spdlog::info("Throwing a test exception from errorTask.");
    throw std::runtime_error("This is a test exception");
}

// Example 1: Basic usage
void basicUsageExample() {
    spdlog::info(
        "===== Example 1: Demonstrating Basic AsyncWorker Usage =====");

    // Create AsyncWorker instance
    AsyncWorker<int> worker;

    // Set priority and CPU affinity
    worker.setPriority(AsyncWorker<int>::Priority::HIGH);
    worker.setPreferredCPU(0);  // Prefer running on the first CPU core

    // Start async task
    spdlog::info("Launching an asynchronous task using AsyncWorker.");
    worker.startAsync(static_cast<int (*)(int, int)>(simpleTask), 1, 500);

    // Check task status
    spdlog::info("Is the task currently active? {}",
                 worker.isActive() ? "yes" : "no");
    spdlog::info("Has the task completed? {}", worker.isDone() ? "yes" : "no");

    // Wait for task to complete and get result
    spdlog::info("Waiting for the asynchronous task to complete.");
    int result = worker.getResult();
    spdlog::info("The result returned by the task is: {}", result);

    // Check status again
    spdlog::info("Is the task currently active after completion? {}",
                 worker.isActive() ? "yes" : "no");
    spdlog::info("Has the task completed after result retrieval? {}",
                 worker.isDone() ? "yes" : "no");
}

// Example 2: Callbacks and timeouts
void callbackAndTimeoutExample() {
    spdlog::info(
        "===== Example 2: Using Callbacks and Timeouts with AsyncWorker =====");

    // Create AsyncWorker instance
    AsyncWorker<int> worker;

    // Set callback function
    worker.setCallback([](int result) {
        spdlog::info("Callback executed after task completion. Result: {}",
                     result);
    });

    // Set timeout
    worker.setTimeout(2s);

    // Start async task
    spdlog::info("Starting an asynchronous task that should complete quickly.");
    worker.startAsync(static_cast<int (*)(int, int)>(simpleTask), 2, 300);

    // Wait for task to complete (triggers callback)
    spdlog::info(
        "Waiting for the task to complete and callback to be triggered.");
    worker.waitForCompletion();
    spdlog::info("Task and callback execution have finished.");

    // Test with timeout
    AsyncWorker<int> slowWorker;
    slowWorker.setTimeout(1s);  // Set 1 second timeout

    spdlog::info("Starting a long-running task to test timeout functionality.");
    slowWorker.startAsync(static_cast<int (*)(int, int)>(simpleTask), 3,
                          2000);  // Task takes 2 seconds

    try {
        spdlog::info(
            "Waiting for the long-running task. Expecting a timeout "
            "exception.");
        slowWorker.waitForCompletion();  // This should timeout
        spdlog::info("This line should not be reached if timeout occurs.");
    } catch (const TimeoutException& e) {
        spdlog::warn("TimeoutException caught as expected: {}", e.what());
    }
}

// Example 3: Managing multiple tasks with AsyncWorkerManager
void managerExample() {
    spdlog::info(
        "===== Example 3: Managing Multiple Async Tasks with "
        "AsyncWorkerManager =====");

    // Create manager
    AsyncWorkerManager<int> manager;

    // Create multiple workers
    spdlog::info("Creating and starting multiple asynchronous tasks.");
    std::vector<std::shared_ptr<AsyncWorker<int>>> workers;

    // Add 3 tasks
    for (int i = 1; i <= 3; i++) {
        spdlog::info("Creating and launching task #{}.", i);
        auto worker = manager.createWorker(
            static_cast<int (*)(int, int)>(simpleTask), i, i * 200);
        workers.push_back(worker);
    }

    // Check manager status
    spdlog::info("Current number of tasks managed: {}", manager.size());
    spdlog::info("Are all tasks completed? {}",
                 manager.allDone() ? "yes" : "no");

    // Wait for all tasks to complete
    spdlog::info("Waiting for all managed tasks to complete.");
    manager.waitForAll();

    // Check status after completion
    spdlog::info("All tasks have completed: {}",
                 manager.allDone() ? "yes" : "no");

    // Get all results
    spdlog::info("Retrieving results from all completed tasks:");
    for (size_t i = 0; i < workers.size(); i++) {
        int result = workers[i]->getResult();
        spdlog::info("Result from task #{}: {}", i + 1, result);
    }

    // Clean up completed tasks
    size_t removed = manager.pruneCompletedWorkers();
    spdlog::info("Removed {} completed tasks from the manager.", removed);
    spdlog::info("Number of remaining tasks in manager: {}", manager.size());
}

// Example 4: Task cancellation
void cancellationExample() {
    spdlog::info("===== Example 4: Demonstrating Task Cancellation =====");

    // Create manager
    AsyncWorkerManager<int> manager;

    // Create a long-running task
    spdlog::info(
        "Creating a long-running task for cancellation demonstration.");
    auto longTask = manager.createWorker([] {
        spdlog::info("Long-running task has started.");
        for (int i = 0; i < 5; i++) {
            spdlog::info("Long-running task progress: step {}/5.", i + 1);
            std::this_thread::sleep_for(500ms);
        }
        spdlog::info("Long-running task has completed.");
        return 9999;
    });

    // Wait for task to start
    std::this_thread::sleep_for(700ms);

    // Cancel single task
    spdlog::info("Cancelling the long-running task.");
    manager.cancel(longTask);

    // Check task status
    spdlog::info("Is the long-running task still active? {}",
                 longTask->isActive() ? "yes" : "no");
    spdlog::info("Has the long-running task completed? {}",
                 longTask->isDone() ? "yes" : "no");

    // Create multiple tasks and then cancel all
    spdlog::info("Creating multiple new tasks for bulk cancellation.");
    for (int i = 1; i <= 3; i++) {
        auto worker =
            manager.createWorker(static_cast<int (*)(int, int)>(simpleTask), i,
                                 2000);  // Each task runs for 2 seconds
    }

    spdlog::info("Total number of tasks in manager: {}", manager.size());

    // Wait for tasks to start
    std::this_thread::sleep_for(300ms);

    // Cancel all tasks
    spdlog::info("Cancelling all tasks managed by AsyncWorkerManager.");
    manager.cancelAll();

    spdlog::info("All tasks have completed after cancellation: {}",
                 manager.allDone() ? "yes" : "no");
}

// Example 5: Exception handling
void exceptionHandlingExample() {
    spdlog::info("===== Example 5: Exception Handling in AsyncWorker =====");

    // Exception - getting result from uninitialized worker
    AsyncWorker<int> uninitialized;
    try {
        spdlog::info(
            "Attempting to retrieve result from an uninitialized AsyncWorker.");
        int result = uninitialized.getResult();
        spdlog::info(
            "This line should not be executed if exception is thrown.");
    } catch (const std::exception& e) {
        spdlog::warn("Expected exception caught: {}", e.what());
    }

    // Exception - task throws internally
    AsyncWorker<void> errorWorker;
    errorWorker.startAsync(static_cast<void (*)()>(errorTask));

    try {
        spdlog::info(
            "Waiting for a task that will throw an exception internally.");
        errorWorker.waitForCompletion();
        spdlog::info(
            "This line should not be executed if exception is thrown.");
    } catch (const std::exception& e) {
        spdlog::warn("Exception caught from task: {}", e.what());
    }

    // Exception - setting null callback
    AsyncWorker<int> callbackWorker;
    try {
        spdlog::info("Attempting to set a null callback function.");
        callbackWorker.setCallback(nullptr);
        spdlog::info(
            "This line should not be executed if exception is thrown.");
    } catch (const std::exception& e) {
        spdlog::warn("Expected exception caught when setting null callback: {}",
                     e.what());
    }

    // Exception - setting negative timeout
    AsyncWorker<int> timeoutWorker;
    try {
        spdlog::info("Attempting to set a negative timeout value.");
        timeoutWorker.setTimeout(-1s);
        spdlog::info(
            "This line should not be executed if exception is thrown.");
    } catch (const std::exception& e) {
        spdlog::warn(
            "Expected exception caught when setting negative timeout: {}",
            e.what());
    }
}

// Example 6: Task validation
void taskValidationExample() {
    spdlog::info("===== Example 6: Validating Task Results =====");

    // Create task
    AsyncWorker<int> worker;
    worker.startAsync(static_cast<int (*)(int, int)>(simpleTask), 6, 300);

    // Wait for task to complete
    spdlog::info("Waiting for the task to complete before validation.");
    worker.waitForCompletion();

    // Validate result with validator
    bool isValid = worker.validate([](int result) {
        spdlog::info("Validating task result: {}", result);
        return result == 600;  // Should be 6 * 100 = 600
    });

    spdlog::info("Validation result: Is the task result valid? {}",
                 isValid ? "yes" : "no");

    // Use validator that doesn't meet conditions
    bool isInvalid = worker.validate([](int result) {
        spdlog::info("Validating task result with a failing condition: {}",
                     result);
        return result > 1000;  // 600 should not be greater than 1000
    });

    spdlog::info("Validation result with failing condition: {}",
                 isInvalid ? "yes" : "no");
}

// Example 7: asyncRetry usage
void asyncRetryExample() {
    spdlog::info("===== Example 7: Demonstrating asyncRetry Mechanism =====");

    // Create a function that fails the first few times
    int attemptsNeeded = 3;
    int currentAttempt = 0;

    auto flakeyFunction = [&]() -> std::string {
        currentAttempt++;
        spdlog::info(
            "Attempting to execute an unstable function. Current attempt: {}",
            currentAttempt);

        if (currentAttempt < attemptsNeeded) {
            spdlog::warn("Function failed on attempt {}. Will retry.",
                         currentAttempt);
            throw std::runtime_error("Deliberate failure, attempt #" +
                                     std::to_string(currentAttempt));
        }

        spdlog::info("Function executed successfully on attempt {}.",
                     currentAttempt);
        return "Successful result on attempt " + std::to_string(currentAttempt);
    };

    try {
        // Create retry logic
        spdlog::info(
            "Starting asynchronous operation with retry (fixed interval "
            "strategy).");
        auto future = asyncRetry(
            flakeyFunction,                  // Function to execute
            5,                               // Maximum number of attempts
            200ms,                           // Initial delay
            BackoffStrategy::FIXED,          // Use fixed interval
            1s,                              // Maximum total delay
            [](const std::string& result) {  // Success callback
                spdlog::info("Success callback executed. Result: {}", result);
            },
            [](const std::exception& e) {  // Exception callback
                spdlog::warn("Exception occurred during retry: {}", e.what());
            },
            []() {  // Completion callback
                spdlog::info("Operation completed callback executed.");
            });

        // Wait for result
        spdlog::info("Waiting for the result of the retry operation.");
        std::string result = future.get();
        spdlog::info("Final result from asyncRetry: {}", result);

    } catch (const std::exception& e) {
        spdlog::error("The retry operation ultimately failed: {}", e.what());
    }

    // Reset counter and try with exponential backoff strategy
    currentAttempt = 0;
    attemptsNeeded = 4;

    try {
        spdlog::info(
            "Starting asynchronous operation with retry (exponential backoff "
            "strategy).");
        auto future = asyncRetry(
            flakeyFunction,                  // Function to execute
            5,                               // Maximum number of attempts
            100ms,                           // Initial delay
            BackoffStrategy::EXPONENTIAL,    // Use exponential backoff
            10s,                             // Maximum total delay
            [](const std::string& result) {  // Success callback
                spdlog::info("Success callback executed. Result: {}", result);
            },
            [](const std::exception& e) {  // Exception callback
                spdlog::warn("Exception occurred during retry: {}", e.what());
            },
            []() {  // Completion callback
                spdlog::info("Operation completed callback executed.");
            });

        // Wait for result
        spdlog::info("Waiting for the result of the retry operation.");
        std::string result = future.get();
        spdlog::info("Final result from asyncRetry: {}", result);

    } catch (const std::exception& e) {
        spdlog::error("The retry operation ultimately failed: {}", e.what());
    }
}

// Example 8: Task coroutine usage (C++20 feature)
Task<int> exampleCoroutine(int value) {
    spdlog::info("Coroutine has started with initial value: {}", value);

    // Simulate async operation
    std::this_thread::sleep_for(500ms);
    value += 100;
    spdlog::info("Coroutine intermediate value after addition: {}", value);

    // Simulate another async operation
    std::this_thread::sleep_for(500ms);
    value *= 2;
    spdlog::info("Coroutine final value after multiplication: {}", value);

    co_return value;
}

void coroutineExample() {
    spdlog::info(
        "===== Example 8: Demonstrating Coroutine Usage with Task =====");

    try {
        spdlog::info("Starting coroutine task with Task<int>.");
        auto task = exampleCoroutine(42);

        spdlog::info("Coroutine started. Awaiting result.");
        int result = task.await_result();
        spdlog::info("Coroutine completed successfully. Result: {}", result);

    } catch (const std::exception& e) {
        spdlog::error("Coroutine execution failed with exception: {}",
                      e.what());
    }

    // Error handling coroutine example
    auto errorCoroutine = []() -> Task<int> {
        spdlog::info(
            "Starting coroutine that will intentionally throw an exception.");
        std::this_thread::sleep_for(300ms);
        spdlog::info("Coroutine is about to throw an exception.");
        throw std::runtime_error("Test exception in coroutine");
        co_return 0;  // Will never reach here
    };

    try {
        spdlog::info("Starting coroutine expected to fail with an exception.");
        auto task = errorCoroutine();

        spdlog::info("Awaiting result from coroutine that should fail.");
        task.await_result();  // Use the return value to fix the 'unused
                              // variable' warning
        spdlog::info(
            "This line should not be executed if exception is thrown.");
    } catch (const std::exception& e) {
        spdlog::warn("Caught exception from coroutine: {}", e.what());
    }
}

// Main function
int main(int argc, char* argv[]) {
    // Initialize spdlog (no explicit init needed for basic usage)

    spdlog::info("=============================================");
    spdlog::info("     AsyncWorker and AsyncWorkerManager Examples     ");
    spdlog::info("=============================================");

    try {
        // Run all examples
        basicUsageExample();
        callbackAndTimeoutExample();
        managerExample();
        cancellationExample();
        exceptionHandlingExample();
        taskValidationExample();
        asyncRetryExample();
        coroutineExample();

        spdlog::info("All example demonstrations have completed successfully.");
    } catch (const std::exception& e) {
        spdlog::error("An unhandled exception was caught in main: {}",
                      e.what());
        return 1;
    }

    return 0;
}
