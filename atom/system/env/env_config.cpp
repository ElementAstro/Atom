/*
 * env_config.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "env_config.hpp"

#include <fstream>
#include <spdlog/spdlog.h>

namespace atom::utils {

// ============================================================================
// EnvConfigManager Implementation
// ============================================================================

EnvConfigManager& EnvConfigManager::getInstance() {
    static EnvConfigManager instance;
    return instance;
}

EnvConfigManager::EnvConfigManager() {
    spdlog::info("Environment configuration manager initialized");
}

const EnvSystemConfig& EnvConfigManager::getConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

void EnvConfigManager::updateConfig(const EnvSystemConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
    spdlog::info("Environment system configuration updated");
}

const EnvSystemMetrics& EnvConfigManager::getMetrics() const {
    return metrics_;
}

void EnvConfigManager::resetMetrics() {
    metrics_.reset();
    spdlog::info("Environment system metrics reset");
}

size_t EnvConfigManager::registerEventCallback(EnvEventCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    size_t id = nextCallbackId_++;
    callbacks_[id] = std::move(callback);
    spdlog::debug("Registered environment event callback with ID: {}", id);
    return id;
}

bool EnvConfigManager::unregisterEventCallback(size_t id) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    auto it = callbacks_.find(id);
    if (it != callbacks_.end()) {
        callbacks_.erase(it);
        spdlog::debug("Unregistered environment event callback with ID: {}", id);
        return true;
    }
    return false;
}

void EnvConfigManager::emitEvent(const EnvEvent& event) {
    if (!config_.enableEventLogging) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (const auto& [id, callback] : callbacks_) {
        try {
            callback(event);
        } catch (const std::exception& e) {
            spdlog::error("Error in environment event callback {}: {}", id, e.what());
        }
    }
}

bool EnvConfigManager::loadFromFile(const String& filePath) {
    try {
        std::ifstream file(std::string(filePath.data(), filePath.size()));
        if (!file.is_open()) {
            spdlog::warn("Could not open environment config file: {}", 
                        std::string(filePath.data(), filePath.size()));
            return false;
        }
        
        // Simple key-value parsing (could be enhanced with JSON/YAML)
        std::string line;
        EnvSystemConfig newConfig = config_;
        
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
            if (key == "enableGlobalCache") {
                newConfig.enableGlobalCache = (value == "true" || value == "1");
            } else if (key == "cacheTTL") {
                newConfig.cacheTTL = std::chrono::milliseconds(std::stoi(value));
            } else if (key == "maxCacheEntries") {
                newConfig.maxCacheEntries = std::stoul(value);
            } else if (key == "enableBatchOperations") {
                newConfig.enableBatchOperations = (value == "true" || value == "1");
            } else if (key == "batchSize") {
                newConfig.batchSize = std::stoul(value);
            } else if (key == "enableMetrics") {
                newConfig.enableMetrics = (value == "true" || value == "1");
            } else if (key == "enableValidation") {
                newConfig.enableValidation = (value == "true" || value == "1");
            }
            // Add more configuration options as needed
        }
        
        updateConfig(newConfig);
        spdlog::info("Environment configuration loaded from: {}", 
                    std::string(filePath.data(), filePath.size()));
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Error loading environment configuration from {}: {}", 
                     std::string(filePath.data(), filePath.size()), e.what());
        return false;
    }
}

bool EnvConfigManager::saveToFile(const String& filePath) const {
    try {
        std::ofstream file(std::string(filePath.data(), filePath.size()));
        if (!file.is_open()) {
            spdlog::error("Could not create environment config file: {}", 
                         std::string(filePath.data(), filePath.size()));
            return false;
        }
        
        std::lock_guard<std::mutex> lock(configMutex_);
        
        file << "# Environment System Configuration\n";
        file << "enableGlobalCache=" << (config_.enableGlobalCache ? "true" : "false") << "\n";
        file << "cacheTTL=" << config_.cacheTTL.count() << "\n";
        file << "maxCacheEntries=" << config_.maxCacheEntries << "\n";
        file << "enableBatchOperations=" << (config_.enableBatchOperations ? "true" : "false") << "\n";
        file << "batchSize=" << config_.batchSize << "\n";
        file << "enableAsyncOperations=" << (config_.enableAsyncOperations ? "true" : "false") << "\n";
        file << "maxAsyncTasks=" << config_.maxAsyncTasks << "\n";
        file << "enableAtomicWrites=" << (config_.enableAtomicWrites ? "true" : "false") << "\n";
        file << "enableBackups=" << (config_.enableBackups ? "true" : "false") << "\n";
        file << "maxBackups=" << config_.maxBackups << "\n";
        file << "enableEncryption=" << (config_.enableEncryption ? "true" : "false") << "\n";
        file << "enableValidation=" << (config_.enableValidation ? "true" : "false") << "\n";
        file << "strictValidation=" << (config_.strictValidation ? "true" : "false") << "\n";
        file << "enableMetrics=" << (config_.enableMetrics ? "true" : "false") << "\n";
        file << "enableEventLogging=" << (config_.enableEventLogging ? "true" : "false") << "\n";
        file << "enableMemoryPooling=" << (config_.enableMemoryPooling ? "true" : "false") << "\n";
        file << "initialPoolSize=" << config_.initialPoolSize << "\n";
        file << "maxPoolSize=" << config_.maxPoolSize << "\n";
        
        spdlog::info("Environment configuration saved to: {}", 
                    std::string(filePath.data(), filePath.size()));
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Error saving environment configuration to {}: {}", 
                     std::string(filePath.data(), filePath.size()), e.what());
        return false;
    }
}

void EnvConfigManager::updateMetrics(EnvEventType type, const String& key, 
                                    std::chrono::microseconds executionTime) {
    if (!config_.enableMetrics) {
        return;
    }
    
    metrics_.totalOperations++;
    metrics_.totalExecutionTime += executionTime.count();
    
    switch (type) {
        case EnvEventType::VARIABLE_GET:
            metrics_.getOperations++;
            break;
        case EnvEventType::VARIABLE_SET:
            metrics_.setOperations++;
            break;
        case EnvEventType::VARIABLE_UNSET:
            metrics_.unsetOperations++;
            break;
        case EnvEventType::BATCH_OPERATION:
            metrics_.batchOperations++;
            break;
        case EnvEventType::FILE_OPERATION:
            metrics_.fileOperations++;
            break;
        case EnvEventType::PATH_OPERATION:
            metrics_.pathOperations++;
            break;
        case EnvEventType::CACHE_HIT:
            metrics_.cacheHits++;
            break;
        case EnvEventType::CACHE_MISS:
            metrics_.cacheMisses++;
            break;
        case EnvEventType::ERROR_OCCURRED:
            metrics_.errors++;
            break;
        case EnvEventType::VALIDATION_FAILED:
            metrics_.validationFailures++;
            break;
    }
    
    // Emit event if enabled
    if (config_.enableEventLogging) {
        emitEvent(EnvEvent(type, key, "", ""));
    }
}

} // namespace atom::utils
