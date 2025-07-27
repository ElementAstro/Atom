/**
 * @file basic_usage.cpp
 * @brief Basic usage example for the Enhanced OS Module
 *
 * This example demonstrates the basic functionality of the Enhanced Operating
 * System Information Module, including system information retrieval,
 * performance monitoring, and security analysis.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <chrono>
#include <iostream>
#include <thread>

#include "atom/sysinfo/bios/os/os.hpp"

using namespace atom::system;

void demonstrateBasicInfo() {
    std::cout << "\n=== Basic System Information ===" << std::endl;
    
    // Get enhanced OS information
    auto& manager = EnhancedOSManager::getInstance();
    auto osInfo = manager.getEnhancedOSInfo();
    
    std::cout << "Operating System: " << osInfo.osName << std::endl;
    std::cout << "Version: " << osInfo.osVersion << std::endl;
    std::cout << "Architecture: " << osArchitectureToString(osInfo.osArch) << std::endl;
    std::cout << "Computer Name: " << osInfo.computerName << std::endl;
    std::cout << "Kernel Version: " << osInfo.kernelVersion << std::endl;
    std::cout << "OS Type: " << osTypeToString(osInfo.osType) << std::endl;
    std::cout << "Server Edition: " << (osInfo.isServer ? "Yes" : "No") << std::endl;
    std::cout << "Uptime: " << osInfo.uptime.count() << " seconds" << std::endl;
    std::cout << "Total Memory: " << (osInfo.totalMemoryBytes / 1024 / 1024) << " MB" << std::endl;
    std::cout << "Available Memory: " << (osInfo.availableMemoryBytes / 1024 / 1024) << " MB" << std::endl;
}

void demonstratePerformanceMetrics() {
    std::cout << "\n=== Performance Metrics ===" << std::endl;
    
    auto& manager = EnhancedOSManager::getInstance();
    auto metrics = manager.getPerformanceMetrics();
    
    std::cout << "CPU Usage: " << metrics.cpuUsagePercent << "%" << std::endl;
    std::cout << "Memory Usage: " << metrics.memoryUsagePercent << "%" << std::endl;
    std::cout << "Disk Usage: " << metrics.diskUsagePercent << "%" << std::endl;
    std::cout << "Network Usage: " << metrics.networkUsageKbps << " Kbps" << std::endl;
    std::cout << "Process Count: " << metrics.processCount << std::endl;
    std::cout << "Thread Count: " << metrics.threadCount << std::endl;
    std::cout << "Response Time: " << metrics.responseTime.count() << " ms" << std::endl;
}

void demonstrateSecurityInfo() {
    std::cout << "\n=== Security Information ===" << std::endl;
    
    auto& manager = EnhancedOSManager::getInstance();
    auto secInfo = manager.getSecurityInfo();
    
    std::cout << "Firewall Enabled: " << (secInfo.firewallEnabled ? "Yes" : "No") << std::endl;
    std::cout << "Antivirus Enabled: " << (secInfo.antivirusEnabled ? "Yes" : "No") << std::endl;
    std::cout << "Encryption Enabled: " << (secInfo.encryptionEnabled ? "Yes" : "No") << std::endl;
    std::cout << "Secure Boot Enabled: " << (secInfo.secureBootEnabled ? "Yes" : "No") << std::endl;
    std::cout << "TPM Enabled: " << (secInfo.tpmEnabled ? "Yes" : "No") << std::endl;
    
    if (!secInfo.securityFeatures.empty()) {
        std::cout << "Security Features:" << std::endl;
        for (const auto& feature : secInfo.securityFeatures) {
            std::cout << "  - " << feature << std::endl;
        }
    }
    
    if (!secInfo.vulnerabilities.empty()) {
        std::cout << "Vulnerabilities:" << std::endl;
        for (const auto& vuln : secInfo.vulnerabilities) {
            std::cout << "  - " << vuln << std::endl;
        }
    }
}

void demonstrateNetworkInfo() {
    std::cout << "\n=== Network Configuration ===" << std::endl;
    
    auto& manager = EnhancedOSManager::getInstance();
    auto netConfig = manager.getNetworkConfiguration();
    
    std::cout << "Primary Interface: " << netConfig.primaryInterface << std::endl;
    std::cout << "IP Address: " << netConfig.ipAddress << std::endl;
    std::cout << "Subnet Mask: " << netConfig.subnetMask << std::endl;
    std::cout << "Gateway: " << netConfig.gateway << std::endl;
    std::cout << "MAC Address: " << netConfig.macAddress << std::endl;
    std::cout << "DHCP Enabled: " << (netConfig.dhcpEnabled ? "Yes" : "No") << std::endl;
    
    if (!netConfig.dnsServers.empty()) {
        std::cout << "DNS Servers:" << std::endl;
        for (const auto& dns : netConfig.dnsServers) {
            std::cout << "  - " << dns << std::endl;
        }
    }
    
    if (!netConfig.additionalInterfaces.empty()) {
        std::cout << "Additional Interfaces:" << std::endl;
        for (const auto& [name, ip] : netConfig.additionalInterfaces) {
            std::cout << "  - " << name << ": " << ip << std::endl;
        }
    }
}

void demonstrateHealthCheck() {
    std::cout << "\n=== System Health Check ===" << std::endl;
    
    auto& manager = EnhancedOSManager::getInstance();
    auto healthResults = manager.performHealthCheck();
    
    if (healthResults.empty()) {
        std::cout << "System health check passed - no issues found." << std::endl;
    } else {
        std::cout << "Health check findings:" << std::endl;
        for (const auto& finding : healthResults) {
            std::cout << "  - " << finding << std::endl;
        }
    }
}

void demonstrateResourceAnalysis() {
    std::cout << "\n=== Resource Usage Analysis ===" << std::endl;
    
    auto& manager = EnhancedOSManager::getInstance();
    auto resourceUsage = manager.analyzeResourceUsage();
    
    for (const auto& [resource, usage] : resourceUsage) {
        std::cout << resource << ": " << usage << std::endl;
    }
}

void demonstrateMonitoring() {
    std::cout << "\n=== Performance Monitoring Demo ===" << std::endl;
    std::cout << "Starting 10-second monitoring session..." << std::endl;
    
    auto& manager = EnhancedOSManager::getInstance();
    
    // Set up monitoring configuration
    SystemMonitoringConfig config;
    config.performanceIntervalMs = 2000; // 2 seconds
    config.enablePerformanceMonitoring = true;
    config.enableSecurityMonitoring = true;
    
    // Set up callbacks
    manager.setPerformanceCallback([](const SystemPerformanceMetrics& metrics) {
        std::cout << "[MONITOR] CPU: " << std::fixed << std::setprecision(1) 
                  << metrics.cpuUsagePercent << "%, Memory: " 
                  << metrics.memoryUsagePercent << "%, Disk: " 
                  << metrics.diskUsagePercent << "%" << std::endl;
    });
    
    manager.setSecurityCallback([](const std::string& event, const std::string& details) {
        std::cout << "[SECURITY] " << event << ": " << details << std::endl;
    });
    
    // Start monitoring
    if (manager.startMonitoring(config)) {
        std::cout << "Monitoring started successfully." << std::endl;
        
        // Monitor for 10 seconds
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        // Stop monitoring
        manager.stopMonitoring();
        std::cout << "Monitoring stopped." << std::endl;
    } else {
        std::cout << "Failed to start monitoring." << std::endl;
    }
}

void demonstrateLegacyCompatibility() {
    std::cout << "\n=== Legacy Compatibility ===" << std::endl;
    
    // Original API still works
    auto osInfo = getOperatingSystemInfo();
    std::cout << "Legacy OS Name: " << osInfo.osName << std::endl;
    std::cout << "Legacy OS Version: " << osInfo.osVersion << std::endl;
    
    // Convenience functions
    std::cout << "Computer Name: " << getComputerName().value_or("Unknown") << std::endl;
    std::cout << "System Uptime: " << getSystemUptime().count() << " seconds" << std::endl;
    std::cout << "System Language: " << getSystemLanguage() << std::endl;
    std::cout << "System Encoding: " << getSystemEncoding() << std::endl;
    std::cout << "Server Edition: " << (isServerEdition() ? "Yes" : "No") << std::endl;
    
    // Enhanced convenience functions
    auto metrics = getSystemMetrics();
    std::cout << "Quick CPU Usage: " << metrics.cpuUsagePercent << "%" << std::endl;
    
    auto security = getSystemSecurity();
    std::cout << "Quick Firewall Status: " << (security.firewallEnabled ? "Enabled" : "Disabled") << std::endl;
}

int main() {
    std::cout << "Enhanced OS Module - Basic Usage Example" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    try {
        // Demonstrate basic functionality
        demonstrateBasicInfo();
        demonstratePerformanceMetrics();
        demonstrateSecurityInfo();
        demonstrateNetworkInfo();
        demonstrateHealthCheck();
        demonstrateResourceAnalysis();
        
        // Demonstrate monitoring (optional - can be commented out for quick testing)
        // demonstrateMonitoring();
        
        // Demonstrate legacy compatibility
        demonstrateLegacyCompatibility();
        
        std::cout << "\n=== Example completed successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
