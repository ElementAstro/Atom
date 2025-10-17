/**
 * @file process_basic.cpp
 * @brief Basic example demonstrating fundamental process operations
 *
 * This example provides a gentle introduction to process management
 * using the Atom System module. It covers:
 * - Basic process enumeration
 * - Simple process information retrieval
 * - Current process information
 * - Basic process searching
 *
 * @note This is a beginner-friendly example
 * @author Atom Framework
 * @date 2024
 */

#include <iomanip>
#include <iostream>
#include "atom/system/process.hpp"

using namespace atom::system;

int main() {
    try {
        std::cout << "=== Basic Process Operations Example ===" << std::endl;
        std::cout << "Learning fundamental process management\n" << std::endl;

        // 1. Get information about all processes
        std::cout << "[1. Process Enumeration]" << std::endl;
        auto allProcesses = getAllProcesses();
        std::cout << "Total processes running: " << allProcesses.size()
                  << std::endl;

        // Display first 5 processes as an example
        std::cout << "\nFirst 5 processes:" << std::endl;
        std::cout << std::setw(8) << "PID" << " | " << "Process Name"
                  << std::endl;
        std::cout << std::string(30, '-') << std::endl;

        int count = 0;
        for (const auto& [pid, name] : allProcesses) {
            if (count++ >= 5)
                break;
            std::cout << std::setw(8) << pid << " | " << name << std::endl;
        }

        // 2. Get information about the current process
        std::cout << "\n[2. Current Process Information]" << std::endl;
        auto selfProcess = getSelfProcessInfo();
        std::cout << "Current Process Details:" << std::endl;
        std::cout << "  PID: " << selfProcess.pid << std::endl;
        std::cout << "  Name: " << selfProcess.name << std::endl;
        std::cout << "  Command: " << selfProcess.command << std::endl;
        std::cout << "  Status: " << selfProcess.status << std::endl;
        std::cout << "  Username: " << selfProcess.username << std::endl;

        // 3. Check if a specific process is running
        std::cout << "\n[3. Process Search]" << std::endl;

        // Try to find a common system process
        std::vector<std::string> commonProcesses = {
#ifdef _WIN32
            "explorer.exe", "svchost.exe", "winlogon.exe"
#elif defined(__APPLE__)
            "launchd", "kernel_task", "Finder"
#else
            "systemd", "init", "bash"
#endif
        };

        bool foundProcess = false;
        for (const auto& processName : commonProcesses) {
            bool isRunning = isProcessRunning(processName);
            std::cout << "Process '" << processName << "' is "
                      << (isRunning ? "running" : "not running") << std::endl;

            if (isRunning && !foundProcess) {
                foundProcess = true;

                // Get process IDs for this process
                auto processIds = getProcessIdByName(processName);
                std::cout << "  Found " << processIds.size()
                          << " instance(s):" << std::endl;
                for (int pid : processIds) {
                    std::cout << "    PID: " << pid << std::endl;
                }

                // Get detailed information about the first instance
                if (!processIds.empty()) {
                    try {
                        auto processInfo = getProcessInfoByPid(processIds[0]);
                        std::cout << "  Details for PID " << processIds[0]
                                  << ":" << std::endl;
                        std::cout << "    Name: " << processInfo.name
                                  << std::endl;
                        std::cout << "    Parent PID: " << processInfo.ppid
                                  << std::endl;
                        std::cout << "    Priority: " << processInfo.priority
                                  << std::endl;
                    } catch (const std::exception& e) {
                        std::cout << "    Could not get details: " << e.what()
                                  << std::endl;
                    }
                }
            }
        }

        // 4. Get parent process information
        std::cout << "\n[4. Parent Process Information]" << std::endl;
        int parentPid = getParentProcessId(selfProcess.pid);
        if (parentPid > 0) {
            std::cout << "Parent process PID: " << parentPid << std::endl;
            try {
                auto parentInfo = getProcessInfoByPid(parentPid);
                std::cout << "Parent process name: " << parentInfo.name
                          << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Could not get parent process details: "
                          << e.what() << std::endl;
            }
        } else {
            std::cout << "No parent process found or access denied"
                      << std::endl;
        }

        // 5. Basic resource information
        std::cout << "\n[5. Basic Resource Information]" << std::endl;
        try {
            auto resources = getProcessResources(selfProcess.pid);
            std::cout << "Current process resources:" << std::endl;
            std::cout << "  CPU Usage: " << std::fixed << std::setprecision(2)
                      << resources.cpuUsage << "%" << std::endl;
            std::cout << "  Memory Usage: "
                      << (resources.memoryUsage / 1024 / 1024) << " MB"
                      << std::endl;
            std::cout << "  Thread Count: " << resources.threadCount
                      << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Could not get resource information: " << e.what()
                      << std::endl;
        }

        // 6. Terminal information (if available)
        std::cout << "\n[6. Terminal Information]" << std::endl;
        try {
            std::string terminal = ctermid();
            if (!terminal.empty()) {
                std::cout << "Controlling terminal: " << terminal << std::endl;
            } else {
                std::cout << "No controlling terminal" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Could not get terminal information: " << e.what()
                      << std::endl;
        }

        std::cout << "\n=== Basic Process Operations Complete ===" << std::endl;
        std::cout << "Next steps: Try the comprehensive process example for "
                     "advanced features!"
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
