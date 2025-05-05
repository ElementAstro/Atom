/*
 * atom/memory/memory_pool.hpp
 *
 * Copyright (C) 2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-11

Description: High-performance memory pool implementation

**************************************************/

#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>
#include "../macro.hpp"

namespace atom {
namespace memory {

/**
 * @brief High-performance fixed-size block memory pool
 *
 * This memory pool is specialized for efficiently allocating and deallocating
 * fixed-size memory blocks. It is suitable for scenarios involving frequent
 * creation and destruction of numerous small objects, reducing memory
 * fragmentation and system call overhead.
 *
 * @tparam BlockSize The size of each memory block in bytes.
 * @tparam BlocksPerChunk The number of blocks contained in each chunk.
 */
template <std::size_t BlockSize = 64, std::size_t BlocksPerChunk = 1024>
class MemoryPool {
private:
    // Linked list node structure for memory blocks
    struct Block {
        // Pointer to the next available block
        Block* next;
    };

    // Memory chunk structure
    struct Chunk {
        // A contiguous block of memory used for allocating multiple small
        // blocks
        std::array<std::byte, BlockSize * BlocksPerChunk> memory;

        // Constructor, initializes the memory block linked list
        Chunk() {
            static_assert(BlockSize >= sizeof(Block), "Block size too small");
        }
    };

    // Head pointer to the list of available blocks
    Block* free_list_ = nullptr;

    // Stores all allocated memory chunks
    std::vector<std::unique_ptr<Chunk>> chunks_;

    // Mutex for thread-safe operations
    std::mutex mutex_;

    // Number of blocks allocated but not yet deallocated
    std::size_t allocated_blocks_ = 0;

    // Total number of blocks
    std::size_t total_blocks_ = 0;

    // Allocates a new chunk when the existing blocks are exhausted
    void allocate_new_chunk() {
        auto chunk = std::make_unique<Chunk>();

        // Link all blocks in the new chunk to the free list
        for (std::size_t i = 0; i < BlocksPerChunk; ++i) {
            auto* block =
                reinterpret_cast<Block*>(&chunk->memory[i * BlockSize]);
            block->next = free_list_;
            free_list_ = block;
        }

        // Add to the chunk list and update statistics
        chunks_.push_back(std::move(chunk));
        total_blocks_ += BlocksPerChunk;
    }

public:
    // Static assertions to ensure block size is sufficient for Block struct and
    // meets alignment requirements
    static_assert(BlockSize >= sizeof(Block),
                  "Block size must be at least sizeof(Block)");
    static_assert(
        BlockSize % alignof(std::max_align_t) == 0,
        "Block size must be a multiple of std::max_align_t alignment");

    /**
     * @brief Default constructor
     */
    MemoryPool() = default;

    /**
     * @brief Destructor, releases all memory
     */
    ~MemoryPool() = default;

    // Disable copying
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    // Allow moving
    MemoryPool(MemoryPool&&) noexcept = default;
    MemoryPool& operator=(MemoryPool&&) noexcept = default;

    /**
     * @brief Allocates a memory block
     *
     * @return void* Pointer to the newly allocated memory block
     */
    ATOM_NODISCARD void* allocate() {
        std::lock_guard lock(mutex_);

        if (free_list_ == nullptr) {
            // Allocate a new chunk if no blocks are available
            allocate_new_chunk();
        }

        // Get a block from the free list
        Block* block = free_list_;
        free_list_ = block->next;
        ++allocated_blocks_;

        return static_cast<void*>(block);
    }

    /**
     * @brief Deallocates a previously allocated memory block
     *
     * @param ptr Pointer to the memory block to deallocate
     */
    void deallocate(void* ptr) ATOM_NOEXCEPT {
        if (!ptr)
            return;

        std::lock_guard lock(mutex_);

        // Return the block to the free list
        Block* block = static_cast<Block*>(ptr);
        block->next = free_list_;
        free_list_ = block;

        --allocated_blocks_;
    }

