#ifndef ATOM_MEMORY_MEMORY_POOL_HPP
#define ATOM_MEMORY_MEMORY_POOL_HPP

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <immintrin.h>  // For memory prefetching

#ifdef ATOM_USE_BOOST
#include <boost/pool/pool.hpp>
#endif

// Cache line size for alignment optimizations
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

namespace atom::memory {

/**
 * @brief Memory pool exception class
 */
class MemoryPoolException : public std::runtime_error {
public:
    explicit MemoryPoolException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Block size calculation strategy interface
 */
class BlockSizeStrategy {
public:
    virtual ~BlockSizeStrategy() = default;
    [[nodiscard]] virtual size_t calculate(
        size_t requested_size) const noexcept = 0;
};

/**
 * @brief Exponential growth block size strategy
 */
class ExponentialBlockSizeStrategy : public BlockSizeStrategy {
public:
    explicit ExponentialBlockSizeStrategy(double growth_factor = 2.0) noexcept
        : growth_factor_(growth_factor) {}

    [[nodiscard]] size_t calculate(
        size_t requested_size) const noexcept override {
        return static_cast<size_t>(requested_size * growth_factor_);
    }

private:
    double growth_factor_;
};

/**
 * @brief Snapshot of memory pool statistics (non-atomic for copying)
 */
struct MemoryPoolStatsSnapshot {
    // Basic allocation statistics
    size_t total_allocated{0};   ///< Total allocated bytes
    size_t total_available{0};   ///< Total available bytes
    size_t allocation_count{0};  ///< Allocation operation count
    size_t deallocation_count{0}; ///< Deallocation operation count
    size_t chunk_count{0};       ///< Number of memory chunks

    // Performance metrics
    size_t cache_hits{0};        ///< Free list cache hits
    size_t cache_misses{0};      ///< Free list cache misses
    size_t coalesce_operations{0}; ///< Number of coalesce operations
    size_t split_operations{0};  ///< Number of block split operations
    size_t peak_allocated{0};    ///< Peak allocated memory
    size_t fragmentation_events{0}; ///< Fragmentation events

    // Timing statistics (in nanoseconds)
    uint64_t total_alloc_time{0}; ///< Total allocation time
    uint64_t total_dealloc_time{0}; ///< Total deallocation time
    uint64_t max_alloc_time{0};   ///< Maximum allocation time
    uint64_t max_dealloc_time{0}; ///< Maximum deallocation time

    // Calculate performance metrics
    double getCacheHitRatio() const noexcept {
        size_t total_requests = cache_hits + cache_misses;
        return total_requests > 0 ? static_cast<double>(cache_hits) / total_requests : 0.0;
    }

    double getAverageAllocTime() const noexcept {
        return allocation_count > 0 ? static_cast<double>(total_alloc_time) / allocation_count : 0.0;
    }

    double getAverageDeallocTime() const noexcept {
        return deallocation_count > 0 ? static_cast<double>(total_dealloc_time) / deallocation_count : 0.0;
    }
};

/**
 * @brief Enhanced memory pool statistics with performance metrics (atomic for thread safety)
 */
struct MemoryPoolStats {
    // Basic allocation statistics
    std::atomic<size_t> total_allocated{0};   ///< Total allocated bytes
    std::atomic<size_t> total_available{0};   ///< Total available bytes
    std::atomic<size_t> allocation_count{0};  ///< Allocation operation count
    std::atomic<size_t> deallocation_count{0}; ///< Deallocation operation count
    std::atomic<size_t> chunk_count{0};       ///< Number of memory chunks

    // Performance metrics
    std::atomic<size_t> cache_hits{0};        ///< Free list cache hits
    std::atomic<size_t> cache_misses{0};      ///< Free list cache misses
    std::atomic<size_t> coalesce_operations{0}; ///< Number of coalesce operations
    std::atomic<size_t> split_operations{0};  ///< Number of block split operations
    std::atomic<size_t> peak_allocated{0};    ///< Peak allocated memory
    std::atomic<size_t> fragmentation_events{0}; ///< Fragmentation events

    // Timing statistics (in nanoseconds)
    std::atomic<uint64_t> total_alloc_time{0}; ///< Total allocation time
    std::atomic<uint64_t> total_dealloc_time{0}; ///< Total deallocation time
    std::atomic<uint64_t> max_alloc_time{0};   ///< Maximum allocation time
    std::atomic<uint64_t> max_dealloc_time{0}; ///< Maximum deallocation time

