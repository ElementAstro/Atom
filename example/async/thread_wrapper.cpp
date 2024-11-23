#include "atom/async/thread_wrapper.hpp"

#include <chrono>
#include <iostream>
#include <thread>

using namespace atom::async;

// Example function to be executed by the thread
void exampleFunction() {
    std::cout << "Thread is running" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Thread has finished" << std::endl;
}

// Example function with stop token
void exampleFunctionWithStop(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        std::cout << "Thread is running with stop token" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "Thread has been requested to stop" << std::endl;
}

int main() {
    // Create a Thread object
    Thread thread;

    // Start the thread with a simple function
    thread.start(exampleFunction);

    // Wait for the thread to finish
    thread.join();

    // Start the thread with a function that uses a stop token
    thread.start(exampleFunctionWithStop);

    // Let the thread run for a while
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Request the thread to stop
    thread.requestStop();

    // Wait for the thread to finish
    thread.join();

    // Check if the thread is running
    if (thread.running()) {
        std::cout << "Thread is still running" << std::endl;
    } else {
        std::cout << "Thread is not running" << std::endl;
    }

    // Get the thread ID
    std::cout << "Thread ID: " << thread.getId() << std::endl;

    // Get the stop source and stop token
    auto stopSource = thread.getStopSource();
    auto stopToken = thread.getStopToken();

    // Swap threads
    Thread anotherThread;
    thread.swap(anotherThread);

    // Check the swapped thread ID
    std::cout << "Swapped Thread ID: " << anotherThread.getId() << std::endl;

    return 0;
}