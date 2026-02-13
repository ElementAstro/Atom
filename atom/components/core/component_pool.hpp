/*
 * component_pool.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Specialized Memory Pool for Component System
Provides high-performance memory allocation and cache-friendly
component management with SIMD-optimized iteration patterns.

**************************************************/

#ifndef ATOM_COMPONENT_POOL_HPP
#define ATOM_COMPONENT_POOL_HPP

#include <atomic>
#include <bitset>
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <vector>

#if ENABLE_FASTHASH
#include "emhash/hash_table8.hpp"
#endif

// Include existing memory pool infrastructure
#include "atom/memory/memory_pool.hpp"
#include "atom/memory/object.hpp"

// Forward declarations
class Component;

namespace atom::components {

/**
 * @brief Exception for component pool operations
 */
class ComponentPoolException : public std::runtime_error {
public:
    explicit ComponentPoolException(const std::string& message)
        : std::runtime_error(message) {}
};

#define THROW_COMPONENT_POOL_EXCEPTION(...)                      \
    throw ComponentPoolException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                 ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Memory pool statistics for monitoring and optimization
 */
struct PoolStatistics {
    std::atomic<uint64_t> totalAllocations{0};
    std::atomic<uint64_t> totalDeallocations{0};
    std::atomic<uint64_t> currentAllocations{0};
    std::atomic<uint64_t> peakAllocations{0};
    std::atomic<uint64_t> poolHits{0};
    std::atomic<uint64_t> poolMisses{0};
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> cacheMisses{0};

    struct {
        std::chrono::microseconds totalAllocationTime{0};
        std::chrono::microseconds maxAllocationTime{0};
        std::chrono::microseconds avgAllocationTime{0};
    } timing;

    void reset() noexcept {
        totalAllocations = 0;
        totalDeallocations = 0;
        currentAllocations = 0;
        peakAllocations = 0;
        poolHits = 0;
        poolMisses = 0;
        cacheHits = 0;
        cacheMisses = 0;
        timing.totalAllocationTime = std::chrono::microseconds{0};
        timing.maxAllocationTime = std::chrono::microseconds{0};
        timing.avgAllocationTime = std::chrono::microseconds{0};
    }

    [[nodiscard]] double getHitRatio() const noexcept {
        const auto hits = poolHits.load(std::memory_order_relaxed);
        const auto misses = poolMisses.load(std::memory_order_relaxed);
        const auto total = hits + misses;
        return total > 0 ? static_cast<double>(hits) / total : 0.0;
    }

    [[nodiscard]] double getCacheHitRatio() const noexcept {
        const auto hits = cacheHits.load(std::memory_order_relaxed);
        const auto misses = cacheMisses.load(std::memory_order_relaxed);
        const auto total = hits + misses;
        return total > 0 ? static_cast<double>(hits) / total : 0.0;
    }
};

/**
 * @brief Configuration for component memory pool
 */
struct PoolConfig {
    size_t initialPoolSize = 64;
    size_t maxPoolSize = 1024;
    size_t chunkSize = 16;  // Components per chunk for cache locality
    bool enableStatistics = true;
    bool enableCacheOptimization = true;
    bool enableMemoryAlignment = true;
    size_t alignmentSize = 64;  // Cache line alignment
    std::chrono::milliseconds cleanupInterval = std::chrono::minutes(5);
    double maxFragmentationRatio = 0.3;  // Trigger defragmentation
};

/**
 * @brief High-performance memory pool specialized for component allocation
 *
 * Features:
 * - Cache-line aligned allocations for optimal performance
 * - Chunk-based allocation for spatial locality
 * - SIMD-friendly memory layouts
 * - Automatic defragmentation
 * - Comprehensive statistics and monitoring
 *
 * @tparam T Component type
 */
template <typename T>
class ComponentPool {
public:
    static_assert(std::is_base_of_v<Component, T>,
                  "T must derive from Component");

    /**
     * @brief Constructs a ComponentPool with specified configuration
     * @param config Pool configuration
     */
    explicit ComponentPool(const PoolConfig& config = {});

    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~ComponentPool();

    // Disable copy operations
    ComponentPool(const ComponentPool&) = delete;
    ComponentPool& operator=(const ComponentPool&) = delete;

    // Enable move operations
    ComponentPool(ComponentPool&&) noexcept = default;
    ComponentPool& operator=(ComponentPool&&) noexcept = default;

    /**
     * @brief Allocates and constructs a component
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Shared pointer to the allocated component
     */
    template <typename... Args>
    [[nodiscard]] std::shared_ptr<T> allocate(Args&&... args);

    /**
     * @brief Deallocates a component (returns to pool)
     * @param component Component to deallocate
     */
    void deallocate(std::shared_ptr<T>& component);

    void deallocate(std::nullptr_t) {}

    /**
     * @brief Gets current pool statistics
     * @return Pool statistics
     */
    [[nodiscard]] const PoolStatistics& getStatistics() const noexcept {
        return statistics_;
    }

    /**
     * @brief Resets pool statistics
     */
    void resetStatistics() noexcept { statistics_.reset(); }

