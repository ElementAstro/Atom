/**
 * @file basic_sysinfo_example.cpp
 * @brief Basic example demonstrating system information gathering
 * 
 * This example shows how to:
 * - Get basic operating system information
 * - Retrieve system uptime
 * - Check system language and encoding
 * - Get basic memory information
 * 
 * @author Max Qian
 * @date 2024-12-19
 */

#include <iostream>
#include <iomanip>
#include <string>

// Atom Sysinfo module headers
#include "atom/sysinfo/os.hpp"
#include "atom/sysinfo/hardware/memory.hpp"

using namespace atom::system;

/**
 * @brief Demonstrates basic operating system information gathering
 */
void basicOsInfoExample() {
    std::cout << "\n=== Basic Operating System Information ===\n";
    
    try {
        // Get basic OS information
        auto osInfo = getOperatingSystemInfo();
        
        std::cout << "OS Name: " << osInfo.osName << "\n";
        std::cout << "OS Version: " << osInfo.osVersion << "\n";
        std::cout << "Kernel Version: " << osInfo.kernelVersion << "\n";
        std::cout << "Architecture: " << osInfo.architecture << "\n";
        std::cout << "Computer Name: " << osInfo.computerName << "\n";
        std::cout << "Time Zone: " << osInfo.timeZone << "\n";
        std::cout << "Character Set: " << osInfo.charSet << "\n";
        std::cout << "Is Server Edition: " << (osInfo.isServer ? "Yes" : "No") << "\n";
        std::cout << "Compiler: " << osInfo.compiler << "\n";
        
        // Get system uptime
        auto uptime = getSystemUptime();
        std::cout << "System Uptime: " << uptime.count() << " seconds\n";
        
        // Convert uptime to human readable format
        auto uptimeSeconds = uptime.count();
        auto days = uptimeSeconds / (24 * 3600);
        uptimeSeconds %= (24 * 3600);
        auto hours = uptimeSeconds / 3600;
        uptimeSeconds %= 3600;
        auto minutes = uptimeSeconds / 60;
        auto seconds = uptimeSeconds % 60;
        
        std::cout << "Uptime (formatted): " << days << " days, " 
                  << hours << " hours, " << minutes << " minutes, " 
                  << seconds << " seconds\n";
        
        // Get system language and encoding
        std::cout << "System Language: " << getSystemLanguage() << "\n";
        std::cout << "System Encoding: " << getSystemEncoding() << "\n";
        
        // Check if running in WSL (Windows Subsystem for Linux)
        if (isWsl()) {
            std::cout << "Running in WSL: Yes\n";
        } else {
            std::cout << "Running in WSL: No\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error getting OS information: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates basic memory information
 */
void basicMemoryInfoExample() {
    std::cout << "\n=== Basic Memory Information ===\n";
    
    try {
        // Get basic memory usage percentage
        auto memoryUsage = getMemoryUsage();
        std::cout << "Memory Usage: " << std::fixed << std::setprecision(1) 
                  << memoryUsage << "%\n";
        
        // Try to get detailed memory information if available
        try {
            auto memInfo = getDetailedMemoryStats();
            
            std::cout << "Total Physical Memory: " 
                      << (memInfo.totalPhysicalMemory / (1024 * 1024 * 1024)) << " GB\n";
            std::cout << "Available Physical Memory: " 
                      << (memInfo.availablePhysicalMemory / (1024 * 1024 * 1024)) << " GB\n";
            std::cout << "Used Physical Memory: " 
                      << ((memInfo.totalPhysicalMemory - memInfo.availablePhysicalMemory) / (1024 * 1024 * 1024)) << " GB\n";
            
            std::cout << "Memory Load Percentage: " << std::fixed << std::setprecision(1) 
                      << memInfo.memoryLoadPercentage << "%\n";
            
            std::cout << "Total Virtual Memory: " 
                      << (memInfo.virtualMemoryMax / (1024 * 1024 * 1024)) << " GB\n";
            std::cout << "Used Virtual Memory: " 
                      << (memInfo.virtualMemoryUsed / (1024 * 1024 * 1024)) << " GB\n";
            
            // Check for memory pressure
            if (memInfo.memoryLoadPercentage > 90.0) {
                std::cout << "\n⚠️  WARNING: Memory usage is above 90%!\n";
            } else if (memInfo.memoryLoadPercentage > 80.0) {
                std::cout << "\n⚠️  CAUTION: Memory usage is above 80%\n";
            } else {
                std::cout << "\n✓ Memory usage is within normal range\n";
            }
            
        } catch (const std::exception& e) {
            std::cout << "Detailed memory information not available: " << e.what() << "\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error getting memory information: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates system information summary
 */
void systemSummaryExample() {
    std::cout << "\n=== System Summary ===\n";
    
    try {
        // Get OS info for summary
        auto osInfo = getOperatingSystemInfo();
        auto uptime = getSystemUptime();
        auto memoryUsage = getMemoryUsage();
        
        std::cout << "System: " << osInfo.osName << " " << osInfo.osVersion << "\n";
        std::cout << "Architecture: " << osInfo.architecture << "\n";
        std::cout << "Computer: " << osInfo.computerName << "\n";
        std::cout << "Uptime: " << (uptime.count() / 3600) << " hours\n";
        std::cout << "Memory Usage: " << std::fixed << std::setprecision(1) 
                  << memoryUsage << "%\n";
        
        // System health check
        std::cout << "\nSystem Health Check:\n";
        
        if (memoryUsage < 80.0) {
            std::cout << "✓ Memory usage is healthy\n";
        } else {
            std::cout << "⚠️  Memory usage is high\n";
        }
        
        if (uptime.count() > 86400) { // More than 1 day
            std::cout << "✓ System has been running stably\n";
        } else {
            std::cout << "ℹ️  System was recently restarted\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error generating system summary: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating basic sysinfo capabilities
 */
int main() {
    std::cout << "=== Atom System Information Module - Basic Example ===\n";
    std::cout << "Gathering basic system information...\n";
    
    try {
        // Run all basic examples
        basicOsInfoExample();
        basicMemoryInfoExample();
        systemSummaryExample();
        
        std::cout << "\n=== Basic System Information Completed Successfully ===\n";
        std::cout << "The sysinfo module provides:\n";
        std::cout << "  ✓ Operating system details and version information\n";
        std::cout << "  ✓ System uptime and status information\n";
        std::cout << "  ✓ Memory usage monitoring\n";
        std::cout << "  ✓ System language and encoding detection\n";
        std::cout << "  ✓ Cross-platform compatibility\n";
        std::cout << "  ✓ System health monitoring\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
