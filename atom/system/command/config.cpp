/*
 * config.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "config.hpp"

#include <algorithm>
#include <fstream>
#include <spdlog/spdlog.h>

namespace atom::system {

// ============================================================================
// RateLimiter Implementation
// ============================================================================

RateLimiter::RateLimiter(size_t maxRequests, std::chrono::milliseconds window)
    : maxRequests_(maxRequests), window_(window) {}

bool RateLimiter::allowRequest(const std::string& identifier) {
    std::lock_guard<std::mutex> globalLock(globalMutex_);

    auto& window = windows_[identifier];
    if (!window) {
        window = std::make_unique<RequestWindow>();
    }

    std::lock_guard<std::mutex> windowLock(window->mutex);

    auto now = std::chrono::steady_clock::now();
    cleanupOldRequests(*window);

    if (window->requests.size() >= maxRequests_) {
        return false;
    }

    window->requests.push_back(now);
    return true;
}

size_t RateLimiter::getCurrentCount(const std::string& identifier) const {
    std::lock_guard<std::mutex> globalLock(globalMutex_);

    auto it = windows_.find(identifier);
    if (it == windows_.end()) {
        return 0;
    }

    std::lock_guard<std::mutex> windowLock(it->second->mutex);
    cleanupOldRequests(*it->second);
    return it->second->requests.size();
}

void RateLimiter::reset() {
    std::lock_guard<std::mutex> lock(globalMutex_);
    windows_.clear();
}

void RateLimiter::cleanupOldRequests(RequestWindow& window) const {
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - window_;

    window.requests.erase(
        std::remove_if(window.requests.begin(), window.requests.end(),
                      [cutoff](const auto& timestamp) {
                          return timestamp < cutoff;
                      }),
        window.requests.end());
}

// ============================================================================
// ConfigManager Implementation
// ============================================================================

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    initializeRateLimiter();
}

const CommandSystemConfig& ConfigManager::getConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

void ConfigManager::updateConfig(const CommandSystemConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;

    // Reinitialize rate limiter with new settings
    initializeRateLimiter();

    spdlog::info("Command system configuration updated");
}

const CommandSystemMetrics& ConfigManager::getMetrics() const {
    return metrics_;
}

void ConfigManager::resetMetrics() {
    metrics_.reset();
    spdlog::info("Command system metrics reset");
}

RateLimiter& ConfigManager::getRateLimiter() {
    return *rateLimiter_;
}

bool ConfigManager::loadFromFile(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            spdlog::warn("Could not open config file: {}", filePath);
            return false;
        }

        // Simple key-value parsing (could be enhanced with JSON/YAML)
        std::string line;
        CommandSystemConfig newConfig = config_;

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
            if (key == "defaultTimeout") {
                newConfig.defaultTimeout = std::chrono::milliseconds(std::stoi(value));
            } else if (key == "maxThreads") {
                newConfig.maxThreads = std::stoul(value);
            } else if (key == "enableMetrics") {
                newConfig.enableMetrics = (value == "true" || value == "1");
            } else if (key == "maxHistoryEntries") {
                newConfig.maxHistoryEntries = std::stoul(value);
            }
            // Add more configuration options as needed
        }

        updateConfig(newConfig);
        spdlog::info("Configuration loaded from: {}", filePath);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Error loading configuration from {}: {}", filePath, e.what());
        return false;
    }
}

bool ConfigManager::saveToFile(const std::string& filePath) const {
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            spdlog::error("Could not create config file: {}", filePath);
            return false;
        }

        std::lock_guard<std::mutex> lock(configMutex_);

        file << "# Command System Configuration\n";
        file << "defaultTimeout=" << config_.defaultTimeout.count() << "\n";
        file << "maxThreads=" << config_.maxThreads << "\n";
        file << "enableMetrics=" << (config_.enableMetrics ? "true" : "false") << "\n";
        file << "maxHistoryEntries=" << config_.maxHistoryEntries << "\n";
        file << "enableSecurityValidation=" << (config_.enableSecurityValidation ? "true" : "false") << "\n";
        file << "auditLogPath=" << config_.auditLogPath << "\n";
        file << "historyFilePath=" << config_.historyFilePath << "\n";

        spdlog::info("Configuration saved to: {}", filePath);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Error saving configuration to {}: {}", filePath, e.what());
        return false;
    }
}

void ConfigManager::initializeRateLimiter() {
    rateLimiter_ = std::make_unique<RateLimiter>(
        config_.maxCommandsPerSecond,
        config_.rateLimitWindow
    );
}

} // namespace atom::system
