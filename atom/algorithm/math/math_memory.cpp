/*
 * math_memory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Custom memory pool and allocator - implementations

**************************************************/

#include "math_memory.hpp"

#include <limits>
#include <memory_resource>
#include <mutex>
#include <new>
#include <shared_mutex>

#ifdef ATOM_USE_BOOST
#include <boost/pool/object_pool.hpp>
#endif

namespace atom::algorithm {

// MathMemoryPool implementation
namespace {

// Memory pools for different block sizes
#ifdef ATOM_USE_BOOST
boost::object_pool<char[SMALL_BLOCK_SIZE]> smallPool;
boost::object_pool<char[MEDIUM_BLOCK_SIZE]> mediumPool;
boost::object_pool<char[LARGE_BLOCK_SIZE]> largePool;
#else
std::pmr::synchronized_pool_resource memoryPool;
#endif
}  // namespace

MathMemoryPool& MathMemoryPool::getInstance() noexcept {
    static MathMemoryPool instance;
    return instance;
}

void* MathMemoryPool::allocate(usize size) {
#ifdef ATOM_USE_BOOST
    std::unique_lock lock(mutex_);
    if (size <= SMALL_BLOCK_SIZE) {
        return smallPool.malloc();
    } else if (size <= MEDIUM_BLOCK_SIZE) {
        return mediumPool.malloc();
    } else if (size <= LARGE_BLOCK_SIZE) {
        return largePool.malloc();
    } else {
        return ::operator new(size);
    }
#else
    return memoryPool.allocate(size);
#endif
}

void MathMemoryPool::deallocate(void* ptr, usize size) noexcept {
#ifdef ATOM_USE_BOOST
    std::unique_lock lock(mutex_);
    if (size <= SMALL_BLOCK_SIZE) {
        smallPool.free(static_cast<char(*)[SMALL_BLOCK_SIZE]>(ptr));
    } else if (size <= MEDIUM_BLOCK_SIZE) {
        mediumPool.free(static_cast<char(*)[MEDIUM_BLOCK_SIZE]>(ptr));
    } else if (size <= LARGE_BLOCK_SIZE) {
        largePool.free(static_cast<char(*)[LARGE_BLOCK_SIZE]>(ptr));
    } else {
        ::operator delete(ptr);
    }
#else
    memoryPool.deallocate(ptr, size);
#endif
}

MathMemoryPool::~MathMemoryPool() {
    // Cleanup is automatically handled by member destructors
}

// MathAllocator implementation
template <typename T>
T* MathAllocator<T>::allocate(usize n) {
    if (n > std::numeric_limits<usize>::max() / sizeof(T)) {
        throw std::bad_alloc();
    }

    void* ptr = MathMemoryPool::getInstance().allocate(n * sizeof(T));
    if (!ptr) {
        throw std::bad_alloc();
    }

    return static_cast<T*>(ptr);
}

template <typename T>
void MathAllocator<T>::deallocate(T* p, usize n) noexcept {
    MathMemoryPool::getInstance().deallocate(p, n * sizeof(T));
}

// Explicit template instantiations for MathAllocator
template class MathAllocator<i32>;
template class MathAllocator<f32>;
template class MathAllocator<f64>;
template class MathAllocator<u64>;

}  // namespace atom::algorithm
