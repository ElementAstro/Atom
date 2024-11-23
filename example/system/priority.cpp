#include "atom/system/priority.hpp"

#include <iostream>
#include <thread>

using namespace atom::system;

void priorityChangeCallback(PriorityManager::PriorityLevel level) {
    std::cout << "Priority changed to: " << static_cast<int>(level)
              << std::endl;
}

int main() {
    // Set the priority of the current process to HIGH
    PriorityManager::setProcessPriority(
        PriorityManager::PriorityLevel::HIGHEST);
    std::cout << "Set process priority to HIGHEST" << std::endl;

    // Get the priority of the current process
    auto processPriority = PriorityManager::getProcessPriority();
    std::cout << "Current process priority: "
              << static_cast<int>(processPriority) << std::endl;

    // Set the priority of the current thread to ABOVE_NORMAL
    PriorityManager::setThreadPriority(
        PriorityManager::PriorityLevel::ABOVE_NORMAL);
    std::cout << "Set thread priority to ABOVE_NORMAL" << std::endl;

    // Get the priority of the current thread
    auto threadPriority = PriorityManager::getThreadPriority();
    std::cout << "Current thread priority: " << static_cast<int>(threadPriority)
              << std::endl;

    // Set the scheduling policy of the current thread to FIFO
    PriorityManager::setThreadSchedulingPolicy(
        PriorityManager::SchedulingPolicy::FIFO);
    std::cout << "Set thread scheduling policy to FIFO" << std::endl;

    // Set the CPU affinity of the current process to CPUs 0 and 1
    PriorityManager::setProcessAffinity({0, 1});
    std::cout << "Set process affinity to CPUs 0 and 1" << std::endl;

    // Get the CPU affinity of the current process
    auto affinity = PriorityManager::getProcessAffinity();
    std::cout << "Current process affinity: ";
    for (int cpu : affinity) {
        std::cout << cpu << " ";
    }
    std::cout << std::endl;

    // Start monitoring the priority of the current process
    PriorityManager::startPriorityMonitor(0, priorityChangeCallback);
    std::cout << "Started priority monitor for the current process"
              << std::endl;

    // Simulate some work
    std::this_thread::sleep_for(std::chrono::seconds(5));

    return 0;
}