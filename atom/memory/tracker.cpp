/**
 * @file tracker.cpp
 * @brief Implementation of global operator new/delete overloads for memory tracking
 *
 * This file contains the definitions of global operator new/delete overloads
 * that are used when ATOM_MEMORY_TRACKING_ENABLED is defined. These must be
 * in a separate compilation unit to avoid ODR (One Definition Rule) violations.
 *
 * @author Max Qian
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "tracker.hpp"

#ifdef ATOM_MEMORY_TRACKING_ENABLED

#include <cstdlib>
#include <new>

/**
 * @brief Global operator new overload for memory tracking
 * @param size Size of memory to allocate
 * @return Pointer to allocated memory
 * @throws std::bad_alloc if allocation fails
 */
void* operator new(size_t size) {
    void* ptr = std::malloc(size);
    if (!ptr)
        throw std::bad_alloc();
    ATOM_TRACK_ALLOC(ptr, size);
    return ptr;
}

/**
 * @brief Global operator delete overload for memory tracking
 * @param ptr Pointer to memory to deallocate
 */
void operator delete(void* ptr) noexcept {
    ATOM_TRACK_FREE(ptr);
    std::free(ptr);
}

/**
 * @brief Global operator new[] overload for memory tracking
 * @param size Size of memory to allocate
 * @return Pointer to allocated memory
 * @throws std::bad_alloc if allocation fails
 */
void* operator new[](size_t size) {
    void* ptr = std::malloc(size);
    if (!ptr)
        throw std::bad_alloc();
    ATOM_TRACK_ALLOC(ptr, size);
    return ptr;
}

/**
 * @brief Global operator delete[] overload for memory tracking
 * @param ptr Pointer to memory to deallocate
 */
void operator delete[](void* ptr) noexcept {
    ATOM_TRACK_FREE(ptr);
    std::free(ptr);
}

/**
 * @brief Global operator new (nothrow) overload for memory tracking
 * @param size Size of memory to allocate
 * @param nothrow_tag nothrow tag
 * @return Pointer to allocated memory, or nullptr if allocation fails
 */
void* operator new(size_t size, const std::nothrow_t&) noexcept {
    void* ptr = std::malloc(size);
    if (ptr) {
        ATOM_TRACK_ALLOC(ptr, size);
    }
    return ptr;
}

/**
 * @brief Global operator delete (nothrow) overload for memory tracking
 * @param ptr Pointer to memory to deallocate
 * @param nothrow_tag nothrow tag
 */
void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    ATOM_TRACK_FREE(ptr);
    std::free(ptr);
}

/**
 * @brief Global operator new[] (nothrow) overload for memory tracking
 * @param size Size of memory to allocate
 * @param nothrow_tag nothrow tag
 * @return Pointer to allocated memory, or nullptr if allocation fails
 */
void* operator new[](size_t size, const std::nothrow_t&) noexcept {
    void* ptr = std::malloc(size);
    if (ptr) {
        ATOM_TRACK_ALLOC(ptr, size);
    }
    return ptr;
}

/**
 * @brief Global operator delete[] (nothrow) overload for memory tracking
 * @param ptr Pointer to memory to deallocate
 * @param nothrow_tag nothrow tag
 */
void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    ATOM_TRACK_FREE(ptr);
    std::free(ptr);
}

#endif  // ATOM_MEMORY_TRACKING_ENABLED

