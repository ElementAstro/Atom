/**
 * @file process_comprehensive.cpp
 * @brief Comprehensive example demonstrating advanced process management
 * capabilities
 *
 * This example showcases the complete process management functionality
 * available in the Atom System module, including:
 * - Process enumeration and filtering
 * - Detailed process information retrieval
 * - Resource monitoring and analysis
 * - Process lifecycle management
 * - Cross-platform process operations
 * - Error handling and edge cases
 *
 * @warning Some operations require elevated privileges
 * @note Cross-platform compatibility: Windows, Linux, macOS
 * @author Atom Framework
 * @date 2024
 */

#include <algorithm>
#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <thread>
#include "atom/system/process.hpp"

using namespace atom::system;

/**
 * @brief Utility function to format memory size in human-readable format
 */
std::string formatMemorySize(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return oss.str();
}

/**
 * @brief Utility function to format time duration
 */
std::string formatDuration(std::chrono::seconds duration) {
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(
        duration % std::chrono::hours(1));
    auto seconds = duration % std::chrono::minutes(1);

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hours.count() << ":"
        << std::setw(2) << minutes.count() << ":" << std::setw(2)
        << seconds.count();
    return oss.str();
}

/**
 * @brief Print detailed process information
 */
void printProcessDetails(const Process& process) {
    std::cout << "\n=== Process Details ===" << std::endl;
    std::cout << "PID: " << process.pid << std::endl;
    std::cout << "PPID: " << process.ppid << std::endl;
    std::cout << "Name: " << process.name << std::endl;
    std::cout << "Command: " << process.command << std::endl;
    std::cout << "Path: " << process.path << std::endl;
    std::cout << "Status: " << process.status << std::endl;
    std::cout << "Username: " << process.username << std::endl;
    std::cout << "Priority: " << process.priority << std::endl;

    // Format and display start time
    auto startTime = std::chrono::system_clock::to_time_t(process.startTime);
    std::cout << "Start Time: "
              << std::put_time(std::localtime(&startTime), "%Y-%m-%d %H:%M:%S")
              << std::endl;

    // Display resource usage
    std::cout << "\n--- Resource Usage ---" << std::endl;
    std::cout << "CPU Usage: " << std::fixed << std::setprecision(2)
              << process.resources.cpuUsage << "%" << std::endl;
    std::cout << "Memory Usage: "
              << formatMemorySize(process.resources.memoryUsage) << std::endl;
    std::cout << "Virtual Memory: "
              << formatMemorySize(process.resources.virtualMemoryUsage)
              << std::endl;
    std::cout << "Thread Count: " << process.resources.threadCount << std::endl;
    std::cout << "Handle Count: " << process.resources.handleCount << std::endl;

    // Display environment variables (first few)
    if (!process.environment.empty()) {
        std::cout << "\n--- Environment Variables (first 5) ---" << std::endl;
        int count = 0;
        for (const auto& [key, value] : process.environment) {
            if (count++ >= 5)
                break;
            std::cout << key << "=" << value.substr(0, 50);
            if (value.length() > 50)
                std::cout << "...";
            std::cout << std::endl;
        }
    }
}