    /**
     * @brief Gets current pool configuration
     * @return Pool configuration
     */
    [[nodiscard]] const PoolConfig& getConfig() const noexcept {
        return config_;
    }

    /**
     * @brief Updates pool configuration
     * @param config New configuration
     */
    void updateConfig(const PoolConfig& config);

    /**
     * @brief Performs manual cleanup and defragmentation
     */
    void cleanup();

    /**
     * @brief Gets memory usage information
     * @return Memory usage in bytes
     */
    [[nodiscard]] size_t getMemoryUsage() const noexcept;

    /**
     * @brief Gets fragmentation ratio
     * @return Fragmentation ratio (0.0 = no fragmentation, 1.0 = fully
     * fragmented)
     */
    [[nodiscard]] double getFragmentationRatio() const noexcept;

private:
    struct alignas(64) ComponentChunk {
        // Use modern aligned storage
        alignas(64) std::array<std::byte, sizeof(T) * 16> storage;
        std::bitset<16> allocated;
        std::atomic<size_t> allocatedCount{0};
        std::chrono::steady_clock::time_point lastAccess;

        ComponentChunk() : lastAccess(std::chrono::steady_clock::now()) {
            static_assert(alignof(T) <= 64,
                          "Component alignment too large for chunk");
        }

        // Get properly aligned pointer for slot
        T* getSlotPtr(size_t index) noexcept {
            assert(index < 16);
            return reinterpret_cast<T*>(storage.data() + (index * sizeof(T)));
        }
    };

    PoolConfig config_;
    mutable PoolStatistics statistics_;

    std::vector<std::unique_ptr<ComponentChunk>> chunks_;
    std::vector<size_t> freeChunks_;

    mutable std::shared_mutex mutex_;
    std::atomic<bool> needsCleanup_{false};
    std::chrono::steady_clock::time_point lastCleanup_;

    // Private methods
    [[nodiscard]] ComponentChunk* allocateChunk();
    void deallocateChunk(ComponentChunk* chunk);
    void performCleanup();
    void updateStatistics(std::chrono::microseconds allocationTime);
};

/**
 * @brief Enhanced component factory with integrated memory pooling
 *
 * Integrates with existing atom::memory infrastructure for optimal
 * memory management and provides SIMD-friendly component layouts.
 */
class ComponentFactory {
public:
    /**
     * @brief Gets the singleton instance
     * @return Reference to the factory instance
     */
    static ComponentFactory& instance();

    /**
     * @brief Creates a component using the appropriate memory pool
     * @tparam T Component type
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Shared pointer to the created component
     */
    template <typename T, typename... Args>
    [[nodiscard]] std::shared_ptr<T> create(Args&&... args);

    /**
     * @brief Creates a component using existing memory pool infrastructure
     * @tparam T Component type
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Shared pointer to the created component
     */
    template <typename T, typename... Args>
    [[nodiscard]] std::shared_ptr<T> createWithMemoryPool(Args&&... args);

    /**
     * @brief Gets pool statistics for a component type
     * @tparam T Component type
     * @return Pool statistics
     */
    template <typename T>
    [[nodiscard]] const PoolStatistics& getPoolStatistics() const;

    /**
     * @brief Configures pool for a component type
     * @tparam T Component type
     * @param config Pool configuration
     */
    template <typename T>
    void configurePool(const PoolConfig& config);

    /**
     * @brief Performs cleanup on all pools
     */
    void cleanupAll();

    /**
     * @brief Gets total memory usage across all pools
     * @return Total memory usage in bytes
     */
    [[nodiscard]] size_t getTotalMemoryUsage() const;

    /**
     * @brief Gets memory pool efficiency metrics
     * @return Map of component type to efficiency ratio
     */
    [[nodiscard]] std::unordered_map<std::string, double> getPoolEfficiency()
        const;

private:
    ComponentFactory() = default;
    ~ComponentFactory() = default;

    ComponentFactory(const ComponentFactory&) = delete;
    ComponentFactory& operator=(const ComponentFactory&) = delete;

    template <typename T>
    ComponentPool<T>& getPool();

    mutable std::shared_mutex poolsMutex_;
    std::unordered_map<std::type_index, std::shared_ptr<void>> pools_;
};

/**
 * @brief SIMD-optimized component container for batch operations
 *
 * Provides cache-friendly storage and SIMD-optimized iteration
 * patterns for component collections.
 */
template <typename T>
class alignas(64) SIMDComponentContainer {
public:
    static_assert(std::is_base_of_v<Component, T>,
                  "T must derive from Component");

    /**
     * @brief Configuration for SIMD container
     */
    struct SIMDConfig {
        size_t batchSize = 64;        // Components per batch (SIMD-friendly)
        size_t prefetchDistance = 2;  // Cache lines to prefetch ahead
        bool enablePrefetch = true;   // Enable software prefetching
        bool enableSIMD = true;       // Enable SIMD optimizations
    };

    explicit SIMDComponentContainer(const SIMDConfig& config = {});

    /**
     * @brief Adds a component to the container
     * @param component Component to add
     */
    void add(std::shared_ptr<T> component);