    void reset() noexcept {
        total_allocated = 0;
        total_available = 0;
        allocation_count = 0;
        deallocation_count = 0;
        chunk_count = 0;
        cache_hits = 0;
        cache_misses = 0;
        coalesce_operations = 0;
        split_operations = 0;
        peak_allocated = 0;
        fragmentation_events = 0;
        total_alloc_time = 0;
        total_dealloc_time = 0;
        max_alloc_time = 0;
        max_dealloc_time = 0;
    }

    // Calculate performance metrics
    double getCacheHitRatio() const noexcept {
        size_t total_requests = cache_hits.load() + cache_misses.load();
        return total_requests > 0 ? static_cast<double>(cache_hits.load()) / total_requests : 0.0;
    }

    double getAverageAllocTime() const noexcept {
        size_t count = allocation_count.load();
        return count > 0 ? static_cast<double>(total_alloc_time.load()) / count : 0.0;
    }

    double getAverageDeallocTime() const noexcept {
        size_t count = deallocation_count.load();
        return count > 0 ? static_cast<double>(total_dealloc_time.load()) / count : 0.0;
    }

    // Create a copyable snapshot of the statistics
    MemoryPoolStatsSnapshot snapshot() const noexcept {
        MemoryPoolStatsSnapshot copy;
        copy.total_allocated = total_allocated.load();
        copy.total_available = total_available.load();
        copy.allocation_count = allocation_count.load();
        copy.deallocation_count = deallocation_count.load();
        copy.chunk_count = chunk_count.load();
        copy.cache_hits = cache_hits.load();
        copy.cache_misses = cache_misses.load();
        copy.coalesce_operations = coalesce_operations.load();
        copy.split_operations = split_operations.load();
        copy.peak_allocated = peak_allocated.load();
        copy.fragmentation_events = fragmentation_events.load();
        copy.total_alloc_time = total_alloc_time.load();
        copy.total_dealloc_time = total_dealloc_time.load();
        copy.max_alloc_time = max_alloc_time.load();
        copy.max_dealloc_time = max_dealloc_time.load();
        return copy;
    }
};

/**
 * @brief Memory tag information, used for debugging
 */
struct MemoryTag {
    std::string name;
    std::string file;
    int line;

    MemoryTag(std::string tag_name, std::string file_name, int line_num)
        : name(std::move(tag_name)),
          file(std::move(file_name)),
          line(line_num) {}
};

/**
 * @brief Lock-free free block node for high-performance allocation
 */
struct alignas(CACHE_LINE_SIZE) LockFreeFreeBlock {
    std::atomic<void*> ptr{nullptr};
    std::atomic<size_t> size{0};
    std::atomic<LockFreeFreeBlock*> next{nullptr};

    LockFreeFreeBlock() = default;
    LockFreeFreeBlock(void* p, size_t s) : ptr(p), size(s) {}
};

/**
 * @brief Cache-optimized free list for fast allocation
 */
class alignas(CACHE_LINE_SIZE) OptimizedFreeList {
private:
    std::atomic<LockFreeFreeBlock*> head_{nullptr};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> size_{0};

public:
    void push(LockFreeFreeBlock* node) noexcept {
        LockFreeFreeBlock* old_head = head_.load(std::memory_order_relaxed);
        do {
            node->next.store(old_head, std::memory_order_relaxed);
        } while (!head_.compare_exchange_weak(old_head, node,
                                            std::memory_order_release,
                                            std::memory_order_relaxed));
        size_.fetch_add(1, std::memory_order_relaxed);
    }

