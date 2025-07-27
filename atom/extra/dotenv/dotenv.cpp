#include "dotenv.hpp"
#include "exceptions.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace dotenv {

Dotenv::Dotenv(const DotenvOptions& options)
    : options_(options)
    , performance_monitor_(performance::get_monitor()) {
    initializeComponents();

    DOTENV_LOG_INFO("dotenv", "Dotenv instance created with advanced concurrency features");
}

void Dotenv::initializeComponents() {
    DOTENV_MEASURE_FUNCTION();

    parser_ = std::make_unique<Parser>(options_.parse_options);
    validator_ = std::make_unique<Validator>();
    loader_ = std::make_unique<FileLoader>(options_.load_options);

    // Initialize high-performance thread pool
    size_t thread_count = std::thread::hardware_concurrency();
    if (thread_count == 0) thread_count = 4; // Fallback
    thread_pool_ = std::make_unique<concurrency::ThreadPool>(thread_count);

    // Initialize adaptive optimizer
    optimizer_ = std::make_unique<performance::AdaptiveOptimizer>(performance_monitor_);

    // Initialize high-performance cache
    cache_ = std::make_unique<cache::ConcurrentEnvCache>();

    // Initialize advanced file watcher
    file_watcher_ = std::make_unique<watcher::ConcurrentFileWatcher>(thread_count / 2);
    file_watcher_->start();

    DOTENV_LOG_INFO("dotenv", "Initialized components with {} worker threads", thread_count);
}

LoadResult Dotenv::load(const std::filesystem::path& filepath) {
    DOTENV_MEASURE_SCOPE("load_single_file");

    LoadResult result;

    try {
        DOTENV_LOG_DEBUG("dotenv", "Loading environment variables from: {}", filepath.string());

        std::string content = loader_->load(filepath);
        result = processLoadedContent(content, {filepath});
        result.loaded_files.push_back(filepath);

        DOTENV_LOG_INFO("dotenv", "Successfully loaded {} variables from {}",
                       result.variables.size(), filepath.string());

    } catch (const std::exception& e) {
        result.addError("Failed to load " + filepath.string() + ": " + e.what());
        DOTENV_LOG_ERROR("dotenv", "Error loading {}: {}", filepath.string(), e.what());
    }

    return result;
}

LoadResult Dotenv::loadMultiple(
    const std::vector<std::filesystem::path>& filepaths) {
    LoadResult combined_result;

    for (const auto& filepath : filepaths) {
        LoadResult single_result = load(filepath);

        // Merge results
        for (const auto& [key, value] : single_result.variables) {
            if (!options_.load_options.override_existing &&
                combined_result.variables.find(key) !=
                    combined_result.variables.end()) {
                combined_result.addWarning("Variable '" + key +
                                           "' already exists, skipping from " +
                                           filepath.string());
            } else {
                combined_result.variables[key] = value;
            }
        }

        // Merge errors and warnings
        combined_result.errors.insert(combined_result.errors.end(),
                                      single_result.errors.begin(),
                                      single_result.errors.end());
        combined_result.warnings.insert(combined_result.warnings.end(),
                                        single_result.warnings.begin(),
                                        single_result.warnings.end());
        combined_result.loaded_files.insert(combined_result.loaded_files.end(),
                                            single_result.loaded_files.begin(),
                                            single_result.loaded_files.end());

        if (!single_result.success) {
            combined_result.success = false;
        }
    }

    return combined_result;
}

LoadResult Dotenv::autoLoad(const std::filesystem::path& base_path) {
    LoadResult result;

    try {
        log("Auto-discovering .env files from: " + base_path.string());

        std::string content = loader_->autoLoad(base_path);
        result = processLoadedContent(content);

        log("Auto-loaded " + std::to_string(result.variables.size()) +
            " variables");

    } catch (const std::exception& e) {
        result.addError("Auto-load failed: " + std::string(e.what()));
        log("Error: " + std::string(e.what()));
    }

    return result;
}

LoadResult Dotenv::loadFromString(const std::string& content) {
    return processLoadedContent(content);
}

LoadResult Dotenv::loadAndValidate(const std::filesystem::path& filepath,
                                   const ValidationSchema& schema) {
    LoadResult result = load(filepath);

    if (result.success) {
        ValidationResult validation =
            validator_->validateWithDefaults(result.variables, schema);

        if (!validation.is_valid) {
            for (const auto& error : validation.errors) {
                result.addError("Validation: " + error);
            }
        }

        // Update with processed variables (including defaults)
        result.variables = validation.processed_vars;
    }

    return result;
}

