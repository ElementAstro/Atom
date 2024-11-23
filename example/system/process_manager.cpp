#include "atom/system/process_manager.hpp"

#include <iostream>

using namespace atom::system;

int main() {
    // Create a ProcessManager object
    ProcessManager processManager;

    // Create a new process
    try {
        bool created = processManager.createProcess("echo Hello, World!",
                                                    "test_process", true);
        std::cout << "Process created: " << std::boolalpha << created
                  << std::endl;
    } catch (const ProcessException& e) {
        std::cerr << "Error creating process: " << e.what() << std::endl;
    }

    // Terminate a process by its PID
    try {
        bool terminated = processManager.terminateProcess(12345);
        std::cout << "Process terminated by PID: " << std::boolalpha
                  << terminated << std::endl;
    } catch (const ProcessException& e) {
        std::cerr << "Error terminating process by PID: " << e.what()
                  << std::endl;
    }

    // Terminate a process by its name
    try {
        bool terminated = processManager.terminateProcessByName("test_process");
        std::cout << "Process terminated by name: " << std::boolalpha
                  << terminated << std::endl;
    } catch (const ProcessException& e) {
        std::cerr << "Error terminating process by name: " << e.what()
                  << std::endl;
    }

    // Check if a process with the given identifier exists
    bool hasProcess = processManager.hasProcess("test_process");
    std::cout << "Process exists: " << std::boolalpha << hasProcess
              << std::endl;

    // Get a list of running processes
    auto runningProcesses = processManager.getRunningProcesses();
    std::cout << "Running processes:" << std::endl;
    for (const auto& process : runningProcesses) {
        std::cout << "PID: " << process.pid << ", Name: " << process.name
                  << std::endl;
    }

    // Get the output of a process by its identifier
    auto output = processManager.getProcessOutput("test_process");
    std::cout << "Process output:" << std::endl;
    for (const auto& line : output) {
        std::cout << line << std::endl;
    }

    // Wait for all managed processes to complete
    processManager.waitForCompletion();
    std::cout << "All processes have completed." << std::endl;

    // Run a script as a new process
    try {
        bool scriptRun = processManager.runScript("echo Running script",
                                                  "script_process", true);
        std::cout << "Script run: " << std::boolalpha << scriptRun << std::endl;
    } catch (const ProcessException& e) {
        std::cerr << "Error running script: " << e.what() << std::endl;
    }

    // Monitor the managed processes and update their statuses
    bool monitoring = processManager.monitorProcesses();
    std::cout << "Monitoring processes: " << std::boolalpha << monitoring
              << std::endl;

    // Retrieve detailed information about a specific process
    try {
        auto processInfo = processManager.getProcessInfo(12345);
        std::cout << "Process info - PID: " << processInfo.pid
                  << ", Name: " << processInfo.name << std::endl;
    } catch (const ProcessException& e) {
        std::cerr << "Error retrieving process info: " << e.what() << std::endl;
    }

#ifdef _WIN32
    // Get the handle of a process by its PID (Windows only)
    try {
        void* handle = processManager.getProcessHandle(12345);
        std::cout << "Process handle: " << handle << std::endl;
    } catch (const ProcessException& e) {
        std::cerr << "Error getting process handle: " << e.what() << std::endl;
    }
#else
    // Get the file path of a process by its PID (non-Windows)
    try {
        std::string filePath = ProcessManager::getProcFilePath(12345, "exe");
        std::cout << "Process file path: " << filePath << std::endl;
    } catch (const ProcessException& e) {
        std::cerr << "Error getting process file path: " << e.what()
                  << std::endl;
    }
#endif

    return 0;
}