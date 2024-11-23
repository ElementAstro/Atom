#include "atom/async/packaged_task.hpp"

#include <iostream>
#include <thread>

using namespace atom::async;

// Example function to be executed as a task
int exampleFunction(int a, int b) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return a + b;
}

// Example callback function
void exampleCallback(int result) {
    std::cout << "Callback: Result is " << result << std::endl;
}

// Example void function to be executed as a task
void exampleVoidFunction() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Void function executed" << std::endl;
}

// Example void callback function
void exampleVoidCallback() {
    std::cout << "Void callback executed" << std::endl;
}

int main() {
    // Create an EnhancedPackagedTask for a function with a result
    EnhancedPackagedTask<int, int, int> task(exampleFunction);

    // Get the EnhancedFuture associated with the task
    auto future = task.getEnhancedFuture();

    // Add a callback to be called upon task completion
    task.onComplete(exampleCallback);

    // Execute the task with arguments
    std::thread taskThread([&task]() { task(5, 10); });

    // Wait for the task to complete and get the result
    int result = future.get();
    std::cout << "Task result: " << result << std::endl;

    // Check if the task is cancelled
    if (task.isCancelled()) {
        std::cout << "Task was cancelled" << std::endl;
    }

    // Join the task thread
    taskThread.join();

    // Create an EnhancedPackagedTask for a void function
    EnhancedPackagedTask<void> voidTask(exampleVoidFunction);

    // Get the EnhancedFuture associated with the void task
    auto voidFuture = voidTask.getEnhancedFuture();

    // Add a callback to be called upon void task completion
    voidTask.onComplete(exampleVoidCallback);

    // Execute the void task
    std::thread voidTaskThread([&voidTask]() { voidTask(); });

    // Wait for the void task to complete
    voidFuture.get();

    // Check if the void task is cancelled
    if (voidTask.isCancelled()) {
        std::cout << "Void task was cancelled" << std::endl;
    }

    // Join the void task thread
    voidTaskThread.join();

    return 0;
}