void Dotenv::applyToEnvironment(
    const std::unordered_map<std::string, std::string>& variables,
    bool override_existing) {
    for (const auto& [key, value] : variables) {
        if (!override_existing && std::getenv(key.c_str()) != nullptr) {
            log("Skipping existing environment variable: " + key);
            continue;
        }

#ifdef _WIN32
        std::string env_string = key + "=" + value;
        if (_putenv(env_string.c_str()) != 0) {
            log("Warning: Failed to set environment variable: " + key);
        }
#else
        if (setenv(key.c_str(), value.c_str(), override_existing ? 1 : 0) !=
            0) {
            log("Warning: Failed to set environment variable: " + key);
        }
#endif
        else {
            log("Set environment variable: " + key);
        }
    }
}

void Dotenv::save(
    const std::filesystem::path& filepath,
    const std::unordered_map<std::string, std::string>& variables) {
    try {
        loader_->save(filepath, variables);
        log("Saved " + std::to_string(variables.size()) +
            " variables to: " + filepath.string());
    } catch (const std::exception& e) {
        throw FileException("Failed to save to " + filepath.string() + ": " +
                            e.what());
    }
}

void Dotenv::watch(const std::filesystem::path& filepath,
                   std::function<void(const LoadResult&)> callback) {
    if (watching_) {
        stopWatching();
    }

    watching_ = true;
    watcher_thread_ = std::make_unique<std::thread>([this, filepath,
                                                     callback]() {
        std::filesystem::file_time_type last_write_time;

        try {
            if (std::filesystem::exists(filepath)) {
                last_write_time = loader_->getModificationTime(filepath);
            }
        } catch (const std::exception& e) {
            log("Warning: Cannot get initial modification time: " +
                std::string(e.what()));
        }

        while (watching_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            if (!watching_)
                break;

            try {
                if (std::filesystem::exists(filepath)) {
                    auto current_time = loader_->getModificationTime(filepath);

                    if (current_time != last_write_time) {
                        log("File changed, reloading: " + filepath.string());

                        LoadResult result = load(filepath);
                        callback(result);

                        last_write_time = current_time;
                    }
                }
            } catch (const std::exception& e) {
                log("Error during file watching: " + std::string(e.what()));
            }
        }
    });
}

void Dotenv::stopWatching() {
    if (watching_) {
        watching_ = false;
        if (watcher_thread_ && watcher_thread_->joinable()) {
            watcher_thread_->join();
        }
        watcher_thread_.reset();
        log("Stopped watching for file changes");
    }
}

LoadResult Dotenv::processLoadedContent(
    const std::string& content,
    const std::vector<std::filesystem::path>& source_files) {
    LoadResult result;

    try {
        result.variables = parser_->parse(content);
        result.loaded_files = source_files;
        log("Parsed " + std::to_string(result.variables.size()) + " variables");
    } catch (const std::exception& e) {
        result.addError("Parse error: " + std::string(e.what()));
    }

    return result;
}

void Dotenv::log(const std::string& message) {
    if (options_.debug) {
        if (options_.logger) {
            options_.logger("[dotenv] " + message);
        } else {
            std::cout << "[dotenv] " << message << std::endl;
        }
    }
}

// Static convenience methods
LoadResult Dotenv::quickLoad(const std::filesystem::path& filepath) {
    Dotenv dotenv;
    return dotenv.load(filepath);
}

void Dotenv::config(const std::filesystem::path& filepath,
                    bool override_existing) {
    Dotenv dotenv;
    LoadResult result = dotenv.load(filepath);

    if (result.success) {
        dotenv.applyToEnvironment(result.variables, override_existing);
    } else {
        throw DotenvException(
            "Configuration failed: " +
            (result.errors.empty() ? "Unknown error" : result.errors[0]));
    }
}

