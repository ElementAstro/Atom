/*!
 * \file global_ptr.cpp
 * \brief Global shared pointer manager
 * \author Max Qian <lightapt.com>
 * \date 2023-06-17
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "global_ptr.hpp"

#if ATOM_ENABLE_DEBUG
#include <iostream>
#include <sstream>
#endif

#include "atom/log/loguru.hpp"

auto GlobalSharedPtrManager::getInstance() -> GlobalSharedPtrManager & {
    static GlobalSharedPtrManager instance;
    LOG_F(INFO, "Get GlobalSharedPtrManager instance");
    return instance;
}

void GlobalSharedPtrManager::removeSharedPtr(const std::string &key) {
    std::unique_lock lock(mutex_);
    shared_ptr_map_.erase(key);
    LOG_F(INFO, "Removed shared pointer with key: {}", key);
}

void GlobalSharedPtrManager::removeExpiredWeakPtrs() {
    std::unique_lock lock(mutex_);
    auto iter = shared_ptr_map_.begin();
    while (iter != shared_ptr_map_.end()) {
        try {
            if (std::any_cast<std::weak_ptr<void>>(iter->second).expired()) {
                LOG_F(INFO, "Removing expired weak pointer with key: {}",
                      iter->first);
                iter = shared_ptr_map_.erase(iter);
            } else {
                ++iter;
            }
        } catch (const std::bad_any_cast &) {
            LOG_F(WARNING, "Bad any_cast encountered for key: {}", iter->first);
            ++iter;
        }
    }
}

void GlobalSharedPtrManager::clearAll() {
    std::unique_lock lock(mutex_);
    shared_ptr_map_.clear();
    LOG_F(INFO, "Cleared all shared pointers");
}

auto GlobalSharedPtrManager::size() const -> size_t {
    std::shared_lock lock(mutex_);
    size_t size = shared_ptr_map_.size();
    LOG_F(INFO, "Current size of shared_ptr_map_: {}", size);
    return size;
}

void GlobalSharedPtrManager::printSharedPtrMap() const {
    std::shared_lock lock(mutex_);
#if ATOM_ENABLE_DEBUG
    std::cout << "GlobalSharedPtrManager:\n";
    for (const auto &pair : shared_ptr_map_) {
        std::cout << "  " << pair.first << "\n";
    }
#endif
    LOG_F(INFO, "Printed shared_ptr_map_ contents");
}