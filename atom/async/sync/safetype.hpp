#ifndef ATOM_ASYNC_SYNC_SAFETYPE_HPP
#define ATOM_ASYNC_SYNC_SAFETYPE_HPP

/**
 * @file safetype.hpp
 * @brief Aggregate header for all thread-safe and lock-free data structures.
 *
 * This header includes all the individual components:
 * - LockFreeStack: Lock-free stack implementation
 * - LockFreeList: Lock-free singly-linked list
 * - LockFreeHashTable: Lock-free hash table with separate chaining
 * - ThreadSafeVector: Thread-safe dynamic array
 * - SafeType: Thread-safe wrapper for any type
 *
 * For finer-grained includes, use the individual headers directly:
 * - lockfree_stack.hpp
 * - lockfree_list.hpp
 * - lockfree_hashtable.hpp
 * - thread_safe_vector.hpp
 * - safe_type.hpp
 */

#include "lockfree_hashtable.hpp"
#include "lockfree_list.hpp"
#include "lockfree_stack.hpp"
#include "safe_type.hpp"
#include "thread_safe_vector.hpp"

#endif  // ATOM_ASYNC_SYNC_SAFETYPE_HPP
