/*
 * cron_cache.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "cron_cache.hpp"
#include "cron_thread_pool.hpp"

#include <spdlog/spdlog.h>

// ============================================================================
// CronCacheManager Implementation
// ============================================================================

CronCacheManager& CronCacheManager::getInstance() {
    static CronCacheManager instance;
    return instance;
}

CronCacheManager::CronCacheManager() {
    const auto& config = CRON_CONFIG();
    
    if (!config.enableCaching) {
        spdlog::info("Cron caching is disabled");
        return;
    }
    
    // Initialize caches with appropriate sizes
    size_t cacheSize = config.cacheSize;
    auto cacheTTL = config.cacheTTL;
    
    jobCache_ = std::make_unique<CronLRUCache<std::string, std::shared_ptr<CronJob>>>(
        cacheSize, cacheTTL);
    
    validationCache_ = std::make_unique<CronLRUCache<std::string, bool>>(
        cacheSize / 2, std::chrono::hours(1)); // Validation results can be cached longer
    
    executionCache_ = std::make_unique<CronLRUCache<std::string, CronExecutionResult>>(
        cacheSize / 4, std::chrono::minutes(10)); // Execution results are short-lived
    
    nextExecutionCache_ = std::make_unique<CronLRUCache<std::string, std::chrono::system_clock::time_point>>(
        cacheSize, std::chrono::minutes(5)); // Next execution times change frequently
    
    categoryCache_ = std::make_unique<CronLRUCache<std::string, std::vector<std::string>>>(
        cacheSize / 10, cacheTTL); // Category lists are less frequently accessed
    
    // Start cleanup thread
    cleanupThread_ = std::thread([this] { cleanupLoop(); });
    
    spdlog::info("Cron cache manager initialized with cache size: {}", cacheSize);
}

void CronCacheManager::cacheJob(const std::string& jobId, std::shared_ptr<CronJob> job) {
    if (!CRON_CONFIG().enableCaching || !jobCache_) {
        return;
    }
    
    jobCache_->put(jobId, job);
}

std::optional<std::shared_ptr<CronJob>> CronCacheManager::getCachedJob(const std::string& jobId) {
    if (!CRON_CONFIG().enableCaching || !jobCache_) {
        return std::nullopt;
    }
    
    return jobCache_->get(jobId);
}

void CronCacheManager::cacheValidationResult(const std::string& expression, bool isValid) {
    if (!CRON_CONFIG().enableCaching || !validationCache_) {
        return;
    }
    
    validationCache_->put(expression, isValid);
}

std::optional<bool> CronCacheManager::getCachedValidationResult(const std::string& expression) {
    if (!CRON_CONFIG().enableCaching || !validationCache_) {
        return std::nullopt;
    }
    
    return validationCache_->get(expression);
}

void CronCacheManager::cacheExecutionResult(const std::string& jobId, const CronExecutionResult& result) {
    if (!CRON_CONFIG().enableCaching || !executionCache_) {
        return;
    }
    
    executionCache_->put(jobId, result);
}

std::optional<CronExecutionResult> CronCacheManager::getCachedExecutionResult(const std::string& jobId) {
    if (!CRON_CONFIG().enableCaching || !executionCache_) {
        return std::nullopt;
    }
    
    return executionCache_->get(jobId);
}

void CronCacheManager::cacheNextExecution(const std::string& jobId, std::chrono::system_clock::time_point nextTime) {
    if (!CRON_CONFIG().enableCaching || !nextExecutionCache_) {
        return;
    }
    
    nextExecutionCache_->put(jobId, nextTime);
}

std::optional<std::chrono::system_clock::time_point> CronCacheManager::getCachedNextExecution(const std::string& jobId) {
    if (!CRON_CONFIG().enableCaching || !nextExecutionCache_) {
        return std::nullopt;
    }
    
    return nextExecutionCache_->get(jobId);
}

void CronCacheManager::cacheJobsByCategory(const std::string& category, const std::vector<std::string>& jobIds) {
    if (!CRON_CONFIG().enableCaching || !categoryCache_) {
        return;
    }
    
    categoryCache_->put(category, jobIds);
}

std::optional<std::vector<std::string>> CronCacheManager::getCachedJobsByCategory(const std::string& category) {
    if (!CRON_CONFIG().enableCaching || !categoryCache_) {
        return std::nullopt;
    }
    
    return categoryCache_->get(category);
}

void CronCacheManager::clearAll() {
    if (jobCache_) jobCache_->clear();
    if (validationCache_) validationCache_->clear();
    if (executionCache_) executionCache_->clear();
    if (nextExecutionCache_) nextExecutionCache_->clear();
    if (categoryCache_) categoryCache_->clear();
    
    spdlog::info("All cron caches cleared");
}

CronCacheManager::CacheStats CronCacheManager::getStats() const {
    if (!CRON_CONFIG().enableCaching) {
        return {};
    }
    
    return {
        jobCache_ ? jobCache_->getStats() : CronLRUCache<std::string, std::shared_ptr<CronJob>>::Stats{},
        validationCache_ ? validationCache_->getStats() : CronLRUCache<std::string, bool>::Stats{},
        executionCache_ ? executionCache_->getStats() : CronLRUCache<std::string, CronExecutionResult>::Stats{},
        nextExecutionCache_ ? nextExecutionCache_->getStats() : CronLRUCache<std::string, std::chrono::system_clock::time_point>::Stats{},
        categoryCache_ ? categoryCache_->getStats() : CronLRUCache<std::string, std::vector<std::string>>::Stats{}
    };
}

size_t CronCacheManager::cleanup() {
    if (!CRON_CONFIG().enableCaching) {
        return 0;
    }
    
    size_t totalRemoved = 0;
    
    if (jobCache_) totalRemoved += jobCache_->cleanup();
    if (validationCache_) totalRemoved += validationCache_->cleanup();
    if (executionCache_) totalRemoved += executionCache_->cleanup();
    if (nextExecutionCache_) totalRemoved += nextExecutionCache_->cleanup();
    if (categoryCache_) totalRemoved += categoryCache_->cleanup();
    
    if (totalRemoved > 0) {
        spdlog::debug("Cleaned up {} expired cache entries", totalRemoved);
    }
    
    return totalRemoved;
}

void CronCacheManager::invalidateJob(const std::string& jobId) {
    if (!CRON_CONFIG().enableCaching) {
        return;
    }
    
    if (jobCache_) jobCache_->remove(jobId);
    if (executionCache_) executionCache_->remove(jobId);
    if (nextExecutionCache_) nextExecutionCache_->remove(jobId);
    
    spdlog::debug("Invalidated cache entries for job: {}", jobId);
}

void CronCacheManager::invalidateCategory(const std::string& category) {
    if (!CRON_CONFIG().enableCaching) {
        return;
    }
    
    if (categoryCache_) categoryCache_->remove(category);
    
    spdlog::debug("Invalidated cache entries for category: {}", category);
}

void CronCacheManager::cleanupLoop() {
    while (!shutdown_.load()) {
        std::this_thread::sleep_for(std::chrono::minutes(5)); // Cleanup every 5 minutes
        
        if (!shutdown_.load()) {
            cleanup();
        }
    }
}
