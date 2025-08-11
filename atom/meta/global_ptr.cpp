/*!
 * \file global_ptr.cpp
 * \brief Enhanced global shared pointer manager implementation
 * \author Max Qian <lightapt.com>
 * \date 2023-06-17
 * \update 2024-03-11
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

    const auto removed_ptr = shared_ptr_map_.erase(str_key);
    const auto removed_meta = metadata_map_.erase(str_key);

    if (removed_ptr > 0 || removed_meta > 0) {
        spdlog::info("Removed shared pointer with key: {}", str_key);
    }
}

size_t GlobalSharedPtrManager::removeExpiredWeakPtrs() {
    std::unique_lock lock(mutex_);
    size_t removed = 0;
    expired_keys_.clear();

    for (auto iter = shared_ptr_map_.begin(); iter != shared_ptr_map_.end();) {
        try {
            if (std::any_cast<std::weak_ptr<void>>(iter->second).expired()) {
                spdlog::debug("Removing expired weak pointer with key: {}",
                              iter->first);
                expired_keys_.insert(iter->first);
                iter = shared_ptr_map_.erase(iter);
                ++removed;
            } else {
                ++iter;
            }
        } catch (const std::bad_any_cast&) {
            spdlog::warn("Bad any_cast for key: {}", iter->first);
            ++iter;
        }
    }

    for (const auto& key : expired_keys_) {
        metadata_map_.erase(key);
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
    const auto now = Clock::now();
    expired_keys_.clear();

    for (auto iter = metadata_map_.begin(); iter != metadata_map_.end();) {
        if (now - iter->second.creation_time > older_than) {
            expired_keys_.insert(iter->first);
            iter = metadata_map_.erase(iter);
            ++removed;
        } else {
            ++iter;
        }
    }

    for (const auto& key : expired_keys_) {
        shared_ptr_map_.erase(key);
    }

    if (removed > 0) {
        spdlog::info("Cleaned {} old pointers", removed);
    }

    return removed;
}

void GlobalSharedPtrManager::clearAll() {
    std::unique_lock lock(mutex_);
    const auto ptr_count = shared_ptr_map_.size();

    shared_ptr_map_.clear();
    metadata_map_.clear();
    total_access_count_ = 0;

    spdlog::info("Cleared all {} shared pointers and metadata", ptr_count);
}

auto GlobalSharedPtrManager::size() const -> size_t {
    std::shared_lock lock(mutex_);
    const auto sz = shared_ptr_map_.size();
    spdlog::debug("Current size of shared_ptr_map_: {} (total accesses: {})",
                  sz, total_access_count_.load());
    return sz;
}

void GlobalSharedPtrManager::printSharedPtrMap() const {
    std::shared_lock lock(mutex_);

#if ATOM_ENABLE_DEBUG
    std::cout << "\n=== GlobalSharedPtrManager Status ===\n";
    std::cout << "Total pointers: " << shared_ptr_map_.size() << "\n";
    std::cout << "Total accesses: " << total_access_count_ << "\n\n";

    for (const auto& [key, meta] : metadata_map_) {
        const auto age_seconds =
            std::chrono::duration_cast<std::chrono::seconds>(Clock::now() -
                                                             meta.creation_time)
                .count();

        std::cout << "Key: " << key << "\n"
                  << "  Type: " << meta.type_name << "\n"
                  << "  Access count: " << meta.access_count << "\n"
                  << "  Reference count: " << meta.ref_count << "\n"
                  << "  Age: " << age_seconds << "s\n"
                  << "  Is weak: " << (meta.is_weak ? "yes" : "no") << "\n"
                  << "  Has custom deleter: "
                  << (meta.has_custom_deleter ? "yes" : "no") << "\n\n";
    }
    std::cout << "==================================\n";
#endif

    spdlog::debug("Printed shared_ptr_map_ contents ({} entries)",
                  shared_ptr_map_.size());
}

auto GlobalSharedPtrManager::getPtrInfo(std::string_view key) const
    -> std::optional<PointerMetadata> {
    std::shared_lock lock(mutex_);

    if (const auto iter = metadata_map_.find(std::string(key));
        iter != metadata_map_.end()) {
        return iter->second;
    }
    return std::nullopt;
}

void GlobalSharedPtrManager::updateMetadata(std::string_view key,
                                            const std::string& type_name,
                                            bool is_weak, bool has_deleter) {
    const std::string str_key{key};
    auto& meta = metadata_map_[str_key];

    meta.creation_time = Clock::now();
    meta.type_name = type_name;
    meta.is_weak = is_weak;
    meta.has_custom_deleter = has_deleter;
    ++meta.access_count;

    if (const auto iter = shared_ptr_map_.find(str_key);
        iter != shared_ptr_map_.end()) {
        try {
            if (is_weak) {
                meta.ref_count =
                    std::any_cast<std::weak_ptr<void>>(iter->second)
                        .use_count();
            } else {
                meta.ref_count =
                    std::any_cast<std::shared_ptr<void>>(iter->second)
                        .use_count();
            }
        } catch (const std::bad_any_cast&) {
            // Ignore type errors in ref counting
        }
    }
}
