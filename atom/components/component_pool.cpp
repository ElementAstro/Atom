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

template <typename T>
ComponentPool<T>::ComponentPool(const PoolConfig& config)
    : config_(config), lastCleanup_(std::chrono::steady_clock::now()) {
    // Pre-allocate initial chunks
    chunks_.reserve(config_.maxPoolSize / config_.chunkSize);
    freeChunks_.reserve(config_.maxPoolSize / config_.chunkSize);

    // Allocate initial pool
    const size_t initialChunks =
        (config_.initialPoolSize + config_.chunkSize - 1) / config_.chunkSize;
    for (size_t i = 0; i < initialChunks; ++i) {
        auto chunk = std::make_unique<ComponentChunk>();
        freeChunks_.push_back(chunks_.size());
        chunks_.push_back(std::move(chunk));
    }
}

template <typename T>
ComponentPool<T>::~ComponentPool() {
    cleanup();
}

template <typename T>
template <typename... Args>
std::shared_ptr<T> ComponentPool<T>::allocate(Args&&... args) {
    const auto startTime = std::chrono::high_resolution_clock::now();

    std::unique_lock lock(mutex_);

    // Find available slot
    ComponentChunk* targetChunk = nullptr;
    size_t slotIndex = 0;

    // First, try to find a chunk with available slots
    for (auto& chunk : chunks_) {
        if (chunk->allocatedCount.load(std::memory_order_relaxed) <
            config_.chunkSize) {
            for (size_t i = 0; i < config_.chunkSize; ++i) {
                if (!chunk->allocated[i]) {
                    targetChunk = chunk.get();
                    slotIndex = i;
                    break;
                }
            }
            if (targetChunk)
                break;
        }
    }

    // If no available slot, allocate new chunk
    if (!targetChunk) {
        if (chunks_.size() >= config_.maxPoolSize / config_.chunkSize) {
            lock.unlock();
            const auto endTime = std::chrono::high_resolution_clock::now();
            updateStatistics(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    endTime - startTime));
            statistics_.poolMisses.fetch_add(1, std::memory_order_relaxed);

            // Fall back to regular allocation
            return std::make_shared<T>(std::forward<Args>(args)...);
        }

        targetChunk = allocateChunk();
        slotIndex = 0;
    }

    assert(targetChunk != nullptr);

    // Mark slot as allocated
    targetChunk->allocated[slotIndex] = true;
    targetChunk->allocatedCount.fetch_add(1, std::memory_order_relaxed);
    targetChunk->lastAccess = std::chrono::steady_clock::now();

    // Get memory location
    void* memory = &targetChunk->storage[slotIndex];

    lock.unlock();

    // Construct object in-place
    T* rawPtr = new (memory) T(std::forward<Args>(args)...);

    // Create shared_ptr with custom deleter
    std::shared_ptr<T> result(rawPtr, [this, targetChunk, slotIndex](T* ptr) {
        // Destroy object
        ptr->~T();

        // Return slot to pool
        std::unique_lock deallocLock(mutex_);
        targetChunk->allocated[slotIndex] = false;
        targetChunk->allocatedCount.fetch_sub(1, std::memory_order_relaxed);

        statistics_.totalDeallocations.fetch_add(1, std::memory_order_relaxed);
        statistics_.currentAllocations.fetch_sub(1, std::memory_order_relaxed);
    });

    // Update statistics
    const auto endTime = std::chrono::high_resolution_clock::now();
    updateStatistics(std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime));

    statistics_.totalAllocations.fetch_add(1, std::memory_order_relaxed);
    const auto current =
        statistics_.currentAllocations.fetch_add(1, std::memory_order_relaxed) +
        1;

    // Update peak allocations
    auto peak = statistics_.peakAllocations.load(std::memory_order_relaxed);
    while (current > peak && !statistics_.peakAllocations.compare_exchange_weak(
                                 peak, current, std::memory_order_relaxed)) {
        // Retry if another thread updated peak
    }

    statistics_.poolHits.fetch_add(1, std::memory_order_relaxed);

    return result;
}

template <typename T>
void ComponentPool<T>::deallocate(std::shared_ptr<T> component) {
    // The actual deallocation is handled by the custom deleter in allocate()
    component.reset();
}

template <typename T>
void ComponentPool<T>::updateConfig(const PoolConfig& config) {
    std::unique_lock lock(mutex_);
    config_ = config;
}

