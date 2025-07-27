#pragma once

#include "common.hpp"
#include "inject.hpp"
#include "resolver.hpp"
#include "binding.hpp"
#include "container.hpp"

/**
 * @file all.hpp
 * @brief Comprehensive dependency injection framework with cutting-edge C++ concurrency primitives
 *
 * This header provides access to all components of the enhanced injection framework:
 *
 * Core Components:
 * - Traditional dependency injection container with binding and resolution
 * - Symbol-based type system for compile-time safety
 * - Lifecycle management (singleton, transient, request scopes)
 *
 * Advanced Concurrency Features:
 * - Lock-free data structures (queue, stack, ring buffer, hash map)
 * - High-performance synchronization primitives (adaptive spinlocks, reader-writer locks)
 * - Hazard pointers for safe memory reclamation
 * - Thread-safe dependency injection with lock-free resolution paths
 * - Thread-local caching with automatic invalidation
 * - Epoch-based memory management for cross-thread deallocation
 * - Performance monitoring with lock-free logging
 *
 * All implementations use C++23 features and are optimized for multicore architectures
 * with minimal contention and seamless scalability.
 */
