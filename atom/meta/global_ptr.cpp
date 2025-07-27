/*!
 * \file global_ptr.cpp
 * \brief Enhanced global shared pointer manager implementation - OPTIMIZED VERSION
 * \author Max Qian <lightapt.com>
 * \date 2023-06-17
 * \update 2024-03-11
 * \optimized 2025-01-22 - Performance optimizations by AI Assistant
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "global_ptr.hpp"

#if ATOM_ENABLE_DEBUG
#include <iostream>
#include <sstream>
#endif

#include <spdlog/spdlog.h>

auto GlobalSharedPtrManager::getInstance() -> GlobalSharedPtrManager& {
    static GlobalSharedPtrManager instance;
    spdlog::info("Retrieved GlobalSharedPtrManager instance");
    return instance;
}

void GlobalSharedPtrManager::removeSharedPtr(std::string_view key) {
    const std::string str_key{key};
    std::unique_lock lock(mutex_);

    const auto removed = pointer_map_.erase(str_key);

    if (removed > 0) {
        spdlog::info("Removed shared pointer with key: {}", str_key);
    }
}

size_t GlobalSharedPtrManager::removeExpiredWeakPtrs() {
    std::unique_lock lock(mutex_);
    size_t removed = 0;
    cleanup_batch_.clear();

    for (auto iter = pointer_map_.begin(); iter != pointer_map_.end();) {
        try {
            if (iter->second.metadata.flags.is_weak) {
                if (std::any_cast<std::weak_ptr<void>>(iter->second.ptr_data).expired()) {
                    spdlog::debug("Removing expired weak pointer with key: {}",
                                  iter->first);
                    iter->second.metadata.flags.is_expired = true;
                    cleanup_batch_.push_back(iter->first);
                    iter = pointer_map_.erase(iter);
                    ++removed;
                } else {
                    ++iter;
                }
            } else {
                ++iter;
            }
        } catch (const std::bad_any_cast&) {
            spdlog::warn("Bad any_cast for key: {}", iter->first);
            ++iter;
        }
    }

    if (removed > 0) {
        spdlog::info("Removed {} expired weak pointers", removed);
    }

    return removed;
}

size_t GlobalSharedPtrManager::cleanOldPointers(
    const std::chrono::seconds& older_than) {
    std::unique_lock lock(mutex_);
    size_t removed = 0;
    const auto now_micros = std::chrono::duration_cast<std::chrono::microseconds>(
        Clock::now().time_since_epoch()).count();
    const auto threshold_micros = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(older_than).count());

    cleanup_batch_.clear();

    for (auto iter = pointer_map_.begin(); iter != pointer_map_.end();) {
        if (now_micros - iter->second.metadata.creation_time_micros > threshold_micros) {
            cleanup_batch_.push_back(iter->first);
            iter = pointer_map_.erase(iter);
            ++removed;
        } else {
            ++iter;
        }
    }

    if (removed > 0) {
        spdlog::info("Cleaned {} old pointers", removed);
    }

    return removed;
}

void GlobalSharedPtrManager::clearAll() {
    std::unique_lock lock(mutex_);
    const auto ptr_count = pointer_map_.size();

    pointer_map_.clear();
    cleanup_batch_.clear();
    total_access_count_.store(0, std::memory_order_relaxed);

    spdlog::info("Cleared all {} shared pointers and metadata", ptr_count);
}

auto GlobalSharedPtrManager::size() const -> size_t {
    std::shared_lock lock(mutex_);
    const auto sz = pointer_map_.size();
    spdlog::debug("Current size of pointer_map_: {} (total accesses: {})",
                  sz, total_access_count_.load());
    return sz;
}

void GlobalSharedPtrManager::printSharedPtrMap() const {
    std::shared_lock lock(mutex_);

#if ATOM_ENABLE_DEBUG
    std::cout << "\n=== GlobalSharedPtrManager Status ===\n";
    std::cout << "Total pointers: " << pointer_map_.size() << "\n";
    std::cout << "Total accesses: " << total_access_count_.load() << "\n\n";

    for (const auto& [key, entry] : pointer_map_) {
        const auto& meta = entry.metadata;
        const auto now_micros = std::chrono::duration_cast<std::chrono::microseconds>(
            Clock::now().time_since_epoch()).count();
        const auto age_seconds = (now_micros - meta.creation_time_micros) / 1000000;

        std::cout << "Key: " << key << "\n"
                  << "  Type: " << meta.type_name << "\n"
                  << "  Access count: " << meta.access_count.load() << "\n"
                  << "  Reference count: " << meta.ref_count.load() << "\n"
                  << "  Age: " << age_seconds << "s\n"
                  << "  Is weak: " << (meta.flags.is_weak ? "yes" : "no") << "\n"
                  << "  Has custom deleter: "
                  << (meta.flags.has_custom_deleter ? "yes" : "no") << "\n\n";
    }
    std::cout << "==================================\n";
#endif

    spdlog::debug("Printed pointer_map_ contents ({} entries)",
                  pointer_map_.size());
}

auto GlobalSharedPtrManager::getPtrInfo(std::string_view key) const
    -> std::optional<PointerMetadata> {
    std::shared_lock lock(mutex_);

    if (const auto iter = pointer_map_.find(std::string(key));
        iter != pointer_map_.end()) {
        return iter->second.metadata;  // Copy constructor handles atomic members
    }
    return std::nullopt;
}

// New optimized methods implementation

auto GlobalSharedPtrManager::getStatistics() const -> Statistics {
    std::shared_lock lock(mutex_);
    Statistics stats;

    stats.total_pointers = pointer_map_.size();
    stats.total_accesses = total_access_count_.load();

    uint64_t total_access_count = 0;
    uint64_t total_age_micros = 0;
    const auto now_micros = std::chrono::duration_cast<std::chrono::microseconds>(
        Clock::now().time_since_epoch()).count();

    for (const auto& [key, entry] : pointer_map_) {
        if (entry.metadata.flags.is_weak) {
            ++stats.weak_pointers;
        }
        if (entry.metadata.flags.is_expired) {
            ++stats.expired_pointers;
        }
        total_access_count += entry.metadata.access_count.load();
        total_age_micros += (now_micros - entry.metadata.creation_time_micros);

        // Estimate memory usage
        stats.memory_usage_bytes += sizeof(PointerEntry) + key.size() +
                                   entry.metadata.type_name.size();
    }

    stats.average_access_count = stats.total_pointers > 0
        ? static_cast<double>(total_access_count) / stats.total_pointers
        : 0.0;

    stats.average_age = stats.total_pointers > 0
        ? std::chrono::milliseconds(total_age_micros / (stats.total_pointers * 1000))
        : std::chrono::milliseconds{0};

    return stats;
}

size_t GlobalSharedPtrManager::batchCleanupExpired() {
    std::unique_lock lock(mutex_);
    size_t removed = 0;
    cleanup_batch_.clear();

    // Collect expired entries in batches for better performance
    for (auto iter = pointer_map_.begin(); iter != pointer_map_.end();) {
        if (iter->second.metadata.flags.is_expired) {
            cleanup_batch_.push_back(iter->first);
            iter = pointer_map_.erase(iter);
            ++removed;

            // Process in batches to avoid holding lock too long
            if (cleanup_batch_.size() >= CLEANUP_BATCH_SIZE) {
                break;
            }
        } else {
            ++iter;
        }
    }

    return removed;
}

// Enhanced feature implementations

void GlobalSharedPtrManager::setCleanupPolicy(const CleanupPolicy& policy) {
    std::unique_lock lock(mutex_);
    cleanup_policy_ = policy;
    spdlog::info("Updated cleanup policy: max_age={}s, max_unused={}, auto_cleanup={}",
                 cleanup_policy_.max_age.count(),
                 cleanup_policy_.max_unused_count,
                 cleanup_policy_.auto_cleanup_enabled);
}

auto GlobalSharedPtrManager::getCleanupPolicy() const -> CleanupPolicy {
    std::shared_lock lock(mutex_);
    return cleanup_policy_;
}

void GlobalSharedPtrManager::setAutoCleanupEnabled(bool enabled) {
    std::unique_lock lock(mutex_);
    cleanup_policy_.auto_cleanup_enabled = enabled;
    if (enabled) {
        last_cleanup_time_ = std::chrono::steady_clock::now();
        spdlog::info("Automatic cleanup enabled");
    } else {
        spdlog::info("Automatic cleanup disabled");
    }
}

void GlobalSharedPtrManager::addDependency(std::string_view dependent_key,
                                          std::string_view dependency_key) {
    std::unique_lock lock(mutex_);
    const std::string dep_str{dependent_key};
    const std::string dependency_str{dependency_key};

    dependencies_[dep_str].push_back(dependency_str);
    spdlog::debug("Added dependency: {} depends on {}", dep_str, dependency_str);
}

void GlobalSharedPtrManager::removeDependency(std::string_view dependent_key,
                                             std::string_view dependency_key) {
    std::unique_lock lock(mutex_);
    const std::string dep_str{dependent_key};
    const std::string dependency_str{dependency_key};

    auto it = dependencies_.find(dep_str);
    if (it != dependencies_.end()) {
        auto& deps = it->second;
        deps.erase(std::remove(deps.begin(), deps.end(), dependency_str), deps.end());
        if (deps.empty()) {
            dependencies_.erase(it);
        }
        spdlog::debug("Removed dependency: {} no longer depends on {}", dep_str, dependency_str);
    }
}

auto GlobalSharedPtrManager::getDependencies(std::string_view key) const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    const std::string key_str{key};

    auto it = dependencies_.find(key_str);
    if (it != dependencies_.end()) {
        return it->second;
    }
    return {};
}

auto GlobalSharedPtrManager::isSafeToCleanup(std::string_view key) const -> bool {
    std::shared_lock lock(mutex_);
    const std::string key_str{key};

    // Check if any other pointer depends on this one
    for (const auto& [dependent, deps] : dependencies_) {
        if (std::find(deps.begin(), deps.end(), key_str) != deps.end()) {
            return false;  // Something depends on this pointer
        }
    }
    return true;
}
