#include "atom/async/timer.hpp"

#include <iostream>
#include <thread>

using namespace atom::async;

// Example function to be executed by the timer
void exampleFunction() {
    std::cout << "Task executed at "
              << std::chrono::steady_clock::now().time_since_epoch().count()
              << std::endl;
}

// Example function with arguments
void exampleFunctionWithArgs(int value) {
    std::cout << "Task executed with value: " << value << " at "
              << std::chrono::steady_clock::now().time_since_epoch().count()
              << std::endl;
}

int main() {
    // Create a Timer object
    Timer timer;

    // Schedule a task to be executed once after a delay
    auto future = timer.setTimeout(exampleFunction, 1000);
    future.get();  // Wait for the task to complete

    // Schedule a task to be executed repeatedly at an interval
    timer.setInterval(exampleFunctionWithArgs, 500, 5, 1, 42);

    // Wait for a short duration to let some tasks execute
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Get the current time
    auto currentTime = timer.now();
    std::cout << "Current time: " << currentTime.time_since_epoch().count()
              << std::endl;

    // Get the number of scheduled tasks
    size_t taskCount = timer.getTaskCount();
    std::cout << "Number of scheduled tasks: " << taskCount << std::endl;

    // Pause the timer
    timer.pause();
    std::cout << "Timer paused" << std::endl;

    // Wait for a short duration
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Resume the timer
    timer.resume();
    std::cout << "Timer resumed" << std::endl;

    // Set a callback function to be called when a task is executed
    timer.setCallback(
        []() { std::cout << "Callback: Task executed" << std::endl; });

    // Wait for all tasks to complete
    timer.wait();

    // Cancel all scheduled tasks
    timer.cancelAllTasks();
    std::cout << "All tasks cancelled" << std::endl;

    // Stop the timer
    timer.stop();
    std::cout << "Timer stopped" << std::endl;

    return 0;
}