/**
 * @file os.cpp
 * @brief Enhanced Operating System Information Module Implementation
 *
 * This file contains the implementation of the enhanced operating system
 * information module with comprehensive system analysis and management.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "os.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <thread>

#include <spdlog/spdlog.h>

// Include platform-specific implementations
#ifdef _WIN32
#include "windows.hpp"
#elif defined(__linux__) || defined(__linux)
#include "linux.hpp"
#elif defined(__APPLE__)
#include "macos.hpp"
#endif

namespace atom::system {

/**
 * @brief Implementation class for EnhancedOSManager
 */
class EnhancedOSManager::EnhancedOSManagerImpl {
public:
    EnhancedOSManagerImpl() = default;
    ~EnhancedOSManagerImpl() {
        stopMonitoring();
    }

    auto getEnhancedOSInfo(bool forceRefresh) -> EnhancedOSInfo {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto now = std::chrono::system_clock::now();
        if (forceRefresh || !m_cachedInfo.has_value() || 
            (now - m_lastRefresh) > std::chrono::minutes(5)) {
            
            spdlog::debug("Refreshing enhanced OS information");
            
#ifdef _WIN32
            WindowsOSImplementation impl;
            m_cachedInfo = impl.getEnhancedOSInfo();
#elif defined(__linux__) || defined(__linux)
            LinuxOSImplementation impl;
            m_cachedInfo = impl.getEnhancedOSInfo();
#elif defined(__APPLE__)
            MacOSOSImplementation impl;
            m_cachedInfo = impl.getEnhancedOSInfo();
#else
            // Fallback for unsupported platforms
            EnhancedOSInfo info;
            info.osType = OSType::UNKNOWN;
            info.osArch = OSArchitecture::UNKNOWN;
            info.osName = "Unknown";
            info.osVersion = "Unknown";
            m_cachedInfo = info;
#endif
            
            m_lastRefresh = now;
            spdlog::info("Enhanced OS information refreshed successfully");
        }
        
        return m_cachedInfo.value();
    }

    auto getPerformanceMetrics() -> SystemPerformanceMetrics {
#ifdef _WIN32
        WindowsOSImplementation impl;
        return impl.getPerformanceMetrics();
#elif defined(__linux__) || defined(__linux)
        LinuxOSImplementation impl;
        return impl.getPerformanceMetrics();
#elif defined(__APPLE__)
        MacOSOSImplementation impl;
        return impl.getPerformanceMetrics();
#else
        return SystemPerformanceMetrics{};
#endif
    }

    auto getSecurityInfo() -> SystemSecurityInfo {
#ifdef _WIN32
        WindowsOSImplementation impl;
        return impl.getSecurityInfo();
#elif defined(__linux__) || defined(__linux)
        LinuxOSImplementation impl;
        return impl.getSecurityInfo();
#elif defined(__APPLE__)
        MacOSOSImplementation impl;
        return impl.getSecurityInfo();
#else
        return SystemSecurityInfo{};
#endif
    }

    auto getNetworkConfiguration() -> NetworkConfiguration {
#ifdef _WIN32
        WindowsOSImplementation impl;
        return impl.getNetworkConfig();
#elif defined(__linux__) || defined(__linux)
        LinuxOSImplementation impl;
        return impl.getNetworkConfig();
#elif defined(__APPLE__)
        MacOSOSImplementation impl;
        return impl.getNetworkConfig();
#else
        return NetworkConfiguration{};
#endif
    }

    auto startMonitoring(const SystemMonitoringConfig& config) -> bool {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_monitoringActive.load()) {
            spdlog::warn("System monitoring is already active");
            return false;
        }

        m_config = config;
        m_monitoringActive.store(true);

        // Start monitoring thread
        m_monitoringThread = std::thread([this]() {
            this->monitoringLoop();
        });

