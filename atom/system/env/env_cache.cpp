/*
 * env_cache.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "env_cache.hpp"

#include <regex>
#include <spdlog/spdlog.h>

namespace atom::utils {

// ============================================================================
// EnvCacheManager Implementation
// ============================================================================

EnvCacheManager& EnvCacheManager::getInstance() {
    static EnvCacheManager instance;
    return instance;
}

EnvCacheManager::EnvCacheManager() {
    const auto& config = ENV_CONFIG();
    
    envVarCache_ = std::make_unique<EnvLRUCache<String, String>>(
        config.maxCacheEntries, config.cacheTTL);
    
    pathCache_ = std::make_unique<EnvLRUCache<String, std::vector<String>>>(
        config.maxCacheEntries / 10, // Use 1/10th for path cache
        config.pathCacheTTL);
    
    systemInfoCache_ = std::make_unique<EnvLRUCache<String, String>>(
        config.maxCacheEntries / 5, // Use 1/5th for system info cache
        std::chrono::minutes(10)); // 10 minute TTL for system info
    
    // Start cleanup thread
    cleanupThread_ = std::thread([this] { cleanupLoop(); });
    
    spdlog::info("Environment cache manager initialized");
}

std::optional<String> EnvCacheManager::getEnvVar(const String& key) {
    if (!ENV_CONFIG().enableGlobalCache) {
        return std::nullopt;
    }
    
    return envVarCache_->get(key);
}

void EnvCacheManager::cacheEnvVar(const String& key, const String& value) {
    if (!ENV_CONFIG().enableGlobalCache) {
        return;
    }
    
    envVarCache_->put(key, value);
}

std::optional<std::vector<String>> EnvCacheManager::getPathEntries() {
    if (!ENV_CONFIG().enablePathCache) {
        return std::nullopt;
    }
    
    return pathCache_->get("PATH_ENTRIES");
}

void EnvCacheManager::cachePathEntries(const std::vector<String>& paths) {
    if (!ENV_CONFIG().enablePathCache) {
        return;
    }
    
    pathCache_->put("PATH_ENTRIES", paths);
}

template<typename T>
std::optional<T> EnvCacheManager::getSystemInfo(const String& key) {
    if (!ENV_CONFIG().enableGlobalCache) {
        return std::nullopt;
    }
    
    auto result = systemInfoCache_->get(key);
    if (result) {
        // This is a simplified implementation - in practice, you'd need
        // proper serialization/deserialization for complex types
        return T(*result);
    }
    return std::nullopt;
}

template<typename T>
void EnvCacheManager::cacheSystemInfo(const String& key, const T& value) {
    if (!ENV_CONFIG().enableGlobalCache) {
        return;
    }
    
    // This is a simplified implementation - in practice, you'd need
    // proper serialization/deserialization for complex types
    systemInfoCache_->put(key, String(value));
}

void EnvCacheManager::clearAll() {
    envVarCache_->clear();
    pathCache_->clear();
    systemInfoCache_->clear();
    spdlog::info("All environment caches cleared");
}

EnvCacheManager::CacheStats EnvCacheManager::getStats() const {
    return {
        envVarCache_->getStats(),
        pathCache_->getStats(),
        systemInfoCache_->getStats()
    };
}

size_t EnvCacheManager::cleanup() {
    size_t totalRemoved = 0;
    totalRemoved += envVarCache_->cleanup();
    totalRemoved += pathCache_->cleanup();
    totalRemoved += systemInfoCache_->cleanup();
    
    if (totalRemoved > 0) {
        spdlog::debug("Cleaned up {} expired cache entries", totalRemoved);
    }
    
    return totalRemoved;
}

void EnvCacheManager::invalidatePattern(const String& pattern) {
    try {
        std::regex regex(std::string(pattern.data(), pattern.size()));
        
        // Get all keys and check which ones match the pattern
        auto envKeys = envVarCache_->getKeysByFrequency();
        for (const auto& key : envKeys) {
            if (std::regex_match(std::string(key.data(), key.size()), regex)) {
                envVarCache_->remove(key);
            }
        }
        
        spdlog::debug("Invalidated cache entries matching pattern: {}", 
                     std::string(pattern.data(), pattern.size()));
        
    } catch (const std::exception& e) {
        spdlog::error("Error invalidating cache pattern '{}': {}", 
                     std::string(pattern.data(), pattern.size()), e.what());
    }
}

void EnvCacheManager::cleanupLoop() {
    while (!shutdown_.load()) {
        std::this_thread::sleep_for(std::chrono::minutes(5)); // Cleanup every 5 minutes
        
        if (!shutdown_.load()) {
            cleanup();
        }
    }
}

// Explicit template instantiations for common types
template std::optional<String> EnvCacheManager::getSystemInfo<String>(const String& key);
template void EnvCacheManager::cacheSystemInfo<String>(const String& key, const String& value);

template std::optional<int> EnvCacheManager::getSystemInfo<int>(const String& key);
template void EnvCacheManager::cacheSystemInfo<int>(const String& key, const int& value);

template std::optional<bool> EnvCacheManager::getSystemInfo<bool>(const String& key);
template void EnvCacheManager::cacheSystemInfo<bool>(const String& key, const bool& value);

} // namespace atom::utils
