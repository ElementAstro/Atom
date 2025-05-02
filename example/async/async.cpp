#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "atom/async/async.hpp"
#include "loguru.hpp"  // Include loguru header

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
    LOG_F(INFO, "Starting task #{}, sleeping for {}ms", id, sleepTime);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
    LOG_F(INFO, "Completed task #{}", id);
    return id * 100;
}

// Task that throws an exception
void errorTask() {
    LOG_F(INFO, "Starting task that will fail");
    std::this_thread::sleep_for(100ms);
    LOG_F(INFO, "Throwing exception");
    throw std::runtime_error("This is a test exception");
}

// Example 1: Basic usage
void basicUsageExample() {
    LOG_F(INFO, "\n===== Example 1: Basic Usage =====");

    // Create AsyncWorker instance
    AsyncWorker<int> worker;

    // Set priority and CPU affinity
    worker.setPriority(AsyncWorker<int>::Priority::HIGH);
    worker.setPreferredCPU(0);  // Prefer running on the first CPU core

    // Start async task
    LOG_F(INFO, "Starting async task");
    worker.startAsync(simpleTask, 1, 500);

    // Check task status
    LOG_F(INFO, "Task is active: %s", worker.isActive() ? "yes" : "no");
    LOG_F(INFO, "Task is done: %s", worker.isDone() ? "yes" : "no");

    // Wait for task to complete and get result
    LOG_F(INFO, "Waiting for task to complete");
    int result = worker.getResult();
    LOG_F(INFO, "Task result: {}", result);

    // Check status again
    LOG_F(INFO, "Task is active: %s", worker.isActive() ? "yes" : "no");
    LOG_F(INFO, "Task is done: %s", worker.isDone() ? "yes" : "no");
}

// Example 2: Callbacks and timeouts
void callbackAndTimeoutExample() {
    LOG_F(INFO, "\n===== Example 2: Callbacks and Timeouts =====");

    // Create AsyncWorker instance
    AsyncWorker<int> worker;

    // Set callback function
    worker.setCallback(
        [](int result) { LOG_F(INFO, "Callback called, result: {}", result); });

    // Set timeout
    worker.setTimeout(2s);

    // Start async task
    LOG_F(INFO, "Starting async task (fast task)");
    worker.startAsync(simpleTask, 2, 300);

    // Wait for task to complete (triggers callback)
    LOG_F(INFO, "Waiting for task to complete (with callback)");
    worker.waitForCompletion();
    LOG_F(INFO, "Task and callback completed");

    // Test with timeout
    AsyncWorker<int> slowWorker;
    slowWorker.setTimeout(1s);  // Set 1 second timeout

    LOG_F(INFO, "Starting long-running task (timeout test)");
    slowWorker.startAsync(simpleTask, 3, 2000);  // Task takes 2 seconds

    try {
        LOG_F(INFO, "Waiting for task, should timeout");
        slowWorker.waitForCompletion();  // This should timeout
        LOG_F(INFO, "This line should not be executed");
    } catch (const TimeoutException& e) {
        LOG_F(INFO, "Caught expected timeout exception: %s", e.what());
    }
}

// Example 3: Managing multiple tasks with AsyncWorkerManager
void managerExample() {
    LOG_F(INFO,
          "\n===== Example 3: AsyncWorkerManager Multi-task Management =====");

    // Create manager
    AsyncWorkerManager<int> manager;

    // Create multiple workers
    LOG_F(INFO, "Creating and starting multiple async tasks");
    std::vector<std::shared_ptr<AsyncWorker<int>>> workers;

    // Add 3 tasks
    for (int i = 1; i <= 3; i++) {
        LOG_F(INFO, "Creating task #{}", i);
        auto worker = manager.createWorker(simpleTask, i, i * 200);
        workers.push_back(worker);
    }

    // Check manager status
    LOG_F(INFO, "Number of tasks in manager: %zu", manager.size());
    LOG_F(INFO, "All tasks completed: %s", manager.allDone() ? "yes" : "no");

    // Wait for all tasks to complete
    LOG_F(INFO, "Waiting for all tasks to complete");
    manager.waitForAll();

    // Check status after completion
    LOG_F(INFO, "All tasks completed: %s", manager.allDone() ? "yes" : "no");

    // Get all results
    LOG_F(INFO, "Getting all task results:");
    for (size_t i = 0; i < workers.size(); i++) {
        int result = workers[i]->getResult();
        LOG_F(INFO, "Task #%zu result: {}", i + 1, result);
    }

    // Clean up completed tasks
    size_t removed = manager.pruneCompletedWorkers();
    LOG_F(INFO, "Removed %zu completed tasks", removed);
    LOG_F(INFO, "Remaining tasks in manager: %zu", manager.size());
}

