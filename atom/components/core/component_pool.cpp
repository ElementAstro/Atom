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
    // Iterate through all pools and perform cleanup
    for (auto& [typeIndex, poolPtr] : pools_) {
        // Cast to ComponentPool<Component> for cleanup
        // Note: This assumes all pools derive from a common base or use
        // Component
        if (auto* pool =
                static_cast<ComponentPool<Component>*>(poolPtr.get())) {
            pool->cleanup();
        }
    }
}

size_t ComponentFactory::getTotalMemoryUsage() const {
    std::shared_lock lock(poolsMutex_);
    size_t total = 0;
    // Sum up memory usage from all pools
    for (const auto& [typeIndex, poolPtr] : pools_) {
        // Cast to ComponentPool<Component> to access getMemoryUsage
        if (const auto* pool =
                static_cast<const ComponentPool<Component>*>(poolPtr.get())) {
            total += pool->getMemoryUsage();
        }
    }
    return total;
}

// Explicit template instantiation for Component
template class ComponentPool<Component>;

}  // namespace atom::components
