#include "atom/system/process.hpp"

#include <iostream>

using namespace atom::system;

int main() {
    // Get information about all processes
    auto allProcesses = getAllProcesses();
    std::cout << "All processes:" << std::endl;
    for (const auto& [pid, name] : allProcesses) {
        std::cout << "PID: " << pid << ", Name: " << name << std::endl;
    }

    // Get information about a process by its PID
    int pid = 12345;  // Example PID
    auto processInfo = getProcessInfoByPid(pid);
    std::cout << "Process info - PID: " << processInfo.pid
              << ", Name: " << processInfo.name << std::endl;

    // Get information about the current process
    auto selfProcessInfo = getSelfProcessInfo();
    std::cout << "Current process info - PID: " << selfProcessInfo.pid
              << ", Name: " << selfProcessInfo.name << std::endl;

    // Get the name of the controlling terminal
    std::string terminalName = ctermid();
    std::cout << "Controlling terminal: " << terminalName << std::endl;

    // Check if a process is running by its name
    std::string processName = "example_process";
    bool isRunning = isProcessRunning(processName);
    std::cout << "Is process running: " << std::boolalpha << isRunning
              << std::endl;

    // Get the parent process ID of a given process
    int parentPid = getParentProcessId(pid);
    std::cout << "Parent process ID: " << parentPid << std::endl;

    // Create a process as a specified user (Windows only)
#ifdef _WIN32
    bool created =
        createProcessAsUser("notepad.exe", "username", "domain", "password");
    std::cout << "Process created as user: " << std::boolalpha << created
              << std::endl;
#endif

    // Get the process IDs of processes with the specified name
    auto processIds = getProcessIdByName(processName);
    std::cout << "Process IDs for " << processName << ":" << std::endl;
    for (int id : processIds) {
        std::cout << id << std::endl;
    }

#ifdef _WIN32
    // Get Windows privileges of a process by its PID (Windows only)
    auto privileges = getWindowsPrivileges(pid);
    std::cout << "Privileges for PID " << pid << ":" << std::endl;
    for (const auto& privilege : privileges) {
        std::cout << privilege << std::endl;
    }
#endif

    return 0;
}