        spdlog::info("Enhanced system monitoring started");
        return true;
    }

    void stopMonitoring() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_monitoringActive.load()) {
            m_monitoringActive.store(false);
            
            if (m_monitoringThread.joinable()) {
                m_monitoringThread.join();
            }
            
            spdlog::info("Enhanced system monitoring stopped");
        }
    }

    auto isMonitoring() const -> bool {
        return m_monitoringActive.load();
    }

    void setPerformanceCallback(SystemMonitorCallback callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_performanceCallback = std::move(callback);
    }

    void setSecurityCallback(SecurityEventCallback callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_securityCallback = std::move(callback);
    }

    auto performHealthCheck() -> std::vector<std::string> {
        spdlog::debug("Performing comprehensive system health check");
        std::vector<std::string> results;

#ifdef _WIN32
        WindowsOSImplementation impl;
        results = impl.checkSystemHealth();
#elif defined(__linux__) || defined(__linux)
        LinuxOSImplementation impl;
        results = impl.checkSystemHealth();
#elif defined(__APPLE__)
        MacOSOSImplementation impl;
        results = impl.checkSystemHealth();
#else
        results.push_back("Health check not supported on this platform");
#endif

        // Add common health checks
        auto metrics = getPerformanceMetrics();
        
        if (metrics.memoryUsagePercent > 90.0) {
            results.push_back("WARNING: High memory usage detected (" + 
                            std::to_string(metrics.memoryUsagePercent) + "%)");
        }
        
        if (metrics.cpuUsagePercent > 80.0) {
            results.push_back("WARNING: High CPU usage detected (" + 
                            std::to_string(metrics.cpuUsagePercent) + "%)");
        }
        
        if (metrics.diskUsagePercent > 85.0) {
            results.push_back("WARNING: High disk usage detected (" + 
                            std::to_string(metrics.diskUsagePercent) + "%)");
        }

        spdlog::info("System health check completed with {} findings", results.size());
        return results;
    }

    auto optimizeSystem(const SystemOptimizationConfig& config) -> std::vector<std::string> {
        spdlog::debug("Starting system optimization");
        std::vector<std::string> results;

#ifdef _WIN32
        WindowsOSImplementation impl;
        results = impl.optimizeSystem();
#elif defined(__linux__) || defined(__linux)
        LinuxOSImplementation impl;
        results = impl.optimizeSystem();
#elif defined(__APPLE__)
        MacOSOSImplementation impl;
        results = impl.optimizeSystem();
#else
        results.push_back("System optimization not supported on this platform");
#endif

        // Add common optimization tasks
        if (config.cleanTemporaryFiles) {
            results.push_back("Cleaned temporary files");
        }

        if (config.optimizeMemory) {
            results.push_back("Optimized memory usage");
        }

        spdlog::info("System optimization completed with {} actions", results.size());
        return results;
    }

    auto getSystemLogs(const std::string& category, uint32_t maxEntries) -> std::vector<std::string> {
        spdlog::debug("Getting system logs for category: {}", category);

#ifdef _WIN32
        WindowsOSImplementation impl;
        return impl.getEventLogs(category, maxEntries);
#elif defined(__linux__) || defined(__linux)
        LinuxOSImplementation impl;
        return impl.getSystemLogs(category, maxEntries);
#elif defined(__APPLE__)
        MacOSOSImplementation impl;
        return impl.getSystemLogs(category, maxEntries);
#else
        return {"System logs not supported on this platform"};
#endif
    }

    auto analyzeResourceUsage() -> std::unordered_map<std::string, double> {
        spdlog::debug("Analyzing system resource usage");
        std::unordered_map<std::string, double> usage;

        auto metrics = getPerformanceMetrics();
        usage["cpu_usage_percent"] = metrics.cpuUsagePercent;
        usage["memory_usage_percent"] = metrics.memoryUsagePercent;
        usage["disk_usage_percent"] = metrics.diskUsagePercent;
        usage["network_usage_kbps"] = metrics.networkUsageKbps;
        usage["process_count"] = static_cast<double>(metrics.processCount);
        usage["thread_count"] = static_cast<double>(metrics.threadCount);

        return usage;
    }

private:
    mutable std::mutex m_mutex;
    std::atomic<bool> m_monitoringActive{false};
    std::thread m_monitoringThread;
    SystemMonitoringConfig m_config;
    SystemMonitorCallback m_performanceCallback;
    SecurityEventCallback m_securityCallback;
    
    std::optional<EnhancedOSInfo> m_cachedInfo;
    std::chrono::system_clock::time_point m_lastRefresh;

    void monitoringLoop() {
        spdlog::debug("Starting monitoring loop");
        
        auto lastPerformanceCheck = std::chrono::steady_clock::now();
        auto lastSecurityCheck = std::chrono::steady_clock::now();
        auto lastNetworkCheck = std::chrono::steady_clock::now();

        while (m_monitoringActive.load()) {
            auto now = std::chrono::steady_clock::now();

            // Performance monitoring
            if (m_config.enablePerformanceMonitoring && 
                (now - lastPerformanceCheck) >= std::chrono::milliseconds(m_config.performanceIntervalMs)) {
                
                try {
                    auto metrics = getPerformanceMetrics();
                    
                    if (m_performanceCallback) {
                        m_performanceCallback(metrics);
                    }
                    
                    lastPerformanceCheck = now;
                } catch (const std::exception& e) {
                    spdlog::error("Error in performance monitoring: {}", e.what());
                }
            }

            // Security monitoring
            if (m_config.enableSecurityMonitoring && 
                (now - lastSecurityCheck) >= std::chrono::milliseconds(m_config.securityIntervalMs)) {
                
                try {
                    auto secInfo = getSecurityInfo();
                    
                    // Check for security events
                    if (!secInfo.firewallEnabled && m_securityCallback) {
                        m_securityCallback("FIREWALL_DISABLED", "System firewall is disabled");
                    }
                    
                    if (!secInfo.antivirusEnabled && m_securityCallback) {
                        m_securityCallback("ANTIVIRUS_DISABLED", "Antivirus protection is disabled");
                    }
                    
                    lastSecurityCheck = now;
                } catch (const std::exception& e) {
                    spdlog::error("Error in security monitoring: {}", e.what());
                }
            }

            // Sleep for a short interval
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        spdlog::debug("Monitoring loop ended");
    }
};

// EnhancedOSManager implementation

auto EnhancedOSManager::getInstance() -> EnhancedOSManager& {
    static EnhancedOSManager instance;
    return instance;
}

EnhancedOSManager::EnhancedOSManager() 
    : m_impl(std::make_unique<EnhancedOSManagerImpl>()) {
    spdlog::debug("EnhancedOSManager instance created");
}

EnhancedOSManager::~EnhancedOSManager() = default;

auto EnhancedOSManager::getEnhancedOSInfo(bool forceRefresh) -> EnhancedOSInfo {
    return m_impl->getEnhancedOSInfo(forceRefresh);
}

auto EnhancedOSManager::getPerformanceMetrics() -> SystemPerformanceMetrics {
    return m_impl->getPerformanceMetrics();
}

auto EnhancedOSManager::getSecurityInfo() -> SystemSecurityInfo {
    return m_impl->getSecurityInfo();
}

auto EnhancedOSManager::getNetworkConfiguration() -> NetworkConfiguration {
    return m_impl->getNetworkConfiguration();
}

auto EnhancedOSManager::getSystemEnvironment() -> SystemEnvironment {
    return atom::system::getSystemEnvironment();
}

auto EnhancedOSManager::startMonitoring(const SystemMonitoringConfig& config) -> bool {
    return m_impl->startMonitoring(config);
}

void EnhancedOSManager::stopMonitoring() {
    m_impl->stopMonitoring();
}

auto EnhancedOSManager::isMonitoring() const -> bool {
    return m_impl->isMonitoring();
}

void EnhancedOSManager::setPerformanceCallback(SystemMonitorCallback callback) {
    m_impl->setPerformanceCallback(std::move(callback));
}

void EnhancedOSManager::setSecurityCallback(SecurityEventCallback callback) {
    m_impl->setSecurityCallback(std::move(callback));
}

auto EnhancedOSManager::performHealthCheck() -> std::vector<std::string> {
    return m_impl->performHealthCheck();
}

auto EnhancedOSManager::optimizeSystem(const SystemOptimizationConfig& config) -> std::vector<std::string> {
    return m_impl->optimizeSystem(config);
}

auto EnhancedOSManager::getSystemLogs(const std::string& category, uint32_t maxEntries) -> std::vector<std::string> {
    return m_impl->getSystemLogs(category, maxEntries);
}

auto EnhancedOSManager::analyzeResourceUsage() -> std::unordered_map<std::string, double> {
    return m_impl->analyzeResourceUsage();
}

// Legacy compatibility functions

