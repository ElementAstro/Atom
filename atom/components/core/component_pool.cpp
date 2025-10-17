/*
 * component_pool.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "component_pool.hpp"
#include "component.hpp"

#include <algorithm>
#include <cassert>

namespace atom::components {

// All ComponentPool<T> template method implementations moved to header file
// for proper template instantiation

// ComponentFactory implementation
ComponentFactory& ComponentFactory::instance() {
    static ComponentFactory instance;
    return instance;
}

// Template implementations moved to header file

void ComponentFactory::cleanupAll() {
    std::shared_lock lock(poolsMutex_);
    // Note: In a real implementation, we'd iterate through all pools
    // For now, this is a placeholder
}

size_t ComponentFactory::getTotalMemoryUsage() const {
    std::shared_lock lock(poolsMutex_);
    size_t total = 0;
    // Note: In a real implementation, we'd sum up all pool memory usage
    return total;
}

// Explicit template instantiation for Component
template class ComponentPool<Component>;

}  // namespace atom::components
