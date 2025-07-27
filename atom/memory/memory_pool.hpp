/*
 * atom/memory/memory_pool.hpp
 *
 * Copyright (C) 2024 Max Qian <lightapt.com>
 */

#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <immintrin.h>  // For memory prefetching

// Cache line size for alignment optimizations
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

namespace atom {
namespace memory {

/**
 * @brief Enhanced statistics for fixed-size memory pool
 */
struct FixedPoolStats {
    std::atomic<size_t> total_allocations{0};     ///< Total allocation count
    std::atomic<size_t> total_deallocations{0};   ///< Total deallocation count
    std::atomic<size_t> current_allocations{0};   ///< Current active allocations
    std::atomic<size_t> peak_allocations{0};      ///< Peak concurrent allocations
    std::atomic<size_t> chunk_count{0};           ///< Number of chunks allocated
    std::atomic<size_t> cache_hits{0};            ///< Free list cache hits
    std::atomic<size_t> cache_misses{0};          ///< Free list cache misses
    std::atomic<uint64_t> total_alloc_time{0};    ///< Total allocation time (ns)
    std::atomic<uint64_t> total_dealloc_time{0};  ///< Total deallocation time (ns)
    std::atomic<uint64_t> max_alloc_time{0};      ///< Maximum allocation time (ns)
    std::atomic<uint64_t> max_dealloc_time{0};    ///< Maximum deallocation time (ns)

    void reset() noexcept {
        total_allocations = 0;
        total_deallocations = 0;
        current_allocations = 0;
        peak_allocations = 0;
        chunk_count = 0;
        cache_hits = 0;
        cache_misses = 0;
        total_alloc_time = 0;
        total_dealloc_time = 0;
        max_alloc_time = 0;
        max_dealloc_time = 0;
    }

    double getCacheHitRatio() const noexcept {
        size_t total_requests = cache_hits.load() + cache_misses.load();
        return total_requests > 0 ? static_cast<double>(cache_hits.load()) / total_requests : 0.0;
    }

    double getAverageAllocTime() const noexcept {
        size_t count = total_allocations.load();
        return count > 0 ? static_cast<double>(total_alloc_time.load()) / count : 0.0;
    }

    double getAverageDeallocTime() const noexcept {
        size_t count = total_deallocations.load();
        return count > 0 ? static_cast<double>(total_dealloc_time.load()) / count : 0.0;
    }

    // Create a copyable snapshot of the statistics
    void snapshot(FixedPoolStats& copy) const noexcept {
        copy.total_allocations.store(total_allocations.load());
        copy.total_deallocations.store(total_deallocations.load());
        copy.current_allocations.store(current_allocations.load());
        copy.peak_allocations.store(peak_allocations.load());
        copy.chunk_count.store(chunk_count.load());
        copy.cache_hits.store(cache_hits.load());
        copy.cache_misses.store(cache_misses.load());
        copy.total_alloc_time.store(total_alloc_time.load());
        copy.total_dealloc_time.store(total_dealloc_time.load());
        copy.max_alloc_time.store(max_alloc_time.load());
        copy.max_dealloc_time.store(max_dealloc_time.load());
    }
};

/**
 * @brief Lock-free block structure for high-performance allocation
 */
struct alignas(CACHE_LINE_SIZE) LockFreeBlock {
    std::atomic<LockFreeBlock*> next{nullptr};

    LockFreeBlock() = default;
    explicit LockFreeBlock(LockFreeBlock* n) : next(n) {}
};

/**
 * @brief Lock-free stack for free block management
 */
class alignas(CACHE_LINE_SIZE) LockFreeStack {
private:
    std::atomic<LockFreeBlock*> head_{nullptr};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> size_{0};

public:
    void push(LockFreeBlock* node) noexcept {
        LockFreeBlock* old_head = head_.load(std::memory_order_relaxed);
        do {
            node->next.store(old_head, std::memory_order_relaxed);
        } while (!head_.compare_exchange_weak(old_head, node,
                                            std::memory_order_release,
                                            std::memory_order_relaxed));
        size_.fetch_add(1, std::memory_order_relaxed);
    }

