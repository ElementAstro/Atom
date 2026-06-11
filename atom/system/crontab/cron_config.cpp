/*
 * cron_config.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "cron_config.hpp"
#include "cron_event_types.hpp"

#include <fstream>
#include <spdlog/spdlog.h>

// ============================================================================
// CronExecutionTimer Implementation
// ============================================================================

CronExecutionTimer::~CronExecutionTimer() {
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime_);
    CronConfigManager::getInstance().updateMetrics(type_, jobId_, duration);
}

// ============================================================================
// CronConfigManager Implementation
// ============================================================================

CronConfigManager& CronConfigManager::getInstance() {
    static CronConfigManager instance;
    return instance;
}

CronConfigManager::CronConfigManager() {
    spdlog::info("Cron configuration manager initialized");
}

const CronSystemConfig& CronConfigManager::getConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

void CronConfigManager::updateConfig(const CronSystemConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
    spdlog::info("Cron system configuration updated");
}

const CronSystemMetrics& CronConfigManager::getMetrics() const {
    return metrics_;
}

CronSystemMetrics& CronConfigManager::getMetrics() { return metrics_; }

void CronConfigManager::resetMetrics() {
    metrics_.reset();
    spdlog::info("Cron system metrics reset");
}

size_t CronConfigManager::registerEventCallback(CronEventCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    size_t id = nextCallbackId_++;
    callbacks_[id] = std::move(callback);
    spdlog::debug("Registered cron event callback with ID: {}", id);
    return id;
}

bool CronConfigManager::unregisterEventCallback(size_t id) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    auto it = callbacks_.find(id);
    if (it != callbacks_.end()) {
        callbacks_.erase(it);
        spdlog::debug("Unregistered cron event callback with ID: {}", id);
        return true;
    }
    return false;
}

void CronConfigManager::emitEvent(const CronEvent& event) {
    if (!config_.enableMetrics) {
        return;
    }

    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (const auto& [id, callback] : callbacks_) {
        try {
            callback(event);
        } catch (const std::exception& e) {
            spdlog::error("Error in cron event callback {}: {}", id, e.what());
        }
    }
}

bool CronConfigManager::loadFromFile(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            spdlog::warn("Could not open cron config file: {}", filePath);
            return false;
        }

        // Simple key-value parsing (could be enhanced with JSON/YAML)
        std::string line;
        CronSystemConfig newConfig = config_;

        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            auto pos = line.find('=');
            if (pos == std::string::npos) continue;

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // Remove whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // Parse configuration values
            if (key == "maxJobs") {
                newConfig.maxJobs = std::stoul(value);
            } else if (key == "maxConcurrentJobs") {
                newConfig.maxConcurrentJobs = std::stoul(value);
            } else if (key == "schedulerInterval") {
                newConfig.schedulerInterval = std::chrono::milliseconds(std::stoi(value));
            } else if (key == "enableMetrics") {
                newConfig.enableMetrics = (value == "true" || value == "1");
            } else if (key == "enableCaching") {
                newConfig.enableCaching = (value == "true" || value == "1");
            } else if (key == "enablePersistence") {
                newConfig.enablePersistence = (value == "true" || value == "1");
            } else if (key == "maxThreads") {
                newConfig.maxThreads = std::stoul(value);
            } else if (key == "minThreads") {
                newConfig.minThreads = std::stoul(value);
            }
            // Add more configuration options as needed
        }

        updateConfig(newConfig);
        spdlog::info("Cron configuration loaded from: {}", filePath);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Error loading cron configuration from {}: {}", filePath, e.what());
        return false;
    }
}

bool CronConfigManager::saveToFile(const std::string& filePath) const {
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            spdlog::error("Could not create cron config file: {}", filePath);
            return false;
        }

        std::lock_guard<std::mutex> lock(configMutex_);

        file << "# Cron System Configuration\n";
        file << "maxJobs=" << config_.maxJobs << "\n";
        file << "maxJobsPerUser=" << config_.maxJobsPerUser << "\n";
        file << "maxExecutionHistory=" << config_.maxExecutionHistory << "\n";
        file << "jobTimeout=" << config_.jobTimeout.count() << "\n";
        file << "schedulerInterval=" << config_.schedulerInterval.count() << "\n";
        file << "maxConcurrentJobs=" << config_.maxConcurrentJobs << "\n";
        file << "enableJobDependencies=" << (config_.enableJobDependencies ? "true" : "false") << "\n";
        file << "enableJobPriorities=" << (config_.enableJobPriorities ? "true" : "false") << "\n";
        file << "minThreads=" << config_.minThreads << "\n";
        file << "maxThreads=" << config_.maxThreads << "\n";
        file << "threadIdleTimeout=" << config_.threadIdleTimeout.count() << "\n";
        file << "enableDynamicScaling=" << (config_.enableDynamicScaling ? "true" : "false") << "\n";
        file << "enablePersistence=" << (config_.enablePersistence ? "true" : "false") << "\n";
        file << "saveInterval=" << config_.saveInterval.count() << "\n";
        file << "enableCompression=" << (config_.enableCompression ? "true" : "false") << "\n";
        file << "enableEncryption=" << (config_.enableEncryption ? "true" : "false") << "\n";
        file << "maxBackupFiles=" << config_.maxBackupFiles << "\n";
        file << "enableMetrics=" << (config_.enableMetrics ? "true" : "false") << "\n";
        file << "metricsUpdateInterval=" << config_.metricsUpdateInterval.count() << "\n";
        file << "enableHealthChecks=" << (config_.enableHealthChecks ? "true" : "false") << "\n";
        file << "healthCheckInterval=" << config_.healthCheckInterval.count() << "\n";
        file << "enableAuthentication=" << (config_.enableAuthentication ? "true" : "false") << "\n";
        file << "enableAuthorization=" << (config_.enableAuthorization ? "true" : "false") << "\n";
        file << "enableAuditLogging=" << (config_.enableAuditLogging ? "true" : "false") << "\n";
        file << "maxAuditLogSize=" << config_.maxAuditLogSize << "\n";
        file << "enableCaching=" << (config_.enableCaching ? "true" : "false") << "\n";
        file << "cacheSize=" << config_.cacheSize << "\n";
        file << "cacheTTL=" << config_.cacheTTL.count() << "\n";
        file << "enableMemoryPooling=" << (config_.enableMemoryPooling ? "true" : "false") << "\n";
        file << "maxMemoryUsageMB=" << config_.maxMemoryUsageMB << "\n";
        file << "maxCpuUsagePercent=" << config_.maxCpuUsagePercent << "\n";
        file << "maxFileDescriptors=" << config_.maxFileDescriptors << "\n";

        spdlog::info("Cron configuration saved to: {}", filePath);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Error saving cron configuration to {}: {}", filePath, e.what());
        return false;
    }
}

void CronConfigManager::updateMetrics(CronEventType type, const std::string& jobId,
                                     std::chrono::microseconds executionTime) {
    if (!config_.enableMetrics) {
        return;
    }

    switch (type) {
        case CronEventType::JOB_CREATED:
            metrics_.totalJobs++;
            break;
        case CronEventType::JOB_STARTED:
            metrics_.activeJobs++;
            break;
        case CronEventType::JOB_COMPLETED:
            metrics_.activeJobs--;
            metrics_.completedJobs++;
            metrics_.successfulExecutions++;
            metrics_.totalExecutions++;
            break;
        case CronEventType::JOB_FAILED:
            metrics_.activeJobs--;
            metrics_.failedJobs++;
            metrics_.failedExecutions++;
            metrics_.totalExecutions++;
            break;
        case CronEventType::JOB_TIMEOUT:
            metrics_.activeJobs--;
            metrics_.timeoutExecutions++;
            metrics_.totalExecutions++;
            break;
        case CronEventType::JOB_SKIPPED:
            metrics_.skippedJobs++;
            break;
        case CronEventType::SECURITY_VIOLATION:
            metrics_.securityViolations++;
            break;
        case CronEventType::SYSTEM_ERROR:
            metrics_.systemErrors++;
            break;
        case CronEventType::CACHE_OPERATION:
            // Cache metrics would be updated by cache implementation
            break;
        default:
            break;
    }

    // Update execution time metrics
    if (executionTime.count() > 0) {
        metrics_.totalExecutionTime += executionTime.count();

        auto currentMin = metrics_.minExecutionTime.load();
        while (executionTime.count() < currentMin &&
               !metrics_.minExecutionTime.compare_exchange_weak(currentMin, executionTime.count())) {
            // Retry until successful
        }

        auto currentMax = metrics_.maxExecutionTime.load();
        while (executionTime.count() > currentMax &&
               !metrics_.maxExecutionTime.compare_exchange_weak(currentMax, executionTime.count())) {
            // Retry until successful
        }
    }

    // Emit event if enabled
    if (config_.enableMetrics) {
        emitEvent(CronEvent(type, jobId, "", ""));
    }
}