    /**
     * @brief Gets the statistics of the memory pool
     *
     * @return std::pair<std::size_t, std::size_t> A pair containing the number
     * of allocated blocks and the total number of blocks
     */
    std::pair<std::size_t, std::size_t> get_stats() const {
        std::lock_guard lock(mutex_);
        return {allocated_blocks_, total_blocks_};
    }

    /**
     * @brief Checks if the memory pool is empty (no allocated blocks)
     *
     * @return bool True if there are no allocated blocks, false otherwise
     */
    bool is_empty() const {
        std::lock_guard lock(mutex_);
        return allocated_blocks_ == 0;
    }

    /**
     * @brief Resets the memory pool, marking all blocks as available
     *
     * Note: This does not release the allocated memory, only resets the
     * internal state.
     */
    void reset() {
        std::lock_guard lock(mutex_);

        // Rebuild the free block list
        free_list_ = nullptr;
        for (auto& chunk : chunks_) {
            for (std::size_t i = 0; i < BlocksPerChunk; ++i) {
                auto* block =
                    reinterpret_cast<Block*>(&chunk->memory[i * BlockSize]);
                block->next = free_list_;
                free_list_ = block;
            }
        }

        // Update statistics
        allocated_blocks_ = 0;
    }
};

/**
 * @brief Generic object pool based on MemoryPool
 *
 * Efficiently allocates and recycles objects of a specific type, avoiding
 * frequent memory allocation and deallocation operations.
 *
 * @tparam T The type of objects in the pool
 * @tparam BlocksPerChunk The number of objects contained in each memory chunk
 */
template <typename T, std::size_t BlocksPerChunk = 1024>
class ObjectPool {
private:
    // Calculate the required block size for each object (considering alignment)
    static constexpr std::size_t block_size =
        ((sizeof(T) + alignof(std::max_align_t) - 1) /
         alignof(std::max_align_t)) *
        alignof(std::max_align_t);

    // Internal memory pool used
    MemoryPool<block_size, BlocksPerChunk> memory_pool_;

public:
    /**
     * @brief Default constructor
     */
    ObjectPool() = default;

    /**
     * @brief Destructor
     */
    ~ObjectPool() = default;

    // Disable copying
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    // Allow moving
    ObjectPool(ObjectPool&&) noexcept = default;
    ObjectPool& operator=(ObjectPool&&) noexcept = default;

    /**
     * @brief Allocates and constructs an object
     *
     * @tparam Args Constructor argument types
     * @param args Arguments passed to the object's constructor
     * @return T* Pointer to the newly created object
     */
    template <typename... Args>
    ATOM_NODISCARD T* allocate(Args&&... args) {
        // Allocate memory
        void* memory = memory_pool_.allocate();

        try {
            // Construct the object in the allocated memory
            return new (memory) T(std::forward<Args>(args)...);
        } catch (...) {
            // If construction fails, deallocate memory and rethrow the
            // exception
            memory_pool_.deallocate(memory);
            throw;
        }
    }

    /**
     * @brief Destructs and deallocates an object
     *
     * @param ptr Pointer to the object to deallocate
     */
    void deallocate(T* ptr) ATOM_NOEXCEPT {
        if (!ptr)
            return;

        // Call the object's destructor
        ptr->~T();

        // Deallocate the memory
        memory_pool_.deallocate(static_cast<void*>(ptr));
    }

    /**
     * @brief Gets the statistics of the object pool
     *
     * @return std::pair<std::size_t, std::size_t>
     * A pair containing the number of allocated objects and the total capacity
     */
    std::pair<std::size_t, std::size_t> get_stats() const {
        return memory_pool_.get_stats();
    }

    /**
     * @brief Checks if the object pool is empty (no allocated objects)
     *
     * @return bool True if there are no allocated objects, false otherwise
     */
    bool is_empty() const { return memory_pool_.is_empty(); }