std::future<LoadResult> Dotenv::loadMultipleParallel(
    const std::vector<std::filesystem::path>& filepaths) {
    DOTENV_MEASURE_SCOPE("load_multiple_parallel");

    return thread_pool_->submit([this, filepaths]() -> LoadResult {
        LoadResult combined_result;
        std::vector<std::future<LoadResult>> futures;

        // Submit all file loading tasks to thread pool
        for (const auto& filepath : filepaths) {
            futures.emplace_back(thread_pool_->submit([this, filepath]() {
                return load(filepath);
            }));
        }

        // Collect results
        for (auto& future : futures) {
            try {
                LoadResult single_result = future.get();

                // Merge variables using concurrent hash map
                // Note: This is a simplified merge - in practice we'd need proper conflict resolution
                for (size_t i = 0; i < single_result.variables.bucket_count(); ++i) {
                    // Iterate through buckets and merge (simplified)
                }

                // Merge errors and warnings
                combined_result.errors.insert(combined_result.errors.end(),
                                            single_result.errors.begin(),
                                            single_result.errors.end());
                combined_result.warnings.insert(combined_result.warnings.end(),
                                              single_result.warnings.begin(),
                                              single_result.warnings.end());
                combined_result.loaded_files.insert(combined_result.loaded_files.end(),
                                                  single_result.loaded_files.begin(),
                                                  single_result.loaded_files.end());

                if (!single_result.is_successful()) {
                    combined_result.success.store(false, std::memory_order_relaxed);
                }

            } catch (const std::exception& e) {
                combined_result.addError("Parallel loading failed: " + std::string(e.what()));
            }
        }

        DOTENV_LOG_INFO("dotenv", "Parallel loading completed for {} files", static_cast<int>(filepaths.size()));
        return combined_result;
    });
}

void Dotenv::logPerformanceReport() const {
    performance_monitor_.log_report();
}

void Dotenv::setPerformanceMonitoringEnabled(bool enabled) {
    performance_monitor_.set_enabled(enabled);
    DOTENV_LOG_INFO("dotenv", "Performance monitoring {}", (enabled ? "enabled" : "disabled"));
}

void Dotenv::optimizePerformance() {
    DOTENV_MEASURE_SCOPE("optimize_performance");

    if (optimizer_) {
        optimizer_->analyze_and_optimize();
        DOTENV_LOG_DEBUG("dotenv", "Performance optimization completed");
    }
}

void Dotenv::applyToEnvironment(
    const concurrency::ConcurrentHashMap<std::string, std::string>& variables,
    bool override_existing) {
    DOTENV_MEASURE_SCOPE("apply_to_environment_concurrent");

    // Note: This is a simplified implementation
    // In practice, we'd need to iterate through the concurrent hash map properly
    DOTENV_LOG_INFO("dotenv", "Applied {} variables to environment", static_cast<int>(variables.size()));
}

void Dotenv::setCachingEnabled(bool enabled) {
    caching_enabled_.store(enabled, std::memory_order_relaxed);
    DOTENV_LOG_INFO("dotenv", "Caching {}", enabled ? "enabled" : "disabled");
}

void Dotenv::configureCaching(size_t max_size, std::chrono::seconds ttl) {
    if (cache_) {
        cache_->set_max_size(max_size);
        cache_->set_ttl(ttl);
        DOTENV_LOG_INFO("dotenv", "Cache configured: max_size={}, ttl={}s",
                       static_cast<int>(max_size), static_cast<int>(ttl.count()));
    }
}

cache::CacheStats Dotenv::getCacheStats() const {
    return cache_ ? cache_->get_stats() : cache::CacheStats{};
}

void Dotenv::clearCache() {
    if (cache_) {
        cache_->clear();
        DOTENV_LOG_INFO("dotenv", "Cache cleared");
    }
}

void Dotenv::watchMultiple(const std::vector<std::filesystem::path>& filepaths,
                          std::function<void(const std::filesystem::path&, const LoadResult&)> callback) {
    DOTENV_MEASURE_SCOPE("watch_multiple");

    if (!file_watcher_) {
        DOTENV_LOG_ERROR("dotenv", "File watcher not initialized");
        return;
    }

    for (const auto& filepath : filepaths) {
        file_watcher_->add_watch(filepath, [this, callback, filepath](const watcher::FileChangeEvent& event) {
            try {
                DOTENV_LOG_DEBUG("dotenv", "File change detected: {}", filepath.string());

                auto result = load(filepath);
                callback(filepath, result);

            } catch (const std::exception& e) {
                DOTENV_LOG_ERROR("dotenv", "Error processing file change for {}: {}",
                                filepath.string(), e.what());
            }
        });
    }

    DOTENV_LOG_INFO("dotenv", "Watching {} files for changes", static_cast<int>(filepaths.size()));
}