// Example 4: Task cancellation
void cancellationExample() {
    LOG_F(INFO, "\n===== Example 4: Task Cancellation =====");

    // Create manager
    AsyncWorkerManager<int> manager;

    // Create a long-running task
    LOG_F(INFO, "Creating long-running task");
    auto longTask = manager.createWorker([] {
        LOG_F(INFO, "Starting long task");
        for (int i = 0; i < 5; i++) {
            LOG_F(INFO, "Long task step {}/5", i + 1);
            std::this_thread::sleep_for(500ms);
        }
        LOG_F(INFO, "Long task completed");
        return 9999;
    });

    // Wait for task to start
    std::this_thread::sleep_for(700ms);

    // Cancel single task
    LOG_F(INFO, "Cancelling long task");
    manager.cancel(longTask);

    // Check task status
    LOG_F(INFO, "Task is active: %s", longTask->isActive() ? "yes" : "no");
    LOG_F(INFO, "Task is done: %s", longTask->isDone() ? "yes" : "no");

    // Create multiple tasks and then cancel all
    LOG_F(INFO, "Creating multiple new tasks");
    for (int i = 1; i <= 3; i++) {
        auto worker = manager.createWorker(
            simpleTask, i, 2000);  // Each task runs for 2 seconds
    }

    LOG_F(INFO, "Number of tasks in manager: %zu", manager.size());

    // Wait for tasks to start
    std::this_thread::sleep_for(300ms);

    // Cancel all tasks
    LOG_F(INFO, "Cancelling all tasks");
    manager.cancelAll();

    LOG_F(INFO, "All tasks completed: %s", manager.allDone() ? "yes" : "no");
}

// Example 5: Exception handling
void exceptionHandlingExample() {
    LOG_F(INFO, "\n===== Example 5: Exception Handling =====");

    // Exception - getting result from uninitialized worker
    AsyncWorker<int> uninitialized;
    try {
        LOG_F(INFO, "Attempting to get result from uninitialized worker");
        int result = uninitialized.getResult();
        LOG_F(INFO, "This line should not be executed");
    } catch (const std::exception& e) {
        LOG_F(INFO, "Expected exception: %s", e.what());
    }

    // Exception - task throws internally
    AsyncWorker<void> errorWorker;
    errorWorker.startAsync(errorTask);

    try {
        LOG_F(INFO, "Waiting for task that will throw an exception");
        errorWorker.waitForCompletion();
        LOG_F(INFO, "This line should not be executed");
    } catch (const std::exception& e) {
        LOG_F(INFO, "Caught task exception: %s", e.what());
    }

    // Exception - setting null callback
    AsyncWorker<int> callbackWorker;
    try {
        LOG_F(INFO, "Attempting to set null callback function");
        callbackWorker.setCallback(nullptr);
        LOG_F(INFO, "This line should not be executed");
    } catch (const std::exception& e) {
        LOG_F(INFO, "Expected exception: %s", e.what());
    }

    // Exception - setting negative timeout
    AsyncWorker<int> timeoutWorker;
    try {
        LOG_F(INFO, "Attempting to set negative timeout value");
        timeoutWorker.setTimeout(-1s);
        LOG_F(INFO, "This line should not be executed");
    } catch (const std::exception& e) {
        LOG_F(INFO, "Expected exception: %s", e.what());
    }
}

// Example 6: Task validation
void taskValidationExample() {
    LOG_F(INFO, "\n===== Example 6: Task Validation =====");

    // Create task
    AsyncWorker<int> worker;
    worker.startAsync(simpleTask, 6, 300);

    // Wait for task to complete
    LOG_F(INFO, "Waiting for task to complete");
    worker.waitForCompletion();

    // Validate result with validator
    bool isValid = worker.validate([](int result) {
        LOG_F(INFO, "Validating result: {}", result);
        return result == 600;  // Should be 6 * 100 = 600
    });

    LOG_F(INFO, "Validation result is valid: %s", isValid ? "yes" : "no");

    // Use validator that doesn't meet conditions
    bool isInvalid = worker.validate([](int result) {
        LOG_F(INFO, "Validating result: {}", result);
        return result > 1000;  // 600 should not be greater than 1000
    });

    LOG_F(INFO, "Failed condition validation result: %s",
          isInvalid ? "yes" : "no");
}