    LockFreeBlock* pop() noexcept {
        LockFreeBlock* head = head_.load(std::memory_order_acquire);
        while (head != nullptr) {
            LockFreeBlock* next = head->next.load(std::memory_order_relaxed);
            if (head_.compare_exchange_weak(head, next,
                                          std::memory_order_release,
                                          std::memory_order_relaxed)) {
                size_.fetch_sub(1, std::memory_order_relaxed);
                return head;
            }
        }
        return nullptr;
    }

    size_t size() const noexcept {
        return size_.load(std::memory_order_relaxed);
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_relaxed) == nullptr;
    }
};

/**
 * @brief Enhanced high-performance fixed-size block memory pool
 *
 * Specialized for efficiently allocating and deallocating fixed-size memory
 * blocks with advanced features including lock-free optimizations, performance
 * monitoring, and cache-friendly memory layout.
 *
 * @tparam BlockSize Size of each memory block in bytes
 * @tparam BlocksPerChunk Number of blocks per chunk
 * @tparam EnableLockFree Enable lock-free optimizations
 */
template <std::size_t BlockSize = 64, std::size_t BlocksPerChunk = 1024, bool EnableLockFree = false>
class MemoryPool {
private:
    struct Block {
        Block* next;
    };

    struct alignas(CACHE_LINE_SIZE) Chunk {
        alignas(std::max_align_t)
            std::array<std::byte, BlockSize * BlocksPerChunk> memory;
        std::atomic<bool> initialized{false};  ///< Initialization flag for thread safety

        constexpr Chunk() noexcept {
            static_assert(BlockSize >= sizeof(Block), "Block size too small");
        }
    };

    // Traditional mutex-based members
    Block* free_list_ = nullptr;
    std::vector<std::unique_ptr<Chunk>> chunks_;
    mutable std::shared_mutex mutex_;
    std::size_t allocated_blocks_ = 0;
    std::size_t total_blocks_ = 0;

    // Enhanced performance tracking
    FixedPoolStats stats_;

    // Lock-free optimization members
    std::conditional_t<EnableLockFree, LockFreeStack, std::nullptr_t> lock_free_list_;
    std::conditional_t<EnableLockFree, std::atomic<bool>, bool> lock_free_mode_{EnableLockFree};

    // Cache optimization
    alignas(CACHE_LINE_SIZE) std::atomic<void*> last_allocated_{nullptr};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> allocation_hint_{0};

    void allocate_new_chunk() {
        auto chunk = std::make_unique<Chunk>();

        if constexpr (EnableLockFree) {
            // Initialize blocks for lock-free operation
            for (std::size_t i = 0; i < BlocksPerChunk; ++i) {
                auto* block = reinterpret_cast<LockFreeBlock*>(&chunk->memory[i * BlockSize]);
                new (block) LockFreeBlock();
                lock_free_list_.push(block);
            }
        } else {
            // Traditional linked list initialization
            for (std::size_t i = 0; i < BlocksPerChunk; ++i) {
                auto* block = reinterpret_cast<Block*>(&chunk->memory[i * BlockSize]);
                block->next = free_list_;
                free_list_ = block;
            }
        }

        chunks_.push_back(std::move(chunk));
        total_blocks_ += BlocksPerChunk;
        stats_.chunk_count.fetch_add(1, std::memory_order_relaxed);

        // Mark chunk as initialized
        chunk->initialized.store(true, std::memory_order_release);
    }

    /**
     * @brief Prefetch memory for better cache performance
     */
    void prefetchMemory(void* ptr) const noexcept {
        if (ptr) {
            _mm_prefetch(static_cast<char*>(ptr), _MM_HINT_T0);
            // Prefetch next cache line as well for larger blocks
            if (BlockSize > CACHE_LINE_SIZE) {
                _mm_prefetch(static_cast<char*>(ptr) + CACHE_LINE_SIZE, _MM_HINT_T0);
            }
        }
    }

