/*
 * cache.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file cache.hpp
 * @brief Main header for the atom::search::cache module.
 * @details This header includes all cache module components for convenience.
 *          For more granular includes, use the individual component headers:
 *          - types.hpp: Basic types, threading abstractions, concepts
 *          - exceptions.hpp: Exception classes
 *          - resource_cache.hpp: ResourceCache class
 *          - lru_cache.hpp: ThreadSafeLRUCache class
 *          - ttl_cache.hpp: TTLCache class
 */

#ifndef ATOM_SEARCH_CACHE_HPP
#define ATOM_SEARCH_CACHE_HPP

#include "exceptions.hpp"
#include "lru_cache.hpp"
#include "resource_cache.hpp"
#include "ttl_cache.hpp"
#include "types.hpp"

namespace atom::search {

// Re-export types from cache namespace for backward compatibility
using cache::Atomic;
using cache::Cacheable;
using cache::CacheConfig;
using cache::CacheStatistics;
using cache::Clock;
using cache::Duration;
using cache::Future;
using cache::HashMap;
using cache::JThread;
using cache::LockFreeQueue;
using cache::LockGuard;
using cache::PairStringHash;
using cache::Promise;
using cache::SharedLock;
using cache::SharedMutex;
using cache::String;
using cache::Thread;
using cache::TimePoint;
using cache::UniqueLock;
using cache::Vector;

// Re-export cache classes
using cache::ResourceCache;
using cache::ThreadSafeLRUCache;
using cache::TTLCache;
using cache::TTLCacheConfig;

}  // namespace atom::search

#endif  // ATOM_SEARCH_CACHE_HPP