    /**
     * @brief Resets the object pool
     *
     * Warning: This will invalidate all pointers to allocated but not yet
     * deallocated objects!
     */
    void reset() { memory_pool_.reset(); }
};

/**
 * @brief Smart pointer using an ObjectPool
 *
 * Similar to std::unique_ptr, but uses an ObjectPool for memory management,
 * providing automatic resource management and performance optimization.
 *
 * @tparam T The type of the object managed by the pointer
 */
template <typename T>
class PoolPtr {
private:
    T* ptr_ = nullptr;
    ObjectPool<T>* pool_ = nullptr;

public:
    /**
     * @brief Default constructor
     */
    PoolPtr() noexcept = default;

    /**
     * @brief Constructs from an object pointer and an object pool
     *
     * @param ptr Object pointer
     * @param pool Pointer to the object pool
     */
    explicit PoolPtr(T* ptr, ObjectPool<T>* pool) noexcept
        : ptr_(ptr), pool_(pool) {}

    /**
     * @brief Destructor, releases the managed object
     */
    ~PoolPtr() { reset(); }

    // Disable copying
    PoolPtr(const PoolPtr&) = delete;
    PoolPtr& operator=(const PoolPtr&) = delete;

    /**
     * @brief Move constructor
     */
    PoolPtr(PoolPtr&& other) noexcept : ptr_(other.ptr_), pool_(other.pool_) {
        other.ptr_ = nullptr;
        other.pool_ = nullptr;
    }

    /**
     * @brief Move assignment operator
     */
    PoolPtr& operator=(PoolPtr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            pool_ = other.pool_;
            other.ptr_ = nullptr;
            other.pool_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief Resets the pointer, releasing the currently managed object
     *
     * @param ptr New object pointer, defaults to nullptr
     * @param pool New object pool pointer, defaults to nullptr
     */
    void reset(T* ptr = nullptr, ObjectPool<T>* pool = nullptr) noexcept {
        if (ptr_ && pool_) {
            pool_->deallocate(ptr_);
        }
        ptr_ = ptr;
        pool_ = pool;
    }

    /**
     * @brief Returns the pointer and releases ownership
     *
     * @return T* The managed object pointer
     */
    T* release() noexcept {
        T* ptr = ptr_;
        ptr_ = nullptr;
        pool_ = nullptr;
        return ptr;
    }

    /**
     * @brief Gets the managed object pointer
     *
     * @return T* Pointer to the managed object
     */
    T* get() const noexcept { return ptr_; }

    /**
     * @brief Dereference operator
     *
     * @return T& Reference to the managed object
     */
    T& operator*() const {
        assert(ptr_ != nullptr);
        return *ptr_;
    }

    /**
     * @brief Member access operator
     *
     * @return T* Pointer to the managed object
     */
    T* operator->() const noexcept {
        assert(ptr_ != nullptr);
        return ptr_;
    }

    /**
     * @brief Boolean conversion operator
     *
     * @return true If managing a valid object
     * @return false If not managing an object
     */
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    /**
     * @brief Swaps the contents of two PoolPtrs
     *
     * @param other The other PoolPtr to swap with
     */
    void swap(PoolPtr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(pool_, other.pool_);
    }
};

/**
 * @brief Helper function to create a PoolPtr from an ObjectPool
 *
 * @tparam T Object type
 * @tparam Args Constructor argument types
 * @param pool Reference to the object pool
 * @param args Arguments passed to the constructor
 * @return PoolPtr<T> Smart pointer managing the newly created object
 */
template <typename T, typename... Args>
PoolPtr<T> make_pool_ptr(ObjectPool<T>& pool, Args&&... args) {
    return PoolPtr<T>(pool.allocate(std::forward<Args>(args)...), &pool);
}

}  // namespace memory
}  // namespace atom