#include "atom/async/async.hpp"

#include <iostream>
#include <thread>

using namespace atom::async;

// Example function to be executed asynchronously
int exampleFunction(int a, int b) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return a + b;
}

// Example callback function
void exampleCallback(int result) {
    std::cout << "Callback: Result is " << result << std::endl;
}

// Example exception handler
void exampleExceptionHandler(const std::exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
}

// Example complete handler
void exampleCompleteHandler() {
    std::cout << "Complete: Task finished" << std::endl;
}

int main() {
    // Create an AsyncWorker for int result type
    AsyncWorker<int> worker;

    // Start the async task
    worker.startAsync(exampleFunction, 5, 10);

    // Set a callback function
    worker.setCallback(exampleCallback);

    // Set a timeout for the task
    worker.setTimeout(std::chrono::seconds(5));

    // Wait for the task to complete
    worker.waitForCompletion();

    // Get the result of the task
    if (worker.isDone()) {
        int result = worker.getResult();
        std::cout << "Result: " << result << std::endl;
    }

    // Create an AsyncWorkerManager for int result type
    AsyncWorkerManager<int> manager;

    // Create multiple workers and start tasks
    auto worker1 = manager.createWorker(exampleFunction, 1, 2);
    auto worker2 = manager.createWorker(exampleFunction, 3, 4);

    // Wait for all tasks to complete
    manager.waitForAll();

    // Check if all tasks are done
    if (manager.allDone()) {
        std::cout << "All tasks are done." << std::endl;
    }

    // Get results from workers
    if (worker1->isDone()) {
        std::cout << "Worker 1 result: " << worker1->getResult() << std::endl;
    }
    if (worker2->isDone()) {
        std::cout << "Worker 2 result: " << worker2->getResult() << std::endl;
    }

    // Cancel all tasks
    manager.cancelAll();

    // Example of asyncRetry usage
    auto retryFuture =
        asyncRetry(exampleFunction, 3, std::chrono::milliseconds(100),
                   BackoffStrategy::EXPONENTIAL,
                   std::chrono::milliseconds(1000), exampleCallback,
                   exampleExceptionHandler, exampleCompleteHandler, 5, 10);

    // Wait for the retry task to complete
    try {
        int retryResult = retryFuture.get();
        std::cout << "Retry result: " << retryResult << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Retry exception: " << e.what() << std::endl;
    }

    return 0;
}