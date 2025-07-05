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

#ifdef ATOM_USE_BOOST
#include <boost/pool/pool.hpp>
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
 * @brief Memory pool statistics
 */
struct MemoryPoolStats {
    std::atomic<size_t> total_allocated{0};   ///< Total allocated bytes
    std::atomic<size_t> total_available{0};   ///< Total available bytes
    std::atomic<size_t> allocation_count{0};  ///< Allocation operation count
    std::atomic<size_t> deallocation_count{
        0};                              ///< Deallocation operation count
    std::atomic<size_t> chunk_count{0};  ///< Number of memory chunks

    void reset() noexcept {
        total_allocated = 0;
        total_available = 0;
        allocation_count = 0;
        deallocation_count = 0;
        chunk_count = 0;
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
     */
    explicit MemoryPool(
        std::unique_ptr<atom::memory::BlockSizeStrategy> block_size_strategy =
            std::make_unique<atom::memory::ExponentialBlockSizeStrategy>())
        : block_size_strategy_(std::move(block_size_strategy)) {
        static_assert(BlockSize >= sizeof(T),
                      "BlockSize must be at least as large as sizeof(T)");
        static_assert(BlockSize % Alignment == 0,
                      "BlockSize must be a multiple of Alignment");

        // Initialize first memory chunk
        addNewChunk(BlockSize);
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

        std::unique_lock lock(mutex_);
        T* result = nullptr;

        // First try to allocate from free list
        if (!free_list_.empty() && free_list_.front().size >= numBytes) {
            auto it = std::find_if(free_list_.begin(), free_list_.end(),
                                   [numBytes](const auto& block) {
                                       return block.size >= numBytes;
                                   });

            if (it != free_list_.end()) {
                result = static_cast<T*>(it->ptr);

                // If free block is much larger than requested size, consider
                // splitting
                if (it->size >= numBytes + sizeof(void*) + Alignment) {
                    void* new_free = static_cast<char*>(it->ptr) + numBytes;
                    size_t new_size = it->size - numBytes;

                    free_list_.push_back({new_free, new_size});
                    it->size = numBytes;
                }

                free_list_.erase(it);
                updateStats(numBytes, true);
                return result;
            }
        }

        // Allocate from existing chunks
        result = allocateFromExistingChunks(numBytes);
        if (result) {
            updateStats(numBytes, true);
            return result;
        }

        // Need a new chunk
        result = allocateFromNewChunk(numBytes);
        updateStats(numBytes, true);
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
        std::unique_lock lock(mutex_);

        // Remove any tags
        tagged_allocations_.erase(p);

        // Add to free list
        free_list_.push_back({p, numBytes});

        // Try to merge adjacent free blocks
        coalesceFreelist();

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
     * @brief Coalesces adjacent blocks in the free list
     *
     * @return Number of bytes coalesced
     */
    size_t coalesceFreelist() {
        if (free_list_.size() <= 1)
            return 0;

        size_t bytes_coalesced = 0;

        // Sort by address
        std::sort(free_list_.begin(), free_list_.end(),
                  [](const auto& a, const auto& b) { return a.ptr < b.ptr; });

        // Merge adjacent blocks
        for (auto it = free_list_.begin(); it != free_list_.end() - 1;) {
            auto next_it = it + 1;

            char* end_of_current = static_cast<char*>(it->ptr) + it->size;

            if (end_of_current == static_cast<char*>(next_it->ptr)) {
                // Blocks are adjacent, merge them
                it->size += next_it->size;
                bytes_coalesced += next_it->size;
                free_list_.erase(next_it);
                // Don't increment it, since we removed next_it
            } else {
                ++it;
            }
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
     * @brief Updates statistics
     *
     * @param num_bytes Number of bytes to update
     * @param is_allocation true for allocation, false for deallocation
     */
    void updateStats(size_t num_bytes, bool is_allocation) noexcept {
        if (is_allocation) {
            stats_.total_allocated.fetch_add(num_bytes,
                                             std::memory_order_relaxed);
            stats_.total_available.fetch_sub(num_bytes,
                                             std::memory_order_relaxed);
            stats_.allocation_count.fetch_add(1, std::memory_order_relaxed);
        } else {
            stats_.total_allocated.fetch_sub(num_bytes,
                                             std::memory_order_relaxed);
            stats_.total_available.fetch_add(num_bytes,
                                             std::memory_order_relaxed);
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
};

#endif  // ATOM_MEMORY_MEMORY_POOL_HPP
