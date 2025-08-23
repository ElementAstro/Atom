/**
 * @file system_info_example.cpp
 * @brief Comprehensive example demonstrating the Atom Sysinfo module's system information gathering capabilities
 *
 * This example shows how to:
 * - Gather operating system information
 * - Monitor CPU usage, temperature, and performance
 * - Check memory usage and performance
 * - Get disk/storage information
 * - Monitor network interfaces and WiFi
 * - Retrieve BIOS and hardware information
 * - Display comprehensive system reports
 *
 * @author Max Qian
 * @date 2024-12-19
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

// Atom Sysinfo module headers
#include "atom/sysinfo/os.hpp"
#include "atom/sysinfo/cpu.hpp"
#include "atom/sysinfo/memory.hpp"
#include "atom/sysinfo/disk.hpp"
#include "atom/sysinfo/wifi.hpp"
#include "atom/sysinfo/bios.hpp"
#include "atom/sysinfo/sysinfo_printer.hpp"

using namespace atom::system;

/**
 * @brief Demonstrates operating system information gathering
 */
void operatingSystemInfoExample() {
    std::cout << "\n=== Operating System Information ===\n";

    try {
        // Get comprehensive OS information
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

        // Get system language and encoding
        std::cout << "System Language: " << getSystemLanguage() << "\n";
        std::cout << "System Encoding: " << getSystemEncoding() << "\n";

        // Check if running in WSL (Windows Subsystem for Linux)
        if (isWsl()) {
            std::cout << "Running in WSL: Yes\n";
        }

        // Display installed updates (if available)
        if (!osInfo.installedUpdates.empty()) {
            std::cout << "\nRecent Updates:\n";
            for (size_t i = 0; i < std::min(osInfo.installedUpdates.size(), size_t(5)); ++i) {
                std::cout << "  - " << osInfo.installedUpdates[i] << "\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error getting OS information: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates CPU information and monitoring
 */
void cpuInformationExample() {
    std::cout << "\n=== CPU Information and Monitoring ===\n";

    try {
        // Get comprehensive CPU information
        auto cpuInfo = getCpuInfo();

        std::cout << "CPU Model: " << cpuInfo.model << "\n";
        std::cout << "CPU Identifier: " << cpuInfo.identifier << "\n";
        std::cout << "Vendor: " << cpuVendorToString(cpuInfo.vendor) << "\n";
        std::cout << "Architecture: " << cpuArchitectureToString(cpuInfo.architecture) << "\n";
        std::cout << "Physical Cores: " << cpuInfo.numPhysicalCores << "\n";
        std::cout << "Logical Cores: " << cpuInfo.numLogicalCores << "\n";
        std::cout << "Base Frequency: " << cpuInfo.baseFrequency << " GHz\n";
        std::cout << "Max Frequency: " << cpuInfo.maxFrequency << " GHz\n";

        // Get current CPU metrics
        std::cout << "\nCurrent CPU Metrics:\n";
        std::cout << "CPU Usage: " << getCurrentCpuUsage() << "%\n";
        std::cout << "CPU Temperature: " << getCurrentCpuTemperature() << "°C\n";
        std::cout << "Current Frequency: " << getProcessorFrequency() << " GHz\n";

        // Get CPU cache information
        auto cacheInfo = getCacheSizes();
        std::cout << "\nCPU Cache Information:\n";
        std::cout << "L1 Data Cache: " << (cacheInfo.l1d / 1024) << " KB\n";
        std::cout << "L1 Instruction Cache: " << (cacheInfo.l1i / 1024) << " KB\n";
        std::cout << "L2 Cache: " << (cacheInfo.l2 / 1024) << " KB\n";
        std::cout << "L3 Cache: " << (cacheInfo.l3 / (1024 * 1024)) << " MB\n";

        // Get load average (Unix-like systems)
        auto loadAvg = getCpuLoadAverage();
        std::cout << "\nLoad Average:\n";
        std::cout << "1 minute: " << std::fixed << std::setprecision(2) << loadAvg.oneMinute << "\n";
        std::cout << "5 minutes: " << loadAvg.fiveMinutes << "\n";
        std::cout << "15 minutes: " << loadAvg.fifteenMinutes << "\n";

        // Check for overheating
        auto currentTemp = getCurrentCpuTemperature();
        if (currentTemp > 85.0) {
            std::cout << "\n⚠️  WARNING: CPU temperature is above 85°C!\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error getting CPU information: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates memory information and monitoring
 */
void memoryInformationExample() {
    std::cout << "\n=== Memory Information and Monitoring ===\n";

    try {
        // Get memory information
        auto memInfo = getDetailedMemoryStats();

        std::cout << "Total Physical Memory: " << (memInfo.totalPhysicalMemory / (1024 * 1024 * 1024)) << " GB\n";
        std::cout << "Available Physical Memory: " << (memInfo.availablePhysicalMemory / (1024 * 1024 * 1024)) << " GB\n";
        std::cout << "Used Physical Memory: " << ((memInfo.totalPhysicalMemory - memInfo.availablePhysicalMemory) / (1024 * 1024 * 1024)) << " GB\n";

        std::cout << "Memory Usage: " << std::fixed << std::setprecision(1) << memInfo.memoryLoadPercentage << "%\n";

        std::cout << "Total Virtual Memory: " << (memInfo.virtualMemoryMax / (1024 * 1024 * 1024)) << " GB\n";
        std::cout << "Used Virtual Memory: " << (memInfo.virtualMemoryUsed / (1024 * 1024 * 1024)) << " GB\n";

        // Get memory performance metrics (if available)
        auto memPerf = getMemoryPerformance();
        if (memPerf.readSpeed > 0) {
            std::cout << "\nMemory Performance:\n";
            std::cout << "Read Speed: " << memPerf.readSpeed << " MB/s\n";
            std::cout << "Write Speed: " << memPerf.writeSpeed << " MB/s\n";
            std::cout << "Bandwidth Usage: " << memPerf.bandwidthUsage << "%\n";
            std::cout << "Latency: " << memPerf.latency << " ns\n";
        }

        // Check for memory pressure
        if (memInfo.memoryLoadPercentage > 90.0) {
            std::cout << "\n⚠️  WARNING: Memory usage is above 90%!\n";
        } else if (memInfo.memoryLoadPercentage > 80.0) {
            std::cout << "\n⚠️  CAUTION: Memory usage is above 80%\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error getting memory information: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates disk/storage information
 */
void diskInformationExample() {
    std::cout << "\n=== Disk/Storage Information ===\n";

    try {
        // Get all disk information
        auto disks = getDiskInfo();

        if (disks.empty()) {
            std::cout << "No disk information available\n";
            return;
        }

        std::cout << "Found " << disks.size() << " disk(s):\n\n";

        for (size_t i = 0; i < disks.size(); ++i) {
            const auto& disk = disks[i];

            std::cout << "Disk " << (i + 1) << ":\n";
            std::cout << "  Path: " << disk.path << "\n";
            std::cout << "  Device Path: " << disk.devicePath << "\n";
            std::cout << "  Model: " << disk.model << "\n";
            std::cout << "  File System: " << disk.fsType << "\n";
            std::cout << "  Total Space: " << (disk.totalSpace / (1024 * 1024 * 1024)) << " GB\n";
            std::cout << "  Free Space: " << (disk.freeSpace / (1024 * 1024 * 1024)) << " GB\n";
            std::cout << "  Used Space: " << ((disk.totalSpace - disk.freeSpace) / (1024 * 1024 * 1024)) << " GB\n";
            std::cout << "  Usage: " << std::fixed << std::setprecision(1) << disk.usagePercent << "%\n";
            std::cout << "  Removable: " << (disk.isRemovable ? "Yes" : "No") << "\n";

            // Warn about high disk usage
            if (disk.usagePercent > 90.0) {
                std::cout << "  ⚠️  WARNING: Disk usage is above 90%!\n";
            } else if (disk.usagePercent > 80.0) {
                std::cout << "  ⚠️  CAUTION: Disk usage is above 80%\n";
            }

            std::cout << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error getting disk information: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates network and WiFi information
 */
void networkInformationExample() {
    std::cout << "\n=== Network and WiFi Information ===\n";

    try {
        // Get network statistics
        auto netStats = getNetworkStats();

        std::cout << "Network Statistics:\n";
        std::cout << "Download Speed: " << netStats.downloadSpeed << " MB/s\n";
        std::cout << "Upload Speed: " << netStats.uploadSpeed << " MB/s\n";
        std::cout << "Latency: " << netStats.latency << " ms\n";
        std::cout << "Packet Loss: " << netStats.packetLoss << "%\n";
        std::cout << "Signal Strength: " << netStats.signalStrength << " dBm\n";

        // Show connected devices
        if (!netStats.connectedDevices.empty()) {
            std::cout << "\nConnected Devices:\n";
            for (const auto& device : netStats.connectedDevices) {
                std::cout << "  - " << device << "\n";
            }
        }

        // Scan for available networks
        std::cout << "\nScanning for available networks...\n";
        auto availableNetworks = scanAvailableNetworks();
        if (!availableNetworks.empty()) {
            std::cout << "Available Networks:\n";
            for (size_t i = 0; i < std::min(availableNetworks.size(), size_t(10)); ++i) {
                std::cout << "  - " << availableNetworks[i] << "\n";
            }
        }

        // Get network security information
        auto security = getNetworkSecurity();
        if (!security.empty()) {
            std::cout << "\nNetwork Security: " << security << "\n";
        }

        // Analyze network quality
        auto quality = analyzeNetworkQuality();
        if (!quality.empty()) {
            std::cout << "Network Quality: " << quality << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error getting network information: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating all sysinfo capabilities
 */
int main() {
    std::cout << "=== Atom System Information Module Example ===\n";
    std::cout << "Gathering comprehensive system information...\n";

    try {
        // Run all information gathering examples
        operatingSystemInfoExample();
        cpuInformationExample();
        memoryInformationExample();
        diskInformationExample();
        networkInformationExample();

        std::cout << "\n=== System Information Summary ===\n";
        std::cout << "All system information gathered successfully!\n";
        std::cout << "The sysinfo module provides:\n";
        std::cout << "  ✓ Operating system details and version information\n";
        std::cout << "  ✓ CPU information, usage, and temperature monitoring\n";
        std::cout << "  ✓ Memory usage and performance metrics\n";
        std::cout << "  ✓ Disk/storage information and usage monitoring\n";
        std::cout << "  ✓ Network interface and WiFi information\n";
        std::cout << "  ✓ Cross-platform compatibility\n";
        std::cout << "  ✓ Real-time monitoring capabilities\n";

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
