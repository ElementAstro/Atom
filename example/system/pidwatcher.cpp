#include "atom/system/pidwatcher.hpp"

#include <iostream>
#include <thread>

using namespace atom::system;

int main() {
    // Create a PidWatcher object
    PidWatcher watcher;

    // Set the callback function to be executed on process exit
    watcher.setExitCallback(
        []() { std::cout << "Process has exited." << std::endl; });

    // Set the monitor function to be executed at specified intervals
    watcher.setMonitorFunction(
        []() { std::cout << "Monitoring process..." << std::endl; },
        std::chrono::milliseconds(1000));

    // Retrieve the PID of a process by its name
    pid_t pid = watcher.getPidByName("some_process_name");
    std::cout << "PID of the process: " << pid << std::endl;

    // Start monitoring the specified process by name
    bool started = watcher.start("some_process_name");
    std::cout << "Monitoring started: " << std::boolalpha << started
              << std::endl;

    // Simulate some work
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Switch the target process to monitor
    bool switched = watcher.Switch("another_process_name");
    std::cout << "Switched to another process: " << std::boolalpha << switched
              << std::endl;

    // Simulate some more work
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Stop monitoring the currently monitored process
    watcher.stop();
    std::cout << "Monitoring stopped." << std::endl;

    return 0;
}