int main() {
    try {
        std::cout << "=== Atom System Process Management Example ==="
                  << std::endl;
        std::cout
            << "Demonstrating comprehensive process management capabilities\n"
            << std::endl;

        // 1. Get information about all processes
        std::cout << "[1. Process Enumeration]" << std::endl;
        auto allProcesses = getAllProcesses();
        std::cout << "Total processes found: " << allProcesses.size()
                  << std::endl;

        // Display first 10 processes
        std::cout << "\nFirst 10 processes:" << std::endl;
        std::cout << std::setw(8) << "PID" << " | " << std::setw(30) << "Name"
                  << std::endl;
        std::cout << std::string(42, '-') << std::endl;

        int count = 0;
        for (const auto& [pid, name] : allProcesses) {
            if (count++ >= 10)
                break;
            std::cout << std::setw(8) << pid << " | " << std::setw(30) << name
                      << std::endl;
        }

        // 2. Get information about the current process
        std::cout << "\n[2. Current Process Information]" << std::endl;
        auto selfProcessInfo = getSelfProcessInfo();
        printProcessDetails(selfProcessInfo);

        // 3. Find processes by name
        std::cout << "\n[3. Process Search by Name]" << std::endl;
        std::string searchName;

        // Try to find a common process name
        std::vector<std::string> commonProcesses = {
#ifdef _WIN32
            "explorer.exe", "winlogon.exe", "svchost.exe", "dwm.exe"
#elif defined(__APPLE__)
            "kernel_task", "launchd", "WindowServer", "Finder"
#else
            "systemd", "kthreadd", "init", "bash", "ssh"
#endif
        };

        for (const auto& processName : commonProcesses) {
            auto processIds = getProcessIdByName(processName);
            if (!processIds.empty()) {
                searchName = processName;
                std::cout << "Found " << processIds.size() << " instances of '"
                          << processName << "':" << std::endl;
                for (int pid : processIds) {
                    std::cout << "  PID: " << pid << std::endl;
                }
                break;
            }
        }

        if (!searchName.empty()) {
            // 4. Get detailed information about found process
            std::cout << "\n[4. Detailed Process Analysis]" << std::endl;
            auto processIds = getProcessIdByName(searchName);
            if (!processIds.empty()) {
                int targetPid = processIds[0];

                try {
                    auto processInfo = getProcessInfoByPid(targetPid);
                    printProcessDetails(processInfo);

                    // Get parent process information
                    std::cout << "\n[5. Parent Process Information]"
                              << std::endl;
                    int parentPid = getParentProcessId(targetPid);
                    if (parentPid > 0) {
                        std::cout << "Parent PID: " << parentPid << std::endl;
                        try {
                            auto parentInfo = getProcessInfoByPid(parentPid);
                            std::cout << "Parent Name: " << parentInfo.name
                                      << std::endl;
                            std::cout
                                << "Parent Command: " << parentInfo.command
                                << std::endl;
                        } catch (const std::exception& e) {
                            std::cout
                                << "Could not get parent process details: "
                                << e.what() << std::endl;
                        }
                    } else {
                        std::cout << "No parent process or access denied"
                                  << std::endl;
                    }

                } catch (const std::exception& e) {
                    std::cout << "Error getting process details: " << e.what()
                              << std::endl;
                }
            }
        }

        // 6. Process filtering and analysis
        std::cout << "\n[6. Process Filtering and Analysis]" << std::endl;

        // Find high CPU usage processes
        std::vector<std::pair<int, double>> highCpuProcesses;
        int processCount = 0;
        const int maxProcessesToCheck =
            20;  // Limit to avoid performance issues

        for (const auto& [pid, name] : allProcesses) {
            if (processCount++ >= maxProcessesToCheck)
                break;

            try {
                auto resources = getProcessResources(pid);
                if (resources.cpuUsage > 1.0) {  // More than 1% CPU
                    highCpuProcesses.emplace_back(pid, resources.cpuUsage);
                }
            } catch (const std::exception&) {
                // Skip processes we can't access
                continue;
            }
        }

        // Sort by CPU usage
        std::sort(
            highCpuProcesses.begin(), highCpuProcesses.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        std::cout << "High CPU usage processes (>1%):" << std::endl;
        std::cout << std::setw(8) << "PID" << " | " << std::setw(10) << "CPU %"
                  << " | " << "Name" << std::endl;
        std::cout << std::string(50, '-') << std::endl;

        for (const auto& [pid, cpuUsage] : highCpuProcesses) {
            try {
                auto info = getProcessInfoByPid(pid);
                std::cout << std::setw(8) << pid << " | " << std::setw(8)
                          << std::fixed << std::setprecision(2) << cpuUsage
                          << "% | " << info.name << std::endl;
            } catch (const std::exception&) {
                // Process might have terminated
                continue;
            }
        }

        if (highCpuProcesses.empty()) {
            std::cout << "No high CPU usage processes found in sample"
                      << std::endl;
        }

        // 7. Process monitoring demonstration
        std::cout << "\n[7. Process Monitoring Demonstration]" << std::endl;
        std::cout << "Monitoring current process for 5 seconds..." << std::endl;

        auto startTime = std::chrono::steady_clock::now();
        auto endTime = startTime + std::chrono::seconds(5);

        while (std::chrono::steady_clock::now() < endTime) {
            try {
                auto currentInfo = getSelfProcessInfo();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startTime);

                std::cout << "\rTime: " << elapsed.count() << "s | "
                          << "CPU: " << std::fixed << std::setprecision(1)
                          << currentInfo.resources.cpuUsage << "% | "
                          << "Memory: "
                          << formatMemorySize(currentInfo.resources.memoryUsage)
                          << " | Threads: " << currentInfo.resources.threadCount
                          << std::flush;

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            } catch (const std::exception& e) {
                std::cout << "\nMonitoring error: " << e.what() << std::endl;
                break;
            }
        }
        std::cout << std::endl;

        // 8. Terminal and environment information
        std::cout << "\n[8. Terminal and Environment Information]" << std::endl;
        try {
            std::string terminalName = ctermid();
            std::cout << "Controlling terminal: " << terminalName << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Could not get terminal name: " << e.what()
                      << std::endl;
        }

        // 9. Platform-specific features
        std::cout << "\n[9. Platform-Specific Features]" << std::endl;

#ifdef _WIN32
        std::cout << "Windows-specific features:" << std::endl;

        // Note: This is just a demonstration - don't actually create processes
        // with credentials
        std::cout << "- Process creation as user (requires valid credentials)"
                  << std::endl;
        std::cout << "- Windows privileges management" << std::endl;
        std::cout << "- Process handle management" << std::endl;

        // Example of getting Windows privileges (if available)
        try {
            auto privileges = getWindowsPrivileges(selfProcessInfo.pid);
            std::cout << "Current process privileges: "
                      << privileges.privilegeNames.size() << " privileges found"
                      << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Could not get Windows privileges: " << e.what()
                      << std::endl;
        }

#elif defined(__linux__)
        std::cout << "Linux-specific features:" << std::endl;
        std::cout << "- Process cgroups information" << std::endl;
        std::cout << "- Process namespaces" << std::endl;
        std::cout << "- System call monitoring" << std::endl;

        // Example of getting system calls (if available)
        try {
            auto syscalls = getProcessSyscalls(selfProcessInfo.pid);
            std::cout << "System calls tracked: " << syscalls.size()
                      << " different calls" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Could not get system calls: " << e.what()
                      << std::endl;
        }

#elif defined(__APPLE__)
        std::cout << "macOS-specific features:" << std::endl;
        std::cout << "- Process code signing information" << std::endl;
        std::cout << "- Sandbox status" << std::endl;
        std::cout << "- App Store compliance" << std::endl;
#endif

        // 10. Error handling demonstration
        std::cout << "\n[10. Error Handling Demonstration]" << std::endl;

        // Try to access a non-existent process
        try {
            auto invalidProcess = getProcessInfoByPid(999999);
            std::cout << "Unexpected: Found process with PID 999999"
                      << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected error for invalid PID: " << e.what()
                      << std::endl;
        }

        // Try to access a process with insufficient privileges
        std::cout << "Testing privilege requirements..." << std::endl;
        int restrictedProcessCount = 0;
        for (const auto& [pid, name] : allProcesses) {
            if (restrictedProcessCount >= 5)
                break;  // Test only a few

            try {
                auto resources = getProcessResources(pid);
                // If we get here, we have access
            } catch (const std::exception&) {
                restrictedProcessCount++;
            }
        }

        std::cout << "Found " << restrictedProcessCount
                  << " processes with restricted access (normal behavior)"
                  << std::endl;

        std::cout << "\n=== Process Management Example Complete ==="
                  << std::endl;
        std::cout << "This example demonstrated:" << std::endl;
        std::cout << "- Process enumeration and filtering" << std::endl;
        std::cout << "- Detailed process information retrieval" << std::endl;
        std::cout << "- Resource monitoring and analysis" << std::endl;
        std::cout << "- Parent-child process relationships" << std::endl;
        std::cout << "- Real-time process monitoring" << std::endl;
        std::cout << "- Platform-specific features" << std::endl;
        std::cout << "- Comprehensive error handling" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }

    return 0;
}