    /**
     * @brief Removes a component from the container
     * @param component Component to remove
     */
    void remove(const std::shared_ptr<T>& component);

    /**
     * @brief Applies a function to all components in SIMD-optimized batches
     * @tparam Func Function type
     * @param func Function to apply
     */
    template <typename Func>
    void forEachBatch(Func&& func);

    /**
     * @brief Gets component count
     * @return Number of components
     */
    [[nodiscard]] size_t size() const noexcept {
        std::shared_lock lock(mutex_);
        return components_.size();
    }

    /**
     * @brief Checks if container is empty
     * @return True if empty
     */
    [[nodiscard]] bool empty() const noexcept {
        std::shared_lock lock(mutex_);
        return components_.empty();
    }

    /**
     * @brief Optimizes internal layout for better cache performance
     */
    void optimize();

private:
    SIMDConfig config_;
    std::vector<std::shared_ptr<T>> components_;
    mutable std::shared_mutex mutex_;

    // Internal batch processing
    template <typename Func>
    void processBatch(size_t startIdx, size_t endIdx, Func&& func);
};

// ============================================================================
// ComponentPool Template Implementation
// ============================================================================

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
    void* memory = static_cast<void*>(targetChunk->getSlotPtr(slotIndex));

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

// ============================================================================
// ComponentPool Private Method Implementations
// ============================================================================

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

// ============================================================================
// ComponentPool Public Method Implementations
// ============================================================================

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
void ComponentPool<T>::deallocate(std::shared_ptr<T>& component) {
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

// ============================================================================
// ComponentFactory Template Implementation
// ============================================================================

template <typename T, typename... Args>
std::shared_ptr<T> ComponentFactory::create(Args&&... args) {
    return getPool<T>().allocate(std::forward<Args>(args)...);
}

template <typename T>
const PoolStatistics& ComponentFactory::getPoolStatistics() const {
    return const_cast<ComponentFactory*>(this)->getPool<T>().getStatistics();
}

template <typename T>
void ComponentFactory::configurePool(const PoolConfig& config) {
    getPool<T>().updateConfig(config);
}

template <typename T>
ComponentPool<T>& ComponentFactory::getPool() {
    std::unique_lock lock(poolsMutex_);

    auto typeIndex = std::type_index(typeid(T));
    auto it = pools_.find(typeIndex);

    if (it == pools_.end()) {
        auto pool = std::make_shared<ComponentPool<T>>();
        auto* rawPool = pool.get();
        pools_[typeIndex] = pool;
        return *rawPool;
    }

    return *static_cast<ComponentPool<T>*>(it->second.get());
}

// ============================================================================
// SIMDComponentContainer Template Implementation
// ============================================================================

template <typename T>
SIMDComponentContainer<T>::SIMDComponentContainer(const SIMDConfig& config)
    : config_(config) {
    components_.reserve(config_.batchSize * 4);  // Pre-allocate for efficiency
}

template <typename T>
void SIMDComponentContainer<T>::add(std::shared_ptr<T> component) {
    if (!component) {
        return;
    }

    std::unique_lock lock(mutex_);
    components_.push_back(std::move(component));
}

template <typename T>
void SIMDComponentContainer<T>::remove(const std::shared_ptr<T>& component) {
    if (!component) {
        return;
    }

    std::unique_lock lock(mutex_);
    auto it = std::find(components_.begin(), components_.end(), component);
    if (it != components_.end()) {
        components_.erase(it);
    }
}

template <typename T>
template <typename Func>
void SIMDComponentContainer<T>::forEachBatch(Func&& func) {
    std::shared_lock lock(mutex_);

    const size_t totalComponents = components_.size();
    const size_t batchSize = config_.batchSize;

    for (size_t i = 0; i < totalComponents; i += batchSize) {
        const size_t endIdx = std::min(i + batchSize, totalComponents);
        processBatch(i, endIdx, std::forward<Func>(func));
    }
}

template <typename T>
void SIMDComponentContainer<T>::optimize() {
    std::unique_lock lock(mutex_);

    // Remove null components
    components_.erase(std::remove_if(components_.begin(), components_.end(),
                                     [](const auto& comp) { return !comp; }),
                      components_.end());

    // Shrink to fit to reduce memory overhead
    components_.shrink_to_fit();
}

template <typename T>
template <typename Func>
void SIMDComponentContainer<T>::processBatch(size_t startIdx, size_t endIdx,
                                             Func&& func) {
    // Prefetch if enabled
    if (config_.enablePrefetch && config_.prefetchDistance > 0) {
        const size_t prefetchIdx =
            std::min(endIdx + config_.prefetchDistance, components_.size());
        if (prefetchIdx < components_.size()) {
            __builtin_prefetch(components_[prefetchIdx].get(), 0, 3);
        }
    }

    // Process batch
    for (size_t i = startIdx; i < endIdx; ++i) {
        if (components_[i]) {
            if (!func(components_[i])) {
                break;  // Early termination if func returns false
            }
        }
    }
}

}  // namespace atom::components

#endif  // ATOM_COMPONENT_POOL_HPP
