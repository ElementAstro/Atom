/*
 * math_memory.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Custom memory pool and allocator for math operations

**************************************************/

#ifndef ATOM_ALGORITHM_MATH_MATH_MEMORY_HPP
#define ATOM_ALGORITHM_MATH_MATH_MEMORY_HPP

#include <shared_mutex>

#include "atom/algorithm/core/rust_numeric.hpp"

namespace atom::algorithm {

/**
 * @brief Custom memory pool for efficient allocation in math operations
 */
class MathMemoryPool {
public:
    /**
     * @brief Get the singleton instance
     *
     * @return Reference to the singleton instance
     */
    static MathMemoryPool& getInstance() noexcept;

    /**
     * @brief Allocate memory from the pool
     *
     * @param size Size in bytes to allocate
     * @return void* Pointer to allocated memory
     */
    [[nodiscard]] void* allocate(usize size);

    /**
     * @brief Return memory to the pool
     *
     * @param ptr Pointer to memory
     * @param size Size of the allocation
     */
    void deallocate(void* ptr, usize size) noexcept;

private:
    MathMemoryPool() = default;
    ~MathMemoryPool();
    MathMemoryPool(const MathMemoryPool&) = delete;
    MathMemoryPool& operator=(const MathMemoryPool&) = delete;
    MathMemoryPool(MathMemoryPool&&) = delete;
    MathMemoryPool& operator=(MathMemoryPool&&) = delete;

    std::shared_mutex mutex_;
    // Implementation details hidden
};

/**
 * @brief Custom allocator that uses MathMemoryPool
 *
 * @tparam T Type to allocate
 */
template <typename T>
class MathAllocator {
public:
    using value_type = T;

    MathAllocator() noexcept = default;

    template <typename U>
    MathAllocator(const MathAllocator<U>&) noexcept {}

    [[nodiscard]] T* allocate(usize n);
    void deallocate(T* p, usize n) noexcept;

    template <typename U>
    bool operator==(const MathAllocator<U>&) const noexcept {
        return true;
    }

    template <typename U>
    bool operator!=(const MathAllocator<U>&) const noexcept {
        return false;
    }
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_MATH_MATH_MEMORY_HPP
