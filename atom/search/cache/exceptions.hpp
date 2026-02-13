/*
 * exceptions.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file exceptions.hpp
 * @brief Exception classes for cache components.
 */

#ifndef ATOM_SEARCH_CACHE_EXCEPTIONS_HPP
#define ATOM_SEARCH_CACHE_EXCEPTIONS_HPP

#include <stdexcept>
#include <string>

namespace atom::search::cache {

/**
 * @brief Base exception class for cache errors.
 */
class CacheException : public std::runtime_error {
public:
    explicit CacheException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Exception thrown when cache lock operations fail.
 */
class CacheLockException : public CacheException {
public:
    explicit CacheLockException(const std::string& message)
        : CacheException("Lock error: " + message) {}
};

/**
 * @brief Exception thrown when cache I/O operations fail.
 */
class CacheIOException : public CacheException {
public:
    explicit CacheIOException(const std::string& message)
        : CacheException("I/O error: " + message) {}
};

/**
 * @brief Exception thrown when cache capacity is exceeded.
 */
class CacheCapacityException : public CacheException {
public:
    explicit CacheCapacityException(const std::string& message)
        : CacheException("Capacity error: " + message) {}
};

/**
 * @brief Exception thrown when cache configuration is invalid.
 */
class CacheConfigException : public CacheException {
public:
    explicit CacheConfigException(const std::string& message)
        : CacheException("Configuration error: " + message) {}
};

// Legacy aliases within cache namespace
using LRUCacheException = CacheException;
using LRUCacheLockException = CacheLockException;
using LRUCacheIOException = CacheIOException;
using TTLCacheException = CacheException;

}  // namespace atom::search::cache

// Expose in atom::search namespace for backward compatibility
namespace atom::search {
using cache::CacheCapacityException;
using cache::CacheConfigException;
using cache::CacheException;
using cache::CacheIOException;
using cache::CacheLockException;
using cache::LRUCacheException;
using cache::LRUCacheIOException;
using cache::LRUCacheLockException;
using cache::TTLCacheException;
}  // namespace atom::search

#endif  // ATOM_SEARCH_CACHE_EXCEPTIONS_HPP