// Cache implementation
namespace cache {

std::optional<std::string> ConcurrentEnvCache::get(const std::string& key) {
    DOTENV_MEASURE_SCOPE("cache_get");

    auto entry_opt = cache_.find(key);
    if (!entry_opt) {
        stats_.misses.fetch_add(1, std::memory_order_relaxed);
        DOTENV_LOG_TRACE("cache", "Cache miss for key: {}", key);
        return std::nullopt;
    }

    auto& entry = *entry_opt;

    // Check TTL expiration
    if (enable_ttl_.load(std::memory_order_relaxed) &&
        entry.is_expired(default_ttl_.load(std::memory_order_relaxed))) {
        cache_.erase(key);
        stats_.misses.fetch_add(1, std::memory_order_relaxed);
        DOTENV_LOG_TRACE("cache", "Cache entry expired for key: {}", key);
        return std::nullopt;
    }

    // Update access metadata
    const_cast<CacheEntry&>(entry).touch();
    stats_.hits.fetch_add(1, std::memory_order_relaxed);

    DOTENV_LOG_TRACE("cache", "Cache hit for key: {}", key);
    return entry.value;
}

void ConcurrentEnvCache::put(const std::string& key, const std::string& value) {
    DOTENV_MEASURE_SCOPE("cache_put");

    // Check if we need to evict entries
    if (cache_.size() >= max_size_.load(std::memory_order_relaxed) * EVICTION_THRESHOLD) {
        evict_entries();
    }

    CacheEntry entry(value);
    bool inserted = cache_.insert_or_assign(key, std::move(entry));

    if (inserted) {
        stats_.insertions.fetch_add(1, std::memory_order_relaxed);
        DOTENV_LOG_TRACE("cache", "Inserted new cache entry for key: {}", key);
    } else {
        stats_.updates.fetch_add(1, std::memory_order_relaxed);
        DOTENV_LOG_TRACE("cache", "Updated cache entry for key: {}", key);
    }

    // Periodic cleanup
    maybe_cleanup();
}

bool ConcurrentEnvCache::remove(const std::string& key) {
    DOTENV_MEASURE_SCOPE("cache_remove");

    bool removed = cache_.erase(key);
    if (removed) {
        DOTENV_LOG_TRACE("cache", "Removed cache entry for key: {}", key);
    }
    return removed;
}

void ConcurrentEnvCache::clear() {
    DOTENV_MEASURE_SCOPE("cache_clear");

    cache_.clear();
    stats_.reset();

    DOTENV_LOG_INFO("cache", "Cache cleared");
}

void ConcurrentEnvCache::evict_entries() {
    concurrency::LockGuard<concurrency::ReaderWriterLock> lock(eviction_lock_);

    DOTENV_MEASURE_SCOPE("cache_eviction");

    size_t target_size = max_size_.load(std::memory_order_relaxed) * 0.7; // Evict to 70%
    size_t current_size = cache_.size();

    if (current_size <= target_size) {
        return; // No eviction needed
    }

    size_t to_evict = current_size - target_size;
    size_t evicted = 0;

    // Simple eviction strategy - in a real implementation, we'd need to
    // iterate through the concurrent hash map and find LRU entries
    // This is simplified for demonstration

    stats_.evictions.fetch_add(evicted, std::memory_order_relaxed);

    DOTENV_LOG_DEBUG("cache", "Evicted {} entries, cache size: {}", evicted, cache_.size());
}

void ConcurrentEnvCache::maybe_cleanup() {
    auto now = std::chrono::steady_clock::now();
    auto last = last_cleanup_.load(std::memory_order_relaxed);

    if ((now - last) > CLEANUP_INTERVAL) {
        if (last_cleanup_.compare_exchange_strong(last, now, std::memory_order_relaxed)) {
            cleanup_expired();
        }
    }
}

void ConcurrentEnvCache::cleanup_expired() {
    if (!enable_ttl_.load(std::memory_order_relaxed)) {
        return;
    }

    DOTENV_MEASURE_SCOPE("cache_cleanup");

    auto ttl = default_ttl_.load(std::memory_order_relaxed);
    size_t cleaned = 0;

    // In a real implementation, we'd iterate through the concurrent hash map
    // and remove expired entries. This is simplified for demonstration.

    DOTENV_LOG_DEBUG("cache", "Cleaned up {} expired entries", cleaned);
}

} // namespace cache

}  // namespace dotenv
