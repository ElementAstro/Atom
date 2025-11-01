/*
 * atom/memory/instantiations.cpp
 *
 * Copyright (C) 2024 Max Qian <lightapt.com>
 */

/**
 * @file instantiations.cpp
 * @brief Explicit template instantiations for commonly used memory types
 *
 * This file provides explicit template instantiations for frequently used
 * memory pool and object pool types to improve compilation times and
 * reduce code bloat in client applications.
 */

// Note: Due to naming conflicts between different MemoryPool classes
// and complex template dependencies, we'll keep this file minimal
// and focus on the most commonly used instantiations that are safe.

#include "short_alloc.hpp"

namespace atom::memory {

// Arena allocators for common sizes
// template<size_t N, size_t alignment, bool ThreadSafe, AllocationStrategy
// Strategy>
template class Arena<1024>;
template class Arena<4096>;
template class Arena<8192>;
template class Arena<16384>;

// Short allocators for common types and sizes
// template<class T, size_t N, size_t Align, bool ThreadSafe, AllocationStrategy
// Strategy>
template class ShortAlloc<char, 1024>;
template class ShortAlloc<int, 1024>;
template class ShortAlloc<double, 1024>;

template class ShortAlloc<char, 4096>;
template class ShortAlloc<int, 4096>;
template class ShortAlloc<double, 4096>;

}  // namespace atom::memory
