/*
 * atom/memory/memory_pool.hpp
 *
 * Copyright (C) 2024 Max Qian <lightapt.com>
 */

#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

namespace atom {
namespace memory {

/**
 * @brief High-performance fixed-size block memory pool
 *
 * Specialized for efficiently allocating and deallocating fixed-size memory
 * blocks. Reduces memory fragmentation and system call overhead for frequent
 * small object operations.
 *
 * @tparam BlockSize Size of each memory block in bytes
 * @tparam BlocksPerChunk Number of blocks per chunk
 */
template <std::size_t BlockSize = 64, std::size_t BlocksPerChunk = 1024>
class MemoryPool {
private:
    struct Block {
        Block* next;
    };

    struct Chunk {
        alignas(std::max_align_t)
            std::array<std::byte, BlockSize * BlocksPerChunk> memory;

        constexpr Chunk() noexcept {
            static_assert(BlockSize >= sizeof(Block), "Block size too small");
        }
    };

    Block* free_list_ = nullptr;
    std::vector<std::unique_ptr<Chunk>> chunks_;
    mutable std::mutex mutex_;
    std::size_t allocated_blocks_ = 0;
    std::size_t total_blocks_ = 0;

    void allocate_new_chunk() {
        auto chunk = std::make_unique<Chunk>();

        for (std::size_t i = 0; i < BlocksPerChunk; ++i) {
            auto* block =
                reinterpret_cast<Block*>(&chunk->memory[i * BlockSize]);
            block->next = free_list_;
            free_list_ = block;
        }

        chunks_.push_back(std::move(chunk));
        total_blocks_ += BlocksPerChunk;
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
        std::lock_guard lock(mutex_);

        if ((free_list_ == nullptr)) {
            allocate_new_chunk();
        }

        Block* block = free_list_;
        free_list_ = block->next;
        ++allocated_blocks_;

        return static_cast<void*>(block);
    }

    /**
     * @brief Deallocates a memory block
     * @param ptr Pointer to memory block to deallocate
     */
    void deallocate(void* ptr) noexcept {
        if ((!ptr))
            return;

        std::lock_guard lock(mutex_);

        Block* block = static_cast<Block*>(ptr);
        block->next = free_list_;
        free_list_ = block;
        --allocated_blocks_;
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
};

/**
 * @brief Generic object pool based on MemoryPool
 *
 * Efficiently allocates and recycles objects of a specific type.
 *
 * @tparam T Object type
 * @tparam BlocksPerChunk Number of objects per chunk
 */
template <typename T, std::size_t BlocksPerChunk = 1024>
class ObjectPool {
private:
    static constexpr std::size_t block_size =
        ((sizeof(T) + alignof(std::max_align_t) - 1) /
         alignof(std::max_align_t)) *
        alignof(std::max_align_t);

    MemoryPool<block_size, BlocksPerChunk> memory_pool_;

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
            return new (memory) T(std::forward<Args>(args)...);
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
    void reset() noexcept { memory_pool_.reset(); }
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