/*
 * cache.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "cache.hpp"

#include <spdlog/spdlog.h>

namespace atom::system {

// ============================================================================
// CommandCacheManager Implementation
// ============================================================================

CommandCacheManager& CommandCacheManager::getInstance() {
    static CommandCacheManager instance;
    return instance;
}

CommandCacheManager::CommandCacheManager() {
    const auto& config = COMMAND_CONFIG();
    
    validationCache_ = std::make_unique<LRUCache<std::string, ValidationResult>>(
        config.maxCacheEntries, config.validationCacheTTL);
    
    metricsCache_ = std::make_unique<LRUCache<std::string, CommandMetrics>>(
        config.maxCacheEntries / 2, // Use half the cache size for metrics
        std::chrono::minutes(10)); // 10 minute TTL for metrics
    
    // Start cleanup thread
    cleanupThread_ = std::thread([this] { cleanupLoop(); });
    
    spdlog::info("CommandCacheManager initialized");
}

std::optional<ValidationResult> CommandCacheManager::getValidationResult(
    const std::string& command) {
    
    if (!COMMAND_CONFIG().enableValidationCache) {
        return std::nullopt;
    }
    
    return validationCache_->get(command);
}

void CommandCacheManager::cacheValidationResult(
    const std::string& command, const ValidationResult& result) {
    
    if (!COMMAND_CONFIG().enableValidationCache) {
        return;
    }
    
    validationCache_->put(command, result);
}

std::optional<CommandMetrics> CommandCacheManager::getCommandMetrics(
    const std::string& command) {
    
    return metricsCache_->get(command);
}

void CommandCacheManager::cacheCommandMetrics(
    const std::string& command, const CommandMetrics& metrics) {
    
    metricsCache_->put(command, metrics);
}

void CommandCacheManager::clearAll() {
    validationCache_->clear();
    metricsCache_->clear();
    spdlog::info("All command caches cleared");
}

CommandCacheManager::CacheStats CommandCacheManager::getStats() const {
    return {
        validationCache_->getStats(),
        metricsCache_->getStats()
    };
}

void CommandCacheManager::cleanup() {
    validationCache_->cleanup();
    metricsCache_->cleanup();
}

void CommandCacheManager::cleanupLoop() {
    while (!shutdown_.load()) {
        std::this_thread::sleep_for(std::chrono::minutes(5)); // Cleanup every 5 minutes
        
        if (!shutdown_.load()) {
            cleanup();
            spdlog::debug("Cache cleanup completed");
        }
    }
}

} // namespace atom::system