// Example 7: asyncRetry usage
void asyncRetryExample() {
    LOG_F(INFO, "\n===== Example 7: asyncRetry Retry Mechanism =====");

    // Create a function that fails the first few times
    int attemptsNeeded = 3;
    int currentAttempt = 0;

    auto flakeyFunction = [&]() -> std::string {
        currentAttempt++;
        LOG_F(INFO,
              "Attempting to execute unstable function, current attempt: {}",
              currentAttempt);

        if (currentAttempt < attemptsNeeded) {
            LOG_F(INFO, "Function failed, will retry");
            throw std::runtime_error("Deliberate failure, attempt #" +
                                     std::to_string(currentAttempt));
        }

        LOG_F(INFO, "Function executed successfully");
        return "Successful result on attempt " + std::to_string(currentAttempt);
    };

    try {
        // Create retry logic
        LOG_F(INFO, "Starting async operation with retry (fixed interval)");
        auto future = asyncRetry(
            flakeyFunction,                  // Function to execute
            5,                               // Maximum number of attempts
            200ms,                           // Initial delay
            BackoffStrategy::FIXED,          // Use fixed interval
            1s,                              // Maximum total delay
            [](const std::string& result) {  // Success callback
                LOG_F(INFO, "Success callback: %s", result.c_str());
            },
            [](const std::exception& e) {  // Exception callback
                LOG_F(INFO, "Exception occurred: %s", e.what());
            },
            []() {  // Completion callback
                LOG_F(INFO, "Operation completed callback");
            });

        // Wait for result
        LOG_F(INFO, "Waiting for retry operation result");
        std::string result = future.get();
        LOG_F(INFO, "Final result: %s", result.c_str());

    } catch (const std::exception& e) {
        LOG_F(INFO, "Operation ultimately failed: %s", e.what());
    }

    // Reset counter and try with exponential backoff strategy
    currentAttempt = 0;
    attemptsNeeded = 4;

    try {
        LOG_F(INFO,
              "\nStarting async operation with retry (exponential backoff)");
        auto future = asyncRetry(
            flakeyFunction,                  // Function to execute
            5,                               // Maximum number of attempts
            100ms,                           // Initial delay
            BackoffStrategy::EXPONENTIAL,    // Use exponential backoff
            10s,                             // Maximum total delay
            [](const std::string& result) {  // Success callback
                LOG_F(INFO, "Success callback: %s", result.c_str());
            },
            [](const std::exception& e) {  // Exception callback
                LOG_F(INFO, "Exception occurred: %s", e.what());
            },
            []() {  // Completion callback
                LOG_F(INFO, "Operation completed callback");
            });

        // Wait for result
        LOG_F(INFO, "Waiting for retry operation result");
        std::string result = future.get();
        LOG_F(INFO, "Final result: %s", result.c_str());

    } catch (const std::exception& e) {
        LOG_F(INFO, "Operation ultimately failed: %s", e.what());
    }
}

// Example 8: Task coroutine usage (C++20 feature)
Task<int> exampleCoroutine(int value) {
    LOG_F(INFO, "Coroutine started, initial value: {}", value);

    // Simulate async operation
    std::this_thread::sleep_for(500ms);
    value += 100;
    LOG_F(INFO, "Coroutine intermediate value: {}", value);

    // Simulate another async operation
    std::this_thread::sleep_for(500ms);
    value *= 2;
    LOG_F(INFO, "Coroutine final value: {}", value);

    co_return value;
}

void coroutineExample() {
    LOG_F(INFO, "\n===== Example 8: Task Coroutine Usage =====");

    try {
        LOG_F(INFO, "Starting coroutine task");
        auto task = exampleCoroutine(42);

        LOG_F(INFO, "Coroutine started, waiting for result");
        int result = task.await_result();
        LOG_F(INFO, "Coroutine result: {}", result);

    } catch (const std::exception& e) {
        LOG_F(INFO, "Coroutine execution failed: %s", e.what());
    }

    // Error handling coroutine example
    auto errorCoroutine = []() -> Task<int> {
        LOG_F(INFO, "Starting coroutine that will fail");
        std::this_thread::sleep_for(300ms);
        LOG_F(INFO, "Coroutine throwing exception");
        throw std::runtime_error("Test exception in coroutine");
        co_return 0;  // Will never reach here
    };

    try {
        LOG_F(INFO, "Starting coroutine that will fail");
        auto task = errorCoroutine();

        LOG_F(INFO, "Waiting for coroutine result (expected to fail)");
        task.await_result();  // Use the return value to fix the 'unused
                              // variable' warning
        LOG_F(INFO, "This line should not be executed");
    } catch (const std::exception& e) {
        LOG_F(INFO, "Caught coroutine exception: %s", e.what());
    }
}

// Main function
int main(int argc, char* argv[]) {
    // Initialize loguru
    loguru::init(argc, argv);

    LOG_F(INFO, "=============================================");
    LOG_F(INFO, "     AsyncWorker/AsyncWorkerManager Examples     ");
    LOG_F(INFO, "=============================================");

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

        LOG_F(INFO, "\nAll examples completed successfully!");
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Caught unhandled exception: %s", e.what());
        return 1;
    }

    return 0;
}