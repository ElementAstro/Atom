/*
 * atom/containers/boost_containers.hpp
 *
 * Copyright (C) 2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-11

Description: Boost High-Performance Containers

**************************************************/

#pragma once

#include "../macro.hpp"

// Enable only if ATOM_USE_BOOST_CONTAINER macro is defined and Boost container
// library is available
#if defined(ATOM_HAS_BOOST_CONTAINER)

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/stable_vector.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/container/string.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

namespace atom {
namespace containers {

/**
 * @brief High-performance flat_map implementation
 *
 * boost::container::flat_map is an associative container based on a sorted
 * vector, offering better cache locality and memory efficiency than std::map.
 * Suitable for scenarios with frequent queries but infrequent modifications.
 */
template <typename Key, typename T, typename Compare = std::less<Key>>
using flat_map = boost::container::flat_map<Key, T, Compare>;

/**
 * @brief High-performance flat_set implementation
 *
 * boost::container::flat_set is an associative container based on a sorted
 * vector, offering better cache locality and memory efficiency than std::set.
 * Suitable for scenarios with frequent queries but infrequent modifications.
 */
template <typename Key, typename Compare = std::less<Key>>
using flat_set = boost::container::flat_set<Key, Compare>;

/**
 * @brief Small vector implementation
 *
 * Suitable for vectors that are usually small, avoiding heap allocation for
 * small data. Internally has a fixed-size buffer, only using heap allocation
 * when the number of elements exceeds this buffer.
 *
 * @tparam T Element type
 * @tparam N Internal buffer size (number of elements)
 */
template <typename T, std::size_t N>
using small_vector = boost::container::small_vector<T, N>;

/**
 * @brief Static vector implementation
 *
 * Vector with a fixed maximum size, all memory allocated on the stack.
 * Never uses heap memory, ideal for real-time systems or performance-critical
 * code.
 *
 * @tparam T Element type
 * @tparam N Maximum number of elements
 */
template <typename T, std::size_t N>
using static_vector = boost::container::static_vector<T, N>;

/**
 * @brief Stable vector implementation
 *
 * Provides stable iterators and references, which remain valid even after
 * insertions and deletions. Suitable for scenarios where iterator validity must
 * be preserved.
 */
template <typename T>
using stable_vector = boost::container::stable_vector<T>;

/**
 * @brief High-performance string implementation
 *
 * Uses small string optimization (SSO) and custom memory management.
 */
using bstring = boost::container::string;

/**
 * @brief High-performance unordered map implementation
 *
 * Offers better performance characteristics than std::unordered_map, especially
 * in highly concurrent environments.
 */
template <typename Key, typename T, typename Hash = boost::hash<Key>,
          typename Pred = std::equal_to<Key>>
using fast_unordered_map = boost::unordered_map<Key, T, Hash, Pred>;

/**
 * @brief High-performance unordered set implementation
 */
template <typename Key, typename Hash = boost::hash<Key>,
          typename Pred = std::equal_to<Key>>
using fast_unordered_set = boost::unordered_set<Key, Hash, Pred>;

// Example usage of PMR (Polymorphic Memory Resource)
namespace pmr {
template <typename T>
using polymorphic_allocator = boost::container::pmr::polymorphic_allocator<T>;

template <typename T>
using vector = boost::container::vector<T, polymorphic_allocator<T>>;

template <typename Key, typename T, typename Compare = std::less<Key>>
using flat_map = boost::container::flat_map<
    Key, T, Compare,
    boost::container::pmr::polymorphic_allocator<std::pair<Key, T>>>;
}  // namespace pmr

}  // namespace containers
}  // namespace atom

#endif  // defined(ATOM_HAS_BOOST_CONTAINER)