    LockFreeFreeBlock* pop() noexcept {
        LockFreeFreeBlock* head = head_.load(std::memory_order_acquire);
        while (head != nullptr) {
            LockFreeFreeBlock* next = head->next.load(std::memory_order_relaxed);
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

}  // namespace atom::memory

/**
 * @brief High-performance memory pool for efficient memory allocation and
 * deallocation
 *
 * This class provides a memory pool that allocates memory in chunks to reduce
 * the overhead of frequent allocations and deallocations. It includes various
 * optimization strategies, supports thread-safe operations, and provides
 * detailed memory usage statistics.
 *
 * @tparam T The type of objects to allocate
 * @tparam BlockSize The size of each memory block in bytes
 * @tparam Alignment Memory alignment requirement in bytes
 */
template <typename T, size_t BlockSize = 4096,
          size_t Alignment = alignof(std::max_align_t)>
class MemoryPool : public std::pmr::memory_resource {
public:
    /**
     * @brief Constructs a MemoryPool object
     *
     * @param block_size_strategy Memory block growth strategy
     * @param enable_lock_free Enable lock-free optimizations for single-threaded scenarios
     */
    explicit MemoryPool(
        std::unique_ptr<atom::memory::BlockSizeStrategy> block_size_strategy =
            std::make_unique<atom::memory::ExponentialBlockSizeStrategy>(),
        bool enable_lock_free = false)
        : block_size_strategy_(std::move(block_size_strategy)),
          lock_free_enabled_(enable_lock_free) {
        static_assert(BlockSize >= sizeof(T),
                      "BlockSize must be at least as large as sizeof(T)");
        static_assert(BlockSize % Alignment == 0,
                      "BlockSize must be a multiple of Alignment");

        // Initialize first memory chunk
        addNewChunk(BlockSize);

        // Initialize free block pool for lock-free operations
        if (lock_free_enabled_) {
            initializeFreeBlockPool();
        }
    }

    /**
     * @brief Move constructor
     */
    MemoryPool(MemoryPool&& other) noexcept
        : block_size_strategy_(std::move(other.block_size_strategy_)),
          free_list_(std::move(other.free_list_)) {
        std::unique_lock lock(other.mutex_);
        pool_ = std::move(other.pool_);
        tagged_allocations_ = std::move(other.tagged_allocations_);

        // Manually copy atomic values
        stats_.total_allocated = other.stats_.total_allocated.load();
        stats_.total_available = other.stats_.total_available.load();
        stats_.allocation_count = other.stats_.allocation_count.load();
        stats_.deallocation_count = other.stats_.deallocation_count.load();
        stats_.chunk_count = other.stats_.chunk_count.load();
    }

    /**
     * @brief Move assignment operator
     */
    MemoryPool& operator=(MemoryPool&& other) noexcept {
        if (this != &other) {
            std::unique_lock lock1(mutex_, std::defer_lock);
            std::unique_lock lock2(other.mutex_, std::defer_lock);
            std::lock(lock1, lock2);

            block_size_strategy_ = std::move(other.block_size_strategy_);
            pool_ = std::move(other.pool_);
            free_list_ = std::move(other.free_list_);
            tagged_allocations_ = std::move(other.tagged_allocations_);

            // Manually copy atomic values
            stats_.total_allocated = other.stats_.total_allocated.load();
            stats_.total_available = other.stats_.total_available.load();
            stats_.allocation_count = other.stats_.allocation_count.load();
            stats_.deallocation_count = other.stats_.deallocation_count.load();
            stats_.chunk_count = other.stats_.chunk_count.load();
        }
        return *this;
    }

    // Disable copying
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    /**
     * @brief Destructor
     */
    ~MemoryPool() override { reset(); }

    /**
     * @brief Allocates memory for n objects of type T
     *
     * @param n The number of objects to allocate
     * @return T* A pointer to the allocated memory
     * @throws atom::memory::MemoryPoolException if allocation fails
     */
    [[nodiscard]] T* allocate(size_t n) {
        const size_t numBytes = n * sizeof(T);
        if (numBytes > maxSize()) {
            throw atom::memory::MemoryPoolException(
                "Requested size exceeds maximum block size");
        }

        // Try optimized allocation first for better performance
        if (lock_free_enabled_) {
            T* result = allocateOptimized(numBytes);
            if (result) {
                updateStats(numBytes, true);
                return result;
            }
        }

        std::unique_lock lock(mutex_);
        T* result = nullptr;

        // First try to allocate from free list with improved search
        if (!free_list_.empty()) {
            // Use allocation hint for better cache locality
            size_t hint = allocation_hint_.load(std::memory_order_relaxed);
            auto it = free_list_.end();

            // If we have a size hint, try to find a block close to that size first
            if (hint > 0 && hint <= numBytes * 2) {
                it = std::find_if(free_list_.begin(), free_list_.end(),
                                 [numBytes, hint](const auto& block) {
                                     return block.size >= numBytes && block.size <= hint * 2;
                                 });
            }

            // Fall back to first-fit if hint-based search fails
            if (it == free_list_.end()) {
                it = std::find_if(free_list_.begin(), free_list_.end(),
                                 [numBytes](const auto& block) {
                                     return block.size >= numBytes;
                                 });
            }

            if (it != free_list_.end()) {
                result = static_cast<T*>(it->ptr);
                stats_.cache_hits.fetch_add(1, std::memory_order_relaxed);

                // Improved block splitting with better size thresholds
                if (it->size >= numBytes + sizeof(void*) + Alignment &&
                    it->size > numBytes * 1.5) {  // Only split if significantly larger
                    void* new_free = static_cast<char*>(it->ptr) + numBytes;
                    size_t new_size = it->size - numBytes;

                    free_list_.push_back({new_free, new_size});
                    it->size = numBytes;
                    stats_.split_operations.fetch_add(1, std::memory_order_relaxed);
                }

                free_list_.erase(it);
                updateStats(numBytes, true);

                // Prefetch allocated memory for better performance
                prefetchMemory(result, numBytes);
                return result;
            } else {
                stats_.cache_misses.fetch_add(1, std::memory_order_relaxed);
            }
        }

        // Allocate from existing chunks
        result = allocateFromExistingChunks(numBytes);
        if (result) {
            updateStats(numBytes, true);
            prefetchMemory(result, numBytes);
            return result;
        }

        // Need a new chunk
        result = allocateFromNewChunk(numBytes);
        updateStats(numBytes, true);
        prefetchMemory(result, numBytes);
        return result;
    }

    /**
     * @brief Allocates memory with a tag for tracking
     *
     * @param n Number of objects to allocate
     * @param tag Memory tag name
     * @param file Source file of allocation
     * @param line Line number of allocation
     * @return T* Pointer to allocated memory
     */
    [[nodiscard]] T* allocateTagged(size_t n, const std::string& tag,
                                    const std::string& file = "",
                                    int line = 0) {
        T* ptr = allocate(n);
        std::unique_lock lock(mutex_);
        tagged_allocations_[ptr] = atom::memory::MemoryTag(tag, file, line);
        return ptr;
    }

    /**
     * @brief Deallocates memory
     *
     * @param p Pointer to memory to deallocate
     * @param n Number of objects to deallocate
     */
    void deallocate(T* p, size_t n) {
        if (!p)
            return;

        const size_t numBytes = n * sizeof(T);
        auto start_time = std::chrono::high_resolution_clock::now();

        // Try lock-free deallocation first if enabled
        if (lock_free_enabled_) {
            auto* node = getFreeBlockNode();
            if (node) {
                node->ptr.store(p, std::memory_order_relaxed);
                node->size.store(numBytes, std::memory_order_relaxed);
                lock_free_list_.push(node);

                // Update timing statistics
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
                stats_.total_dealloc_time.fetch_add(duration, std::memory_order_relaxed);

                uint64_t current_max = stats_.max_dealloc_time.load();
                while (duration > current_max &&
                       !stats_.max_dealloc_time.compare_exchange_weak(current_max, duration)) {
                    // Keep trying until we successfully update or find a larger value
                }

                updateStats(numBytes, false);
                return;
            }
        }

        std::unique_lock lock(mutex_);

        // Remove any tags
        tagged_allocations_.erase(p);

        // Add to free list
        free_list_.push_back({p, numBytes});

        // Try to merge adjacent free blocks with improved coalescing
        size_t coalesced_bytes = coalesceFreelist();
        if (coalesced_bytes > 0) {
            stats_.coalesce_operations.fetch_add(1, std::memory_order_relaxed);
        }

        // Update timing statistics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
        stats_.total_dealloc_time.fetch_add(duration, std::memory_order_relaxed);

        uint64_t current_max = stats_.max_dealloc_time.load();
        while (duration > current_max &&
               !stats_.max_dealloc_time.compare_exchange_weak(current_max, duration)) {
            // Keep trying until we successfully update or find a larger value
        }

        updateStats(numBytes, false);
    }

    /**
     * @brief Checks if this memory resource is equal to another
     *
     * @param other The other memory resource to compare with
     * @return bool True if memory resources are equal, false otherwise
     */
    [[nodiscard]] auto do_is_equal(const std::pmr::memory_resource& other)
        const noexcept -> bool override {
        return this == &other;
    }

    /**
     * @brief Resets the memory pool, freeing all allocated memory
     */
    void reset() {
        std::unique_lock lock(mutex_);
        pool_.clear();
        free_list_.clear();
        tagged_allocations_.clear();
        stats_.reset();
    }

    /**
     * @brief Compacts the memory pool to reduce fragmentation
     *
     * @return Number of bytes compacted
     */
    size_t compact() {
        std::unique_lock lock(mutex_);
        size_t bytes_compacted = 0;

        // Sort free blocks by address
        std::sort(free_list_.begin(), free_list_.end(),
                  [](const auto& a, const auto& b) { return a.ptr < b.ptr; });

        // Merge adjacent blocks
        bytes_compacted = coalesceFreelist();

        return bytes_compacted;
    }

    /**
     * @brief Gets the total memory allocated by the pool
     *
     * @return Total allocated memory in bytes
     */
    [[nodiscard]] auto getTotalAllocated() const noexcept -> size_t {
        return stats_.total_allocated.load(std::memory_order_relaxed);
    }

    /**
     * @brief Gets the total memory available in the pool
     *
     * @return Total available memory in bytes
     */
    [[nodiscard]] auto getTotalAvailable() const noexcept -> size_t {
        return stats_.total_available.load(std::memory_order_relaxed);
    }

    /**
     * @brief Gets the allocation operation count
     *
     * @return Number of allocation operations
     */
    [[nodiscard]] auto getAllocationCount() const noexcept -> size_t {
        return stats_.allocation_count.load(std::memory_order_relaxed);
    }

    /**
     * @brief Gets the deallocation operation count
     *
     * @return Number of deallocation operations
     */
    [[nodiscard]] auto getDeallocationCount() const noexcept -> size_t {
        return stats_.deallocation_count.load(std::memory_order_relaxed);
    }

    /**
     * @brief Gets the current fragmentation ratio (0.0-1.0)
     *
     * @return Fragmentation ratio, 1.0 means completely fragmented
     */
    [[nodiscard]] auto getFragmentationRatio() const -> double {
        std::shared_lock lock(mutex_);

        if (free_list_.empty() || stats_.total_available.load() == 0) {
            return 0.0;
        }

        // Calculate average size of free blocks
        size_t total_free_size = 0;
        for (const auto& block : free_list_) {
            total_free_size += block.size;
        }

        size_t avg_free_size = total_free_size / free_list_.size();
        size_t largest_possible_block = stats_.total_available.load();

        // Fragmentation ratio = 1 - (average free block size / largest possible
        // block size)
        return 1.0 -
               (static_cast<double>(avg_free_size) / largest_possible_block);
    }

    /**
     * @brief Finds the memory tag associated with a given pointer
     *
     * @param ptr Pointer to look up
     * @return The tag associated with the pointer, if any
     */
    [[nodiscard]] std::optional<atom::memory::MemoryTag> findTag(
        void* ptr) const {
        std::shared_lock lock(mutex_);
        auto it = tagged_allocations_.find(ptr);
        if (it != tagged_allocations_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief Gets all tagged allocation information
     *
     * @return A copy of the pointer-to-tag mapping
     */
    [[nodiscard]] std::unordered_map<void*, atom::memory::MemoryTag>
    getTaggedAllocations() const {
        std::shared_lock lock(mutex_);
        return tagged_allocations_;
    }

    /**
     * @brief Adjusts pool size to accommodate expected allocation patterns
     *
     * @param expected_allocations Expected number of allocations
     * @param avg_size Expected average size per allocation
     */
    void reserve(size_t expected_allocations, size_t avg_size = sizeof(T)) {
        const size_t total_size = expected_allocations * avg_size;
        const size_t current_size = getTotalAvailable();

        if (total_size > current_size) {
            std::unique_lock lock(mutex_);
            addNewChunk(total_size - current_size);
        }
    }

    /**
     * @brief Get detailed performance statistics
     *
     * @return Enhanced statistics including performance metrics
     */
    [[nodiscard]] auto getDetailedStats() const -> atom::memory::MemoryPoolStatsSnapshot {
        std::shared_lock lock(mutex_);
        return stats_.snapshot();
    }

    /**
     * @brief Get cache performance metrics
     *
     * @return Cache hit ratio and related metrics
     */
    [[nodiscard]] auto getCachePerformance() const -> std::tuple<double, size_t, size_t> {
        std::shared_lock lock(mutex_);
        size_t hits = stats_.cache_hits.load();
        size_t misses = stats_.cache_misses.load();
        double hit_ratio = stats_.getCacheHitRatio();
        return std::make_tuple(hit_ratio, hits, misses);
    }

    /**
     * @brief Get timing performance metrics
     *
     * @return Average and maximum allocation/deallocation times
     */
    [[nodiscard]] auto getTimingPerformance() const -> std::tuple<double, double, uint64_t, uint64_t> {
        std::shared_lock lock(mutex_);
        double avg_alloc = stats_.getAverageAllocTime();
        double avg_dealloc = stats_.getAverageDeallocTime();
        uint64_t max_alloc = stats_.max_alloc_time.load();
        uint64_t max_dealloc = stats_.max_dealloc_time.load();
        return std::make_tuple(avg_alloc, avg_dealloc, max_alloc, max_dealloc);
    }

    /**
     * @brief Enable or disable lock-free optimizations
     *
     * @param enable Whether to enable lock-free optimizations
     */
    void setLockFreeMode(bool enable) {
        std::unique_lock lock(mutex_);
        if (enable && !lock_free_enabled_) {
            initializeFreeBlockPool();
        }
        lock_free_enabled_ = enable;
    }

protected:
    /**
     * @brief Allocates memory with a specified alignment
     *
     * @param bytes Number of bytes to allocate
     * @param alignment Memory alignment
     * @return Pointer to allocated memory
     * @throws atom::memory::MemoryPoolException if allocation fails
     */
    void* do_allocate(size_t bytes, size_t alignment) override {
        if (alignment <= Alignment && bytes <= maxSize()) {
            return allocate(bytes / sizeof(T) + (bytes % sizeof(T) ? 1 : 0));
        }

        // Fall back to aligned allocation
        void* ptr = aligned_alloc(alignment, bytes);
        if (!ptr) {
            throw atom::memory::MemoryPoolException(
                "Aligned allocation failed");
        }

        std::unique_lock lock(mutex_);
        updateStats(bytes, true);
        return ptr;
    }

    /**
     * @brief Deallocates memory with a specified alignment
     *
     * @param p Pointer to memory to deallocate
     * @param bytes Number of bytes to deallocate
     * @param alignment Memory alignment
     */
    void do_deallocate(void* p, size_t bytes, size_t alignment) override {
        if (alignment <= Alignment && bytes <= maxSize() && isFromPool(p)) {
            deallocate(static_cast<T*>(p),
                       bytes / sizeof(T) + (bytes % sizeof(T) ? 1 : 0));
        } else {
            std::unique_lock lock(mutex_);
            updateStats(bytes, false);
            free(p);  // Use free for aligned-allocated memory
        }
    }

private:
    /**
     * @brief Structure representing a memory chunk
     */
    struct Chunk {
        size_t size;  ///< Size of the chunk
        size_t used;  ///< Amount of memory used in the chunk
        std::unique_ptr<std::byte[]> memory;  ///< Memory block

        /**
         * @brief Constructs a Chunk object
         *
         * @param s Size of the chunk
         */
        explicit Chunk(size_t s)
            : size(s), used(0), memory(std::make_unique<std::byte[]>(s)) {}
    };

    /**
     * @brief Structure representing a free memory block
     */
    struct FreeBlock {
        void* ptr;    ///< Pointer to the free block
        size_t size;  ///< Size of the free block
    };

    /**
     * @brief Gets the maximum size of a memory block
     *
     * @return Maximum size of a memory block
     */
    [[nodiscard]] constexpr size_t maxSize() const noexcept {
        return BlockSize;
    }

    /**
     * @brief Allocates from existing chunks
     *
     * @param num_bytes Number of bytes to allocate
     * @return Pointer to allocated memory, nullptr if allocation fails
     */
    T* allocateFromExistingChunks(size_t num_bytes) {
        for (auto& chunk : pool_) {
            // Ensure alignment
            size_t aligned_used =
                (chunk.used + Alignment - 1) & ~(Alignment - 1);

            if (chunk.size - aligned_used >= num_bytes) {
                T* p = reinterpret_cast<T*>(chunk.memory.get() + aligned_used);
                chunk.used = aligned_used + num_bytes;
                return p;
            }
        }
        return nullptr;
    }

    /**
     * @brief Allocates from a new chunk
     *
     * @param num_bytes Number of bytes to allocate
     * @return Pointer to allocated memory
     */
    T* allocateFromNewChunk(size_t num_bytes) {
        // Use strategy to calculate new chunk size
        size_t new_chunk_size = std::max(
            num_bytes, block_size_strategy_->calculate(
                           pool_.empty() ? BlockSize : pool_.back().size));

        // Ensure new chunk size is a multiple of alignment
        new_chunk_size = (new_chunk_size + Alignment - 1) & ~(Alignment - 1);

        // Add new chunk
        addNewChunk(new_chunk_size);

        // Allocate from new chunk
        Chunk& newChunk = pool_.back();
        // Ensure alignment
        size_t aligned_used =
            (newChunk.used + Alignment - 1) & ~(Alignment - 1);
        T* p = reinterpret_cast<T*>(newChunk.memory.get() + aligned_used);
        newChunk.used = aligned_used + num_bytes;

        return p;
    }

    /**
     * @brief Adds a new memory chunk
     *
     * @param size Size of the new chunk
     */
    void addNewChunk(size_t size) {
        // Ensure size is a multiple of alignment
        size_t aligned_size = (size + Alignment - 1) & ~(Alignment - 1);
        pool_.emplace_back(aligned_size);
        stats_.total_available.fetch_add(aligned_size,
                                         std::memory_order_relaxed);
        stats_.chunk_count.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Enhanced coalescing algorithm with better performance
     *
     * @return Number of bytes coalesced
     */
    size_t coalesceFreelist() {
        if (free_list_.size() <= 1)
            return 0;

        size_t bytes_coalesced = 0;
        size_t original_size = free_list_.size();

        // Sort by address for efficient merging
        std::sort(free_list_.begin(), free_list_.end(),
                  [](const auto& a, const auto& b) { return a.ptr < b.ptr; });

        // Use two-pointer technique for efficient merging
        size_t write_idx = 0;
        for (size_t read_idx = 0; read_idx < free_list_.size(); ++read_idx) {
            if (write_idx != read_idx) {
                free_list_[write_idx] = free_list_[read_idx];
            }

            // Try to merge with subsequent blocks
            while (read_idx + 1 < free_list_.size()) {
                char* end_of_current = static_cast<char*>(free_list_[write_idx].ptr) +
                                      free_list_[write_idx].size;
                char* start_of_next = static_cast<char*>(free_list_[read_idx + 1].ptr);

                if (end_of_current == start_of_next) {
                    // Blocks are adjacent, merge them
                    free_list_[write_idx].size += free_list_[read_idx + 1].size;
                    bytes_coalesced += free_list_[read_idx + 1].size;
                    ++read_idx;  // Skip the merged block
                } else {
                    break;  // No more adjacent blocks
                }
            }
            ++write_idx;
        }

        // Resize the vector to remove merged blocks
        free_list_.resize(write_idx);

        // Update fragmentation statistics
        if (original_size > write_idx) {
            stats_.fragmentation_events.fetch_add(original_size - write_idx,
                                                 std::memory_order_relaxed);
        }

        return bytes_coalesced;
    }

    /**
     * @brief Checks if a pointer is from the pool
     *
     * @param p Pointer to check
     * @return True if pointer is from pool, false otherwise
     */
    [[nodiscard]] bool isFromPool(void* p) const noexcept {
        auto* ptr = reinterpret_cast<std::byte*>(p);
        for (const auto& chunk : pool_) {
            if (ptr >= chunk.memory.get() &&
                ptr < chunk.memory.get() + chunk.size) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Updates statistics with enhanced tracking
     *
     * @param num_bytes Number of bytes to update
     * @param is_allocation true for allocation, false for deallocation
     */
    void updateStats(size_t num_bytes, bool is_allocation) noexcept {
        if (is_allocation) {
            stats_.total_allocated.fetch_add(num_bytes, std::memory_order_relaxed);
            stats_.total_available.fetch_sub(num_bytes, std::memory_order_relaxed);
            stats_.allocation_count.fetch_add(1, std::memory_order_relaxed);

            // Update peak allocated memory
            size_t current_allocated = stats_.total_allocated.load();
            size_t current_peak = stats_.peak_allocated.load();
            while (current_allocated > current_peak &&
                   !stats_.peak_allocated.compare_exchange_weak(current_peak, current_allocated)) {
                // Keep trying until we successfully update or find a larger value
            }
        } else {
            stats_.total_allocated.fetch_sub(num_bytes, std::memory_order_relaxed);
            stats_.total_available.fetch_add(num_bytes, std::memory_order_relaxed);
            stats_.deallocation_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

private:
    std::unique_ptr<atom::memory::BlockSizeStrategy>
        block_size_strategy_;           ///< Block size strategy
    std::vector<Chunk> pool_;           ///< Pool of memory chunks
    std::vector<FreeBlock> free_list_;  ///< List of free blocks
    mutable std::shared_mutex mutex_;   ///< Mutex to protect shared resources
    atom::memory::MemoryPoolStats stats_;  ///< Memory pool statistics
    std::unordered_map<void*, atom::memory::MemoryTag>
        tagged_allocations_;  ///< Tagged allocations

    // Lock-free optimization members
    bool lock_free_enabled_{false};    ///< Enable lock-free optimizations
    atom::memory::OptimizedFreeList lock_free_list_; ///< Lock-free free list
    std::vector<std::unique_ptr<atom::memory::LockFreeFreeBlock>> free_block_pool_; ///< Pool of free block nodes
    std::atomic<size_t> free_block_pool_index_{0}; ///< Index for free block pool

    // Performance optimization members
    alignas(CACHE_LINE_SIZE) std::atomic<void*> last_allocated_{nullptr}; ///< Last allocated pointer for locality
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> allocation_hint_{0}; ///< Hint for next allocation size

    /**
     * @brief Initialize the free block pool for lock-free operations
     */
    void initializeFreeBlockPool() {
        constexpr size_t INITIAL_POOL_SIZE = 1024;
        free_block_pool_.reserve(INITIAL_POOL_SIZE);
        for (size_t i = 0; i < INITIAL_POOL_SIZE; ++i) {
            free_block_pool_.emplace_back(std::make_unique<atom::memory::LockFreeFreeBlock>());
        }
    }

    /**
     * @brief Get a free block node from the pool
     */
    atom::memory::LockFreeFreeBlock* getFreeBlockNode() {
        if (lock_free_enabled_) {
            size_t index = free_block_pool_index_.fetch_add(1, std::memory_order_relaxed);
            if (index < free_block_pool_.size()) {
                return free_block_pool_[index].get();
            }
        }
        return new atom::memory::LockFreeFreeBlock();
    }

    /**
     * @brief Prefetch memory for better cache performance
     */
    void prefetchMemory(void* ptr, size_t size) const noexcept {
        if (ptr && size > 0) {
            // Prefetch the memory region
            char* mem = static_cast<char*>(ptr);
            for (size_t offset = 0; offset < size; offset += CACHE_LINE_SIZE) {
                _mm_prefetch(mem + offset, _MM_HINT_T0);
            }
        }
    }

    /**
     * @brief Optimized allocation with timing and cache optimization
     */
    T* allocateOptimized(size_t numBytes) {
        auto start_time = std::chrono::high_resolution_clock::now();

        T* result = nullptr;

        // Try lock-free allocation first if enabled
        if (lock_free_enabled_ && !lock_free_list_.empty()) {
            auto* node = lock_free_list_.pop();
            if (node && node->size.load() >= numBytes) {
                result = static_cast<T*>(node->ptr.load());
                stats_.cache_hits.fetch_add(1, std::memory_order_relaxed);
            } else if (node) {
                // Put it back if size doesn't match
                lock_free_list_.push(node);
            }
        }

        if (!result) {
            stats_.cache_misses.fetch_add(1, std::memory_order_relaxed);
            // Fall back to regular allocation
            result = allocateFromExistingChunks(numBytes);
            if (!result) {
                result = allocateFromNewChunk(numBytes);
            }
        }

        // Update timing statistics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
        stats_.total_alloc_time.fetch_add(duration, std::memory_order_relaxed);

        uint64_t current_max = stats_.max_alloc_time.load();
        while (duration > current_max &&
               !stats_.max_alloc_time.compare_exchange_weak(current_max, duration)) {
            // Keep trying until we successfully update or find a larger value
        }

        // Prefetch allocated memory
        if (result) {
            prefetchMemory(result, numBytes);
            last_allocated_.store(result, std::memory_order_relaxed);
            allocation_hint_.store(numBytes, std::memory_order_relaxed);
        }

        return result;
    }
};

#endif  // ATOM_MEMORY_MEMORY_POOL_HPP
