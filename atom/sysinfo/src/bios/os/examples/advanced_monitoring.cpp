/**
 * @file advanced_monitoring.cpp
 * @brief Advanced monitoring example for the Enhanced OS Module
 *
 * This example demonstrates advanced monitoring capabilities including
 * real-time performance tracking, security monitoring, system optimization,
 * and platform-specific features.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "atom/sysinfo/bios/os/os.hpp"

#ifdef __linux__
#include "atom/sysinfo/bios/os/linux.hpp"
#elif defined(_WIN32)
#include "atom/sysinfo/bios/os/windows.hpp"
#elif defined(__APPLE__)
#include "atom/sysinfo/bios/os/macos.hpp"
#endif

using namespace atom::system;

class SystemMonitor {
private:
    EnhancedOSManager& m_manager;
    std::vector<SystemPerformanceMetrics> m_performanceHistory;
    std::vector<std::string> m_securityEvents;
    bool m_running = false;
    
public:
    SystemMonitor() : m_manager(EnhancedOSManager::getInstance()) {}
    
    void startAdvancedMonitoring() {
        std::cout << "\n=== Starting Advanced System Monitoring ===" << std::endl;
        
        // Configure monitoring
        SystemMonitoringConfig config;
        config.performanceIntervalMs = 1000;
        config.securityIntervalMs = 5000;
        config.networkIntervalMs = 2000;
        config.enablePerformanceMonitoring = true;
        config.enableSecurityMonitoring = true;
        config.enableNetworkMonitoring = true;
        config.enableEventLogging = true;
        config.logFilePath = "system_monitor.log";
        config.maxLogEntries = 1000;
        
        // Set up performance callback
        m_manager.setPerformanceCallback([this](const SystemPerformanceMetrics& metrics) {
            this->handlePerformanceUpdate(metrics);
        });
        
        // Set up security callback
        m_manager.setSecurityCallback([this](const std::string& event, const std::string& details) {
            this->handleSecurityEvent(event, details);
        });
        
        // Start monitoring
        if (m_manager.startMonitoring(config)) {
            m_running = true;
            std::cout << "Advanced monitoring started successfully." << std::endl;
            
            // Monitor for 30 seconds
            auto startTime = std::chrono::steady_clock::now();
            while (m_running && 
                   (std::chrono::steady_clock::now() - startTime) < std::chrono::seconds(30)) {
                
                // Perform periodic health checks
                if ((std::chrono::steady_clock::now() - startTime) > std::chrono::seconds(10)) {
                    performPeriodicHealthCheck();
                    startTime = std::chrono::steady_clock::now(); // Reset timer
                }
                
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
            stopMonitoring();
        } else {
            std::cout << "Failed to start advanced monitoring." << std::endl;
        }
    }
    
    void stopMonitoring() {
        if (m_running) {
            m_manager.stopMonitoring();
            m_running = false;
            std::cout << "Advanced monitoring stopped." << std::endl;
            
            // Generate summary report
            generateSummaryReport();
        }
    }
    
private:
    void handlePerformanceUpdate(const SystemPerformanceMetrics& metrics) {
        m_performanceHistory.push_back(metrics);
        
        // Keep only last 100 entries
        if (m_performanceHistory.size() > 100) {
            m_performanceHistory.erase(m_performanceHistory.begin());
        }
        
        // Check for performance alerts
        if (metrics.cpuUsagePercent > 80.0) {
            std::cout << "[ALERT] High CPU usage: " << metrics.cpuUsagePercent << "%" << std::endl;
        }
        
        if (metrics.memoryUsagePercent > 85.0) {
            std::cout << "[ALERT] High memory usage: " << metrics.memoryUsagePercent << "%" << std::endl;
        }
        
        if (metrics.diskUsagePercent > 90.0) {
            std::cout << "[ALERT] High disk usage: " << metrics.diskUsagePercent << "%" << std::endl;
        }
        
        // Display current metrics
        std::cout << "[PERF] " << std::fixed << std::setprecision(1)
                  << "CPU: " << metrics.cpuUsagePercent << "% | "
                  << "MEM: " << metrics.memoryUsagePercent << "% | "
                  << "DISK: " << metrics.diskUsagePercent << "% | "
                  << "NET: " << metrics.networkUsageKbps << " Kbps | "
                  << "PROC: " << metrics.processCount << std::endl;
    }
    
    void handleSecurityEvent(const std::string& event, const std::string& details) {
        auto timestamp = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        
        std::string eventLog = "[" + std::string(std::ctime(&time_t)) + "] " + event + ": " + details;
        m_securityEvents.push_back(eventLog);
        
        std::cout << "[SECURITY] " << event << ": " << details << std::endl;
        
        // Take action based on event type
        if (event == "FIREWALL_DISABLED") {
            std::cout << "[ACTION] Recommending firewall activation" << std::endl;
        } else if (event == "ANTIVIRUS_DISABLED") {
            std::cout << "[ACTION] Recommending antivirus activation" << std::endl;
        }
    }
    
    void performPeriodicHealthCheck() {
        std::cout << "\n[HEALTH CHECK] Performing periodic system health check..." << std::endl;
        
        auto healthResults = m_manager.performHealthCheck();
        if (healthResults.empty()) {
            std::cout << "[HEALTH CHECK] System health: OK" << std::endl;
        } else {
            std::cout << "[HEALTH CHECK] Issues found:" << std::endl;
            for (const auto& issue : healthResults) {
                std::cout << "  - " << issue << std::endl;
            }
        }
        
        // Analyze resource trends
        if (m_performanceHistory.size() >= 10) {
            analyzePerformanceTrends();
        }
    }
    
    void analyzePerformanceTrends() {
        if (m_performanceHistory.size() < 10) return;
        
        // Calculate averages for last 10 measurements
        double avgCpu = 0.0, avgMemory = 0.0, avgDisk = 0.0;
        size_t count = std::min(m_performanceHistory.size(), size_t(10));
        
        for (size_t i = m_performanceHistory.size() - count; i < m_performanceHistory.size(); ++i) {
            avgCpu += m_performanceHistory[i].cpuUsagePercent;
            avgMemory += m_performanceHistory[i].memoryUsagePercent;
            avgDisk += m_performanceHistory[i].diskUsagePercent;
        }
        
        avgCpu /= count;
        avgMemory /= count;
        avgDisk /= count;
        
        std::cout << "[TREND] Last 10 measurements average - "
                  << "CPU: " << std::fixed << std::setprecision(1) << avgCpu << "%, "
                  << "Memory: " << avgMemory << "%, "
                  << "Disk: " << avgDisk << "%" << std::endl;
        
        // Trend analysis
        if (avgCpu > 70.0) {
            std::cout << "[TREND] CPU usage trending high - consider optimization" << std::endl;
        }
        if (avgMemory > 80.0) {
            std::cout << "[TREND] Memory usage trending high - consider cleanup" << std::endl;
        }
    }
    
    void generateSummaryReport() {
        std::cout << "\n=== Monitoring Summary Report ===" << std::endl;
        
        if (!m_performanceHistory.empty()) {
            // Calculate statistics
            double maxCpu = 0.0, maxMemory = 0.0, maxDisk = 0.0;
            double minCpu = 100.0, minMemory = 100.0, minDisk = 100.0;
            double avgCpu = 0.0, avgMemory = 0.0, avgDisk = 0.0;
            
            for (const auto& metrics : m_performanceHistory) {
                maxCpu = std::max(maxCpu, metrics.cpuUsagePercent);
                maxMemory = std::max(maxMemory, metrics.memoryUsagePercent);
                maxDisk = std::max(maxDisk, metrics.diskUsagePercent);
                
                minCpu = std::min(minCpu, metrics.cpuUsagePercent);
                minMemory = std::min(minMemory, metrics.memoryUsagePercent);
                minDisk = std::min(minDisk, metrics.diskUsagePercent);
                
                avgCpu += metrics.cpuUsagePercent;
                avgMemory += metrics.memoryUsagePercent;
                avgDisk += metrics.diskUsagePercent;
            }
            
            size_t count = m_performanceHistory.size();
            avgCpu /= count;
            avgMemory /= count;
            avgDisk /= count;
            
            std::cout << "Performance Statistics (" << count << " measurements):" << std::endl;
            std::cout << "  CPU Usage    - Min: " << std::fixed << std::setprecision(1) 
                      << minCpu << "%, Max: " << maxCpu << "%, Avg: " << avgCpu << "%" << std::endl;
            std::cout << "  Memory Usage - Min: " << minMemory << "%, Max: " << maxMemory 
                      << "%, Avg: " << avgMemory << "%" << std::endl;
            std::cout << "  Disk Usage   - Min: " << minDisk << "%, Max: " << maxDisk 
                      << "%, Avg: " << avgDisk << "%" << std::endl;
        }
        
        if (!m_securityEvents.empty()) {
            std::cout << "\nSecurity Events (" << m_securityEvents.size() << " total):" << std::endl;
            for (const auto& event : m_securityEvents) {
                std::cout << "  " << event << std::endl;
            }
        }
        
        // Save report to file
        saveReportToFile();
    }
    
    void saveReportToFile() {
        std::ofstream reportFile("monitoring_report.txt");
        if (reportFile.is_open()) {
            reportFile << "System Monitoring Report\n";
            reportFile << "========================\n\n";
            
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            reportFile << "Generated: " << std::ctime(&time_t) << "\n";
            
            // Add performance data
            reportFile << "Performance History:\n";
            for (const auto& metrics : m_performanceHistory) {
                reportFile << "CPU: " << metrics.cpuUsagePercent 
                          << "%, Memory: " << metrics.memoryUsagePercent 
                          << "%, Disk: " << metrics.diskUsagePercent << "%\n";
            }
            
            // Add security events
            reportFile << "\nSecurity Events:\n";
            for (const auto& event : m_securityEvents) {
                reportFile << event << "\n";
            }
            
            reportFile.close();
            std::cout << "Report saved to monitoring_report.txt" << std::endl;
        }
    }
};

void demonstrateSystemOptimization() {
    std::cout << "\n=== System Optimization Demo ===" << std::endl;
    
    auto& manager = EnhancedOSManager::getInstance();
    
    // Configure optimization
    SystemOptimizationConfig config;
    config.optimizeMemory = true;
    config.optimizeDisk = true;
    config.optimizeNetwork = true;
    config.cleanTemporaryFiles = true;
    config.optimizeServices = false; // Be careful with this in production
    config.optimizeStartup = false;  // Be careful with this in production
    
    std::cout << "Performing system optimization..." << std::endl;
    auto results = manager.optimizeSystem(config);
    
    std::cout << "Optimization results:" << std::endl;
    for (const auto& result : results) {
        std::cout << "  - " << result << std::endl;
    }
}

void demonstratePlatformSpecificFeatures() {
    std::cout << "\n=== Platform-Specific Features ===" << std::endl;
    
#ifdef __linux__
    std::cout << "Linux-specific features:" << std::endl;
    LinuxOSImplementation linuxImpl;
    
    auto distInfo = linuxImpl.getDistributionInfo();
    std::cout << "  Distribution: " << distInfo.prettyName << std::endl;
    std::cout << "  ID: " << distInfo.id << std::endl;
    std::cout << "  Version ID: " << distInfo.versionId << std::endl;
    
    auto packageInfo = linuxImpl.getPackageInfo();
    std::cout << "  Package Manager: " << packageInfo.packageManager << std::endl;
    std::cout << "  Total Packages: " << packageInfo.totalPackages << std::endl;
    std::cout << "  Available Updates: " << packageInfo.upgradablePackages << std::endl;
    
    auto containerInfo = linuxImpl.getContainerInfo();
    if (containerInfo.isContainer) {
        std::cout << "  Container Type: " << containerInfo.containerType << std::endl;
        std::cout << "  Container Runtime: " << containerInfo.containerRuntime << std::endl;
    } else {
        std::cout << "  Not running in a container" << std::endl;
    }
    
#elif defined(_WIN32)
    std::cout << "Windows-specific features:" << std::endl;
    WindowsOSImplementation windowsImpl;
    
    auto editionInfo = windowsImpl.getEditionInfo();
    std::cout << "  Product Name: " << editionInfo.productName << std::endl;
    std::cout << "  Edition: " << editionInfo.edition << std::endl;
    std::cout << "  Build Number: " << editionInfo.buildNumber << std::endl;
    std::cout << "  Display Version: " << editionInfo.displayVersion << std::endl;
    
    auto servicesInfo = windowsImpl.getServicesInfo();
    std::cout << "  Running Services: " << servicesInfo.runningServices.size() << std::endl;
    std::cout << "  Stopped Services: " << servicesInfo.stoppedServices.size() << std::endl;
    
#elif defined(__APPLE__)
    std::cout << "macOS-specific features:" << std::endl;
    MacOSOSImplementation macosImpl;
    
    auto versionInfo = macosImpl.getVersionInfo();
    std::cout << "  Product Name: " << versionInfo.productName << std::endl;
    std::cout << "  Product Version: " << versionInfo.productVersion << std::endl;
    std::cout << "  Build Version: " << versionInfo.productBuildVersion << std::endl;
    std::cout << "  Darwin Version: " << versionInfo.darwinVersion << std::endl;
    std::cout << "  Apple Silicon: " << (versionInfo.isAppleSilicon ? "Yes" : "No") << std::endl;
    
    auto secInfo = macosImpl.getSecurityInfo();
    std::cout << "  SIP Enabled: " << (secInfo.sipEnabled ? "Yes" : "No") << std::endl;
    std::cout << "  Gatekeeper Enabled: " << (secInfo.gatekeeperEnabled ? "Yes" : "No") << std::endl;
    std::cout << "  FileVault Enabled: " << (secInfo.filevaultEnabled ? "Yes" : "No") << std::endl;
    
#else
    std::cout << "Platform-specific features not available for this platform." << std::endl;
#endif
}

int main() {
    std::cout << "Enhanced OS Module - Advanced Monitoring Example" << std::endl;
    std::cout << "================================================" << std::endl;
    
    try {
        // Create and run system monitor
        SystemMonitor monitor;
        
        // Demonstrate platform-specific features first
        demonstratePlatformSpecificFeatures();
        
        // Demonstrate system optimization
        demonstrateSystemOptimization();
        
        // Start advanced monitoring
        std::cout << "\nPress Enter to start advanced monitoring (30 seconds)..." << std::endl;
        std::cin.get();
        
        monitor.startAdvancedMonitoring();
        
        std::cout << "\n=== Advanced monitoring example completed ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
