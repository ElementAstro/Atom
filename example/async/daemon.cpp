#include "atom/async/daemon.hpp"

#include <iostream>
#include <thread>

using namespace atom::async;

// Example main callback function to be executed in the child process
int exampleMainCallback(int argc, char** argv) {
    std::cout << "Daemon process started with arguments: ";
    for (int i = 0; i < argc; ++i) {
        std::cout << argv[i] << " ";
    }
    std::cout << std::endl;

    // Simulate some work in the daemon process
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "Daemon process finished work." << std::endl;

    return 0;
}

int main(int argc, char** argv) {
    // Create a DaemonGuard object
    DaemonGuard daemonGuard;

    // Check if the process ID file exists
    if (checkPidFile()) {
        std::cerr << "Daemon is already running." << std::endl;
        return 1;
    }

    // Write the process ID to a file
    writePidFile();

    // Start the daemon process
    bool isDaemon = true;
    int result =
        daemonGuard.startDaemon(argc, argv, exampleMainCallback, isDaemon);

    // Print the process information
    std::cout << "Process information: " << daemonGuard.toString() << std::endl;

    return result;
}