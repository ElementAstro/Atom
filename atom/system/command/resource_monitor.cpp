/*
 * resource_monitor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "resource_monitor.hpp"

#include <atomic>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

// Global instance
namespace {
    std::shared_ptr<ResourceMonitor> g_resourceMonitor;
    std::mutex g_resourceMonitorMutex;
}

class ResourceMonitor::Impl {
public:
    Impl() = default;

    ~Impl() {
        stopMonitoring();
    }

    void startMonitoring(const ResourceLimits& limits, std::function<void(const std::string&)> callback) {
        limits_ = limits;
        callback_ = callback;
        monitoringActive_ = true;

        monitoringThread_ = std::thread([this]() {
            while (monitoringActive_) {
                checkResourceUsage();
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });

        spdlog::info("Resource monitoring started");
    }

    void stopMonitoring() {
        monitoringActive_ = false;
        if (monitoringThread_.joinable()) {
            monitoringThread_.join();
        }
        spdlog::info("Resource monitoring stopped");
    }

    auto getCurrentResourceUsage() const -> std::string {
        std::ostringstream usage;
        usage << "Current Resource Usage:\n";

#ifdef _WIN32
        // Windows implementation
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);

        usage << "  Memory Usage: " << (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024 * 1024) << " MB\n";
        usage << "  Memory Available: " << memInfo.ullAvailPhys / (1024 * 1024) << " MB\n";
#else
        // Linux implementation
        std::ifstream meminfo("/proc/meminfo");
        if (meminfo.is_open()) {
            std::string line;
            while (std::getline(meminfo, line)) {
                if (line.substr(0, 9) == "MemTotal:" ||
                    line.substr(0, 12) == "MemAvailable:") {
                    usage << "  " << line << "\n";
                }
            }
        }

        std::ifstream loadavg("/proc/loadavg");
        if (loadavg.is_open()) {
            std::string load;
            std::getline(loadavg, load);
            usage << "  Load Average: " << load << "\n";
        }
#endif

        return usage.str();
    }

    auto areResourceLimitsExceeded(const ResourceLimits& limits) const -> bool {
        // Simplified implementation
        // In a real implementation, you'd check actual resource usage
        return false;
    }

private:
    void checkResourceUsage() {
        if (areResourceLimitsExceeded(limits_)) {
            if (callback_) {
                callback_("Resource limits exceeded");
            }
        }
    }

    ResourceLimits limits_;
    std::function<void(const std::string&)> callback_;
    std::atomic<bool> monitoringActive_{false};
    std::thread monitoringThread_;
};

ResourceMonitor::ResourceMonitor() : pImpl_(std::make_unique<Impl>()) {}

ResourceMonitor::~ResourceMonitor() = default;

void ResourceMonitor::startMonitoring(const ResourceLimits& limits, std::function<void(const std::string&)> callback) {
    pImpl_->startMonitoring(limits, callback);
}

void ResourceMonitor::stopMonitoring() {
    pImpl_->stopMonitoring();
}

auto ResourceMonitor::getCurrentResourceUsage() const -> std::string {
    return pImpl_->getCurrentResourceUsage();
}

auto ResourceMonitor::areResourceLimitsExceeded(const ResourceLimits& limits) const -> bool {
    return pImpl_->areResourceLimitsExceeded(limits);
}

auto getGlobalResourceMonitor() -> std::shared_ptr<ResourceMonitor> {
    std::lock_guard<std::mutex> lock(g_resourceMonitorMutex);

    if (!g_resourceMonitor) {
        g_resourceMonitor = std::make_shared<ResourceMonitor>();
        spdlog::info("Created global resource monitor");
    }

    return g_resourceMonitor;
}

}  // namespace atom::system
