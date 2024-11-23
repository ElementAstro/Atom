#include "atom/async/pool.hpp"

#include <iostream>
#include <thread>
#include <vector>

using namespace atom::async;

// Example function to be executed by the thread pool
void exampleFunction(int id) {
    std::cout << "Task " << id << " is running on thread "
              << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Task " << id << " is completed" << std::endl;
}

int main() {
    // Create a ThreadPool with the default number of threads
    ThreadPool<> threadPool;

    // Enqueue tasks into the thread pool
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(threadPool.enqueue(exampleFunction, i));
    }

    // Wait for all tasks to complete
    for (auto& future : futures) {
        future.get();
    }

    // Enqueue a task that does not return a future (detached task)
    threadPool.enqueueDetach([]() {
        std::cout << "Detached task is running on thread "
                  << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "Detached task is completed" << std::endl;
    });

    // Wait for all tasks to complete before destroying the thread pool
    threadPool.waitForTasks();

    return 0;
}