    /**
     * @brief Update timing statistics
     */
    void updateTimingStats(uint64_t duration, bool is_allocation) noexcept {
        if (is_allocation) {
            stats_.total_alloc_time.fetch_add(duration, std::memory_order_relaxed);
            uint64_t current_max = stats_.max_alloc_time.load();
            while (duration > current_max &&
                   !stats_.max_alloc_time.compare_exchange_weak(current_max, duration)) {
                // Keep trying until we successfully update or find a larger value
            }
        } else {
            stats_.total_dealloc_time.fetch_add(duration, std::memory_order_relaxed);
            uint64_t current_max = stats_.max_dealloc_time.load();
            while (duration > current_max &&
                   !stats_.max_dealloc_time.compare_exchange_weak(current_max, duration)) {
                // Keep trying until we successfully update or find a larger value
            }
        }
    }

public:
    static_assert(BlockSize >= sizeof(Block),
                  "Block size must be at least sizeof(Block)");
    static_assert(BlockSize % alignof(std::max_align_t) == 0,
                  "Block size must be aligned to std::max_align_t");

    MemoryPool() = default;
    ~MemoryPool() = default;
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&) noexcept = default;
    MemoryPool& operator=(MemoryPool&&) noexcept = default;

    /**
     * @brief Allocates a memory block
     * @return Pointer to allocated memory block
     */
    [[nodiscard]] void* allocate() {
        auto start_time = std::chrono::high_resolution_clock::now();
        void* result = nullptr;

        if constexpr (EnableLockFree) {
            // Try lock-free allocation first
            if (auto* block = lock_free_list_.pop()) {
                result = block;
                stats_.cache_hits.fetch_add(1, std::memory_order_relaxed);

                // Update allocation statistics
                stats_.total_allocations.fetch_add(1, std::memory_order_relaxed);
                size_t current = stats_.current_allocations.fetch_add(1, std::memory_order_relaxed) + 1;

                // Update peak allocations
                size_t current_peak = stats_.peak_allocations.load();
                while (current > current_peak &&
                       !stats_.peak_allocations.compare_exchange_weak(current_peak, current)) {
                    // Keep trying until we successfully update or find a larger value
                }

                prefetchMemory(result);
                last_allocated_.store(result, std::memory_order_relaxed);

                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
                updateTimingStats(duration, true);

                return result;
            } else {
                stats_.cache_misses.fetch_add(1, std::memory_order_relaxed);
            }
        }

        // Fall back to mutex-based allocation
        std::lock_guard lock(mutex_);

        if ((free_list_ == nullptr)) {
            allocate_new_chunk();
        }

        Block* block = free_list_;
        free_list_ = block->next;
        ++allocated_blocks_;
        result = static_cast<void*>(block);

        // Update statistics
        stats_.total_allocations.fetch_add(1, std::memory_order_relaxed);
        size_t current = stats_.current_allocations.fetch_add(1, std::memory_order_relaxed) + 1;

        // Update peak allocations
        size_t current_peak = stats_.peak_allocations.load();
        while (current > current_peak &&
               !stats_.peak_allocations.compare_exchange_weak(current_peak, current)) {
            // Keep trying until we successfully update or find a larger value
        }

        prefetchMemory(result);
        last_allocated_.store(result, std::memory_order_relaxed);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
        updateTimingStats(duration, true);

        return result;
    }

    /**
     * @brief Enhanced deallocate method with performance optimizations
     * @param ptr Pointer to memory block to deallocate
     */
    void deallocate(void* ptr) noexcept {
        if ((!ptr))
            return;

        auto start_time = std::chrono::high_resolution_clock::now();

        if constexpr (EnableLockFree) {
            // Try lock-free deallocation first
            auto* block = static_cast<LockFreeBlock*>(ptr);
            lock_free_list_.push(block);

            // Update statistics
            stats_.total_deallocations.fetch_add(1, std::memory_order_relaxed);
            stats_.current_allocations.fetch_sub(1, std::memory_order_relaxed);

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
            updateTimingStats(duration, false);

            return;
        }

        // Fall back to mutex-based deallocation
        std::lock_guard lock(mutex_);

        Block* block = static_cast<Block*>(ptr);
        block->next = free_list_;
        free_list_ = block;
        --allocated_blocks_;

        // Update statistics
        stats_.total_deallocations.fetch_add(1, std::memory_order_relaxed);
        stats_.current_allocations.fetch_sub(1, std::memory_order_relaxed);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
        updateTimingStats(duration, false);
    }

    /**
     * @brief Gets memory pool statistics
     * @return Pair of (allocated_blocks, total_blocks)
     */
    std::pair<std::size_t, std::size_t> get_stats() const noexcept {
        std::lock_guard lock(mutex_);
        return {allocated_blocks_, total_blocks_};
    }

    /**
     * @brief Checks if pool is empty
     * @return True if no blocks are allocated
     */
    bool is_empty() const noexcept {
        std::lock_guard lock(mutex_);
        return allocated_blocks_ == 0;
    }

    /**
     * @brief Resets the pool, marking all blocks as available
     */
    void reset() noexcept {
        std::lock_guard lock(mutex_);

        free_list_ = nullptr;
        for (auto& chunk : chunks_) {
            for (std::size_t i = 0; i < BlocksPerChunk; ++i) {
                auto* block =
                    reinterpret_cast<Block*>(&chunk->memory[i * BlockSize]);
                block->next = free_list_;
                free_list_ = block;
            }
        }
        allocated_blocks_ = 0;
    }

    /**
     * @brief Get detailed performance statistics
     * @param stats Reference to statistics structure to fill
     */
    void getDetailedStats(FixedPoolStats& stats) const noexcept {
        stats_.snapshot(stats);
    }

    /**
     * @brief Get cache performance metrics
     * @return Tuple of (hit_ratio, hits, misses)
     */
    [[nodiscard]] auto getCachePerformance() const noexcept -> std::tuple<double, size_t, size_t> {
        size_t hits = stats_.cache_hits.load(std::memory_order_relaxed);
        size_t misses = stats_.cache_misses.load(std::memory_order_relaxed);
        double hit_ratio = stats_.getCacheHitRatio();
        return std::make_tuple(hit_ratio, hits, misses);
    }

    /**
     * @brief Get timing performance metrics
     * @return Tuple of (avg_alloc_time, avg_dealloc_time, max_alloc_time, max_dealloc_time)
     */
    [[nodiscard]] auto getTimingPerformance() const noexcept -> std::tuple<double, double, uint64_t, uint64_t> {
        double avg_alloc = stats_.getAverageAllocTime();
        double avg_dealloc = stats_.getAverageDeallocTime();
        uint64_t max_alloc = stats_.max_alloc_time.load(std::memory_order_relaxed);
        uint64_t max_dealloc = stats_.max_dealloc_time.load(std::memory_order_relaxed);
        return std::make_tuple(avg_alloc, avg_dealloc, max_alloc, max_dealloc);
    }

    /**
     * @brief Get memory utilization statistics
     * @return Tuple of (utilization_ratio, peak_allocations, current_allocations)
     */
    [[nodiscard]] auto getUtilizationStats() const noexcept -> std::tuple<double, size_t, size_t> {
        size_t current = stats_.current_allocations.load(std::memory_order_relaxed);
        size_t peak = stats_.peak_allocations.load(std::memory_order_relaxed);
        double utilization = total_blocks_ > 0 ? static_cast<double>(current) / total_blocks_ : 0.0;
        return std::make_tuple(utilization, peak, current);
    }

    /**
     * @brief Reset all performance statistics
     */
    void resetStats() noexcept {
        stats_.reset();
    }

    /**
     * @brief Check if lock-free mode is enabled
     * @return True if lock-free optimizations are enabled
     */
    [[nodiscard]] constexpr bool isLockFreeEnabled() const noexcept {
        return EnableLockFree;
    }

    /**
     * @brief Get the block size
     * @return Size of each block in bytes
     */
    [[nodiscard]] constexpr std::size_t getBlockSize() const noexcept {
        return BlockSize;
    }

    /**
     * @brief Get the number of blocks per chunk
     * @return Number of blocks allocated per chunk
     */
    [[nodiscard]] constexpr std::size_t getBlocksPerChunk() const noexcept {
        return BlocksPerChunk;
    }
};

/**
 * @brief Enhanced generic object pool based on MemoryPool
 *
 * Efficiently allocates and recycles objects of a specific type with
 * advanced features including lock-free optimizations and performance monitoring.
 *
 * @tparam T Object type
 * @tparam BlocksPerChunk Number of objects per chunk
 * @tparam EnableLockFree Enable lock-free optimizations
 */
template <typename T, std::size_t BlocksPerChunk = 1024, bool EnableLockFree = false>
class ObjectPool {
private:
    static constexpr std::size_t block_size =
        ((sizeof(T) + alignof(std::max_align_t) - 1) /
         alignof(std::max_align_t)) *
        alignof(std::max_align_t);

    MemoryPool<block_size, BlocksPerChunk, EnableLockFree> memory_pool_;

    // Object-specific statistics
    std::atomic<size_t> objects_constructed_{0};
    std::atomic<size_t> objects_destroyed_{0};

public:
    ObjectPool() = default;
    ~ObjectPool() = default;
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) noexcept = default;
    ObjectPool& operator=(ObjectPool&&) noexcept = default;

    /**
     * @brief Allocates and constructs an object
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Pointer to newly created object
     */
    template <typename... Args>
    [[nodiscard]] T* allocate(Args&&... args) {
        void* memory = memory_pool_.allocate();
        try {
            T* obj = new (memory) T(std::forward<Args>(args)...);
            objects_constructed_.fetch_add(1, std::memory_order_relaxed);
            return obj;
        } catch (...) {
            memory_pool_.deallocate(memory);
            throw;
        }
    }

    /**
     * @brief Destructs and deallocates an object
     * @param ptr Pointer to object to deallocate
     */
    void deallocate(T* ptr) noexcept {
        if ((!ptr))
            return;

        ptr->~T();
        memory_pool_.deallocate(static_cast<void*>(ptr));
        objects_destroyed_.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Gets object pool statistics
     * @return Pair of (allocated_objects, total_capacity)
     */
    std::pair<std::size_t, std::size_t> get_stats() const noexcept {
        return memory_pool_.get_stats();
    }

    /**
     * @brief Checks if pool is empty
     * @return True if no objects are allocated
     */
    bool is_empty() const noexcept { return memory_pool_.is_empty(); }

    /**
     * @brief Resets the object pool
     * @warning Invalidates all allocated object pointers
     */
    void reset() noexcept {
        memory_pool_.reset();
        objects_constructed_.store(0, std::memory_order_relaxed);
        objects_destroyed_.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Get object pool statistics
     * @return Tuple of (constructed, destroyed, active)
     */
    [[nodiscard]] auto getObjectStats() const noexcept -> std::tuple<size_t, size_t, size_t> {
        size_t constructed = objects_constructed_.load(std::memory_order_relaxed);
        size_t destroyed = objects_destroyed_.load(std::memory_order_relaxed);
        size_t active = constructed - destroyed;
        return std::make_tuple(constructed, destroyed, active);
    }

    /**
     * @brief Get underlying memory pool statistics
     * @param stats Reference to statistics structure to fill
     */
    void getMemoryStats(FixedPoolStats& stats) const noexcept {
        memory_pool_.getDetailedStats(stats);
    }

    /**
     * @brief Get cache performance from underlying memory pool
     * @return Tuple of (hit_ratio, hits, misses)
     */
    [[nodiscard]] auto getCachePerformance() const noexcept -> std::tuple<double, size_t, size_t> {
        return memory_pool_.getCachePerformance();
    }

    /**
     * @brief Get timing performance from underlying memory pool
     * @return Tuple of (avg_alloc_time, avg_dealloc_time, max_alloc_time, max_dealloc_time)
     */
    [[nodiscard]] auto getTimingPerformance() const noexcept -> std::tuple<double, double, uint64_t, uint64_t> {
        return memory_pool_.getTimingPerformance();
    }

    /**
     * @brief Check if lock-free mode is enabled
     * @return True if lock-free optimizations are enabled
     */
    [[nodiscard]] constexpr bool isLockFreeEnabled() const noexcept {
        return EnableLockFree;
    }

    /**
     * @brief Get the size of objects managed by this pool
     * @return Size of each object in bytes
     */
    [[nodiscard]] constexpr std::size_t getObjectSize() const noexcept {
        return sizeof(T);
    }

    /**
     * @brief Get the effective block size used by the underlying memory pool
     * @return Block size in bytes
     */
    [[nodiscard]] constexpr std::size_t getBlockSize() const noexcept {
        return block_size;
    }
};

/**
 * @brief Smart pointer using ObjectPool for memory management
 *
 * Similar to std::unique_ptr but uses ObjectPool for allocation/deallocation.
 *
 * @tparam T Managed object type
 */
template <typename T>
class PoolPtr {
private:
    T* ptr_ = nullptr;
    ObjectPool<T>* pool_ = nullptr;

public:
    PoolPtr() noexcept = default;

    /**
     * @brief Constructs from object pointer and pool
     * @param ptr Object pointer
     * @param pool Object pool pointer
     */
    explicit PoolPtr(T* ptr, ObjectPool<T>* pool) noexcept
        : ptr_(ptr), pool_(pool) {}

    ~PoolPtr() { reset(); }

    PoolPtr(const PoolPtr&) = delete;
    PoolPtr& operator=(const PoolPtr&) = delete;

    PoolPtr(PoolPtr&& other) noexcept : ptr_(other.ptr_), pool_(other.pool_) {
        other.ptr_ = nullptr;
        other.pool_ = nullptr;
    }

    PoolPtr& operator=(PoolPtr&& other) noexcept {
        if ((this != &other)) [[likely]] {
            reset();
            ptr_ = other.ptr_;
            pool_ = other.pool_;
            other.ptr_ = nullptr;
            other.pool_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief Resets the pointer
     * @param ptr New object pointer
     * @param pool New object pool pointer
     */
    void reset(T* ptr = nullptr, ObjectPool<T>* pool = nullptr) noexcept {
        if ((ptr_ && pool_)) [[likely]] {
            pool_->deallocate(ptr_);
        }
        ptr_ = ptr;
        pool_ = pool;
    }

    /**
     * @brief Releases ownership and returns pointer
     * @return Managed object pointer
     */
    T* release() noexcept {
        T* ptr = ptr_;
        ptr_ = nullptr;
        pool_ = nullptr;
        return ptr;
    }

    /**
     * @brief Gets the managed object pointer
     * @return Pointer to managed object
     */
    T* get() const noexcept { return ptr_; }

    /**
     * @brief Dereference operator
     * @return Reference to managed object
     */
    T& operator*() const {
        assert(ptr_ != nullptr);
        return *ptr_;
    }

    /**
     * @brief Member access operator
     * @return Pointer to managed object
     */
    T* operator->() const noexcept {
        assert(ptr_ != nullptr);
        return ptr_;
    }

    /**
     * @brief Boolean conversion operator
     * @return True if managing a valid object
     */
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    /**
     * @brief Swaps contents with another PoolPtr
     * @param other PoolPtr to swap with
     */
    void swap(PoolPtr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(pool_, other.pool_);
    }
};

/**
 * @brief Creates a PoolPtr from an ObjectPool
 * @tparam T Object type
 * @tparam Args Constructor argument types
 * @param pool Object pool reference
 * @param args Constructor arguments
 * @return PoolPtr managing the newly created object
 */
template <typename T, typename... Args>
[[nodiscard]] PoolPtr<T> make_pool_ptr(ObjectPool<T>& pool, Args&&... args) {
    return PoolPtr<T>(pool.allocate(std::forward<Args>(args)...), &pool);
}

}  // namespace memory
}  // namespace atom
