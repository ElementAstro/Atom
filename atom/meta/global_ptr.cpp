/*!
 * \file global_ptr.cpp
 * \brief Enhanced global shared pointer manager implementation
 * \author Max Qian <lightapt.com>
 * \date 2023-06-17
 * \update 2024-03-11
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "global_ptr.hpp"

#include <algorithm>

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

    for (auto iter = pointer_map_.begin(); iter != pointer_map_.end();) {
        if (iter->second.metadata.flags.is_weak && iter->second.expired_check &&
            iter->second.expired_check()) {
            spdlog::debug("Removing expired weak pointer with key: {}",
                          iter->first);
            iter = pointer_map_.erase(iter);
            ++removed;
        } else {
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
    const auto now_micros =
        std::chrono::duration_cast<std::chrono::microseconds>(
            Clock::now().time_since_epoch())
            .count();
    const auto older_than_micros =
        std::chrono::duration_cast<std::chrono::microseconds>(older_than)
            .count();

    // Grace period: entries created or accessed within the last 50ms are
    // never considered "old", even with a zero threshold. This prevents
    // removing pointers that are actively in use.
    static constexpr int64_t MIN_IDLE_MICROS = 50'000;
    const auto idle_threshold_micros =
        std::max<int64_t>(older_than_micros, MIN_IDLE_MICROS);

    for (auto iter = pointer_map_.begin(); iter != pointer_map_.end();) {
        const auto last_access_micros = static_cast<int64_t>(
            iter->second.metadata.last_access_micros.load(
                std::memory_order_relaxed));
        if (now_micros - last_access_micros > idle_threshold_micros) {
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
    total_access_count_ = 0;

    spdlog::info("Cleared all {} shared pointers and metadata", ptr_count);
}

auto GlobalSharedPtrManager::size() const -> size_t {
    std::shared_lock lock(mutex_);
    const auto sz = pointer_map_.size();
    spdlog::debug("Current size of pointer_map_: {} (total accesses: {})", sz,
                  total_access_count_.load());
    return sz;
}

void GlobalSharedPtrManager::printSharedPtrMap() const {
    std::shared_lock lock(mutex_);

#if ATOM_ENABLE_DEBUG
    std::cout << "\n=== GlobalSharedPtrManager Status ===\n";
    std::cout << "Total pointers: " << pointer_map_.size() << "\n";
    std::cout << "Total accesses: " << total_access_count_ << "\n\n";

    for (const auto& [key, entry] : pointer_map_) {
        const auto now_micros =
            std::chrono::duration_cast<std::chrono::microseconds>(
                Clock::now().time_since_epoch())
                .count();
        const auto age_seconds =
            (now_micros - entry.metadata.creation_time_micros) / 1000000;

        std::cout << "Key: " << key << "\n"
                  << "  Type: " << entry.metadata.type_name << "\n"
                  << "  Access count: " << entry.metadata.access_count << "\n"
                  << "  Reference count: " << entry.metadata.ref_count << "\n"
                  << "  Age: " << age_seconds << "s\n"
                  << "  Is weak: "
                  << (entry.metadata.flags.is_weak ? "yes" : "no") << "\n"
                  << "  Has custom deleter: "
                  << (entry.metadata.flags.has_custom_deleter ? "yes" : "no")
                  << "\n\n";
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
        const auto& entry = iter->second;
        // Querying metadata counts as an access; counters are mutable atomics.
        entry.metadata.recordAccess();
        if (entry.use_count_fn) {
            // Refresh the reference count from the live ownership group.
            entry.metadata.ref_count.store(
                static_cast<uint32_t>(entry.use_count_fn()),
                std::memory_order_relaxed);
        }
        return entry.metadata;
    }
    return std::nullopt;
}

void GlobalSharedPtrManager::markCustomDeleter(std::string_view key) {
    std::unique_lock lock(mutex_);

    if (const auto iter = pointer_map_.find(std::string(key));
        iter != pointer_map_.end()) {
        iter->second.metadata.flags.has_custom_deleter = true;
    }
}