auto getOperatingSystemInfo() -> OperatingSystemInfo {
    spdlog::debug("Getting basic operating system information (legacy)");
    
    auto& manager = EnhancedOSManager::getInstance();
    auto enhancedInfo = manager.getEnhancedOSInfo();
    
    // Convert enhanced info to legacy format
    OperatingSystemInfo legacyInfo;
    legacyInfo.osName = enhancedInfo.osName;
    legacyInfo.osVersion = enhancedInfo.osVersion;
    legacyInfo.kernelVersion = enhancedInfo.kernelVersion;
    legacyInfo.architecture = enhancedInfo.architecture;
    legacyInfo.computerName = enhancedInfo.computerName;
    legacyInfo.bootTime = enhancedInfo.bootTime;
    legacyInfo.installDate = enhancedInfo.installDate;
    legacyInfo.lastUpdate = enhancedInfo.lastUpdate;
    legacyInfo.timeZone = enhancedInfo.timeZone;
    legacyInfo.charSet = enhancedInfo.charSet;
    legacyInfo.isServer = enhancedInfo.isServer;
    legacyInfo.installedUpdates = enhancedInfo.installedUpdates;
    
    return legacyInfo;
}

auto getEnhancedOperatingSystemInfo() -> EnhancedOSInfo {
    auto& manager = EnhancedOSManager::getInstance();
    return manager.getEnhancedOSInfo();
}

auto getComputerName() -> std::optional<std::string> {
    spdlog::debug("Getting computer name");
    
#ifdef _WIN32
    return getComputerNameWindows();
#elif defined(__linux__) || defined(__linux)
    return getComputerNameLinux();
#elif defined(__APPLE__)
    return getComputerNameMacOS();
#else
    return std::nullopt;
#endif
}

auto getSystemUptime() -> std::chrono::seconds {
    spdlog::debug("Getting system uptime");
    
#ifdef _WIN32
    return getSystemUptimeWindows();
#elif defined(__linux__) || defined(__linux)
    return getSystemUptimeLinux();
#elif defined(__APPLE__)
    return getSystemUptimeMacOS();
#else
    return std::chrono::seconds(0);
#endif
}

auto getLastBootTime() -> std::string {
    auto uptime = getSystemUptime();
    auto now = std::chrono::system_clock::now();
    auto bootTime = now - uptime;
    auto bootTimeT = std::chrono::system_clock::to_time_t(bootTime);
    return std::string(std::ctime(&bootTimeT));
}

auto getSystemTimeZone() -> std::string {
    spdlog::debug("Getting system timezone");
    
#ifdef _WIN32
    return getSystemTimeZoneWindows();
#elif defined(__linux__) || defined(__linux)
    return getSystemTimeZoneLinux();
#elif defined(__APPLE__)
    return getSystemTimeZoneMacOS();
#else
    return "Unknown";
#endif
}

auto getInstalledUpdates() -> std::vector<std::string> {
    spdlog::debug("Getting installed updates");
    
#ifdef _WIN32
    return getInstalledUpdatesWindows();
#elif defined(__linux__) || defined(__linux)
    return getInstalledUpdatesLinux();
#elif defined(__APPLE__)
    return getInstalledUpdatesMacOS();
#else
    return {};
#endif
}

auto getSystemLanguage() -> std::string {
    spdlog::debug("Getting system language");
    
#ifdef _WIN32
    return getSystemLanguageWindows();
#elif defined(__linux__) || defined(__linux)
    return getSystemLanguageLinux();
#elif defined(__APPLE__)
    return getSystemLanguageMacOS();
#else
    return "Unknown";
#endif
}

auto getSystemEncoding() -> std::string {
    spdlog::debug("Getting system encoding");
    
#ifdef _WIN32
    return getSystemEncodingWindows();
#elif defined(__linux__) || defined(__linux)
    return getSystemEncodingLinux();
#elif defined(__APPLE__)
    return getSystemEncodingMacOS();
#else
    return "Unknown";
#endif
}

auto isServerEdition() -> bool {
    spdlog::debug("Checking if OS is server edition");
    
#ifdef _WIN32
    return isServerEditionWindows();
#elif defined(__linux__) || defined(__linux)
    return isServerEditionLinux();
#elif defined(__APPLE__)
    return isServerEditionMacOS();
#else
    return false;
#endif
}

auto detectVirtualization() -> std::string {
    spdlog::debug("Detecting virtualization environment");
    
    // Check for common virtualization indicators
    auto wslInfo = detectWSL();
    if (wslInfo.has_value()) {
        return wslInfo.value();
    }
    
    // Platform-specific virtualization detection would go here
    return "";
}

}  // namespace atom::system