template <typename T>
void ComponentPool<T>::cleanup() {
    std::unique_lock lock(mutex_);
    performCleanup();
}

template <typename T>
size_t ComponentPool<T>::getMemoryUsage() const noexcept {
    std::shared_lock lock(mutex_);
    return chunks_.size() * sizeof(ComponentChunk);
}

template <typename T>
double ComponentPool<T>::getFragmentationRatio() const noexcept {
    std::shared_lock lock(mutex_);

    if (chunks_.empty())
        return 0.0;

    size_t totalSlots = chunks_.size() * config_.chunkSize;
    size_t allocatedSlots = 0;
    size_t fragmentedChunks = 0;

    for (const auto& chunk : chunks_) {
        const auto allocated =
            chunk->allocatedCount.load(std::memory_order_relaxed);
        allocatedSlots += allocated;

        // A chunk is fragmented if it has both allocated and free slots
        if (allocated > 0 && allocated < config_.chunkSize) {
            fragmentedChunks++;
        }
    }

    return totalSlots > 0
               ? static_cast<double>(fragmentedChunks) / chunks_.size()
               : 0.0;
}

template <typename T>
typename ComponentPool<T>::ComponentChunk* ComponentPool<T>::allocateChunk() {
    auto chunk = std::make_unique<ComponentChunk>();
    ComponentChunk* rawPtr = chunk.get();
    chunks_.push_back(std::move(chunk));
    return rawPtr;
}

template <typename T>
void ComponentPool<T>::deallocateChunk(ComponentChunk* chunk) {
    auto it =
        std::find_if(chunks_.begin(), chunks_.end(),
                     [chunk](const auto& ptr) { return ptr.get() == chunk; });

    if (it != chunks_.end()) {
        chunks_.erase(it);
    }
}

template <typename T>
void ComponentPool<T>::performCleanup() {
    const auto now = std::chrono::steady_clock::now();
    const auto cleanupThreshold = now - config_.cleanupInterval;

    // Remove empty chunks that haven't been accessed recently
    chunks_.erase(
        std::remove_if(chunks_.begin(), chunks_.end(),
                       [cleanupThreshold](const auto& chunk) {
                           return chunk->allocatedCount.load(
                                      std::memory_order_relaxed) == 0 &&
                                  chunk->lastAccess < cleanupThreshold;
                       }),
        chunks_.end());

    lastCleanup_ = now;
    needsCleanup_.store(false, std::memory_order_relaxed);
}

template <typename T>
void ComponentPool<T>::updateStatistics(
    std::chrono::microseconds allocationTime) {
    if (!config_.enableStatistics)
        return;

    statistics_.timing.totalAllocationTime += allocationTime;

    if (allocationTime > statistics_.timing.maxAllocationTime) {
        statistics_.timing.maxAllocationTime = allocationTime;
    }

    const auto totalAllocs =
        statistics_.totalAllocations.load(std::memory_order_relaxed);
    if (totalAllocs > 0) {
        statistics_.timing.avgAllocationTime = std::chrono::microseconds{
            statistics_.timing.totalAllocationTime.count() / totalAllocs};
    }
}

// ComponentFactory implementation
ComponentFactory& ComponentFactory::instance() {
    static ComponentFactory instance;
    return instance;
}

template <typename T, typename... Args>
std::shared_ptr<T> ComponentFactory::create(Args&&... args) {
    return getPool<T>().allocate(std::forward<Args>(args)...);
}

template <typename T>
const PoolStatistics& ComponentFactory::getPoolStatistics() const {
    return getPool<T>().getStatistics();
}

template <typename T>
void ComponentFactory::configurePool(const PoolConfig& config) {
    getPool<T>().updateConfig(config);
}

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

template <typename T>
ComponentPool<T>& ComponentFactory::getPool() {
    std::unique_lock lock(poolsMutex_);

    auto typeIndex = std::type_index(typeid(T));
    auto it = pools_.find(typeIndex);

    if (it == pools_.end()) {
        auto pool = std::make_unique<ComponentPool<T>>();
        auto* rawPool = pool.get();

        pools_[typeIndex] = std::unique_ptr<void, void (*)(void*)>(
            pool.release(),
            [](void* ptr) { delete static_cast<ComponentPool<T>*>(ptr); });

        return *rawPool;
    }

    return *static_cast<ComponentPool<T>*>(it->second.get());
}

// Explicit template instantiation for Component
template class ComponentPool<Component>;

}  // namespace atom::components
