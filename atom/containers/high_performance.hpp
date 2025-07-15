/*!
 * \file high_performance.hpp
 * \brief High-performance container unified interface
 * \author Max Qian <lightapt.com>
 * \date 2024-4-11
 * \copyright Copyright (C) 2024 Max Qian <lightapt.com>
 */

#pragma once

#include <array>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(ATOM_HAS_BOOST_CONTAINER)
#include "boost_containers.hpp"
#endif

#if defined(ATOM_HAS_BOOST_GRAPH)
#include "graph.hpp"
#endif

#if defined(ATOM_HAS_BOOST_LOCKFREE)
#include "lockfree.hpp"
#endif

#if defined(ATOM_HAS_BOOST_INTRUSIVE)
#include "intrusive.hpp"
#endif

namespace atom::containers::hp {

#if defined(ATOM_HAS_BOOST_CONTAINER)

// Use Boost containers when available
using namespace atom::containers;

#else

// Fallback to standard library containers
template <typename K, typename V, typename Compare = std::less<K> >
using flat_map = std::map<K, V, Compare>;

template <typename K, typename Compare = std::less<K> >
using flat_set = std::set<K, Compare>;

template <typename T, std::size_t N>
using small_vector = std::vector<T>;

template <typename T, std::size_t N>
using static_vector = std::array<T, N>;

template <typename T>
using stable_vector = std::deque<T>;

using bstring = std::string;

template <typename K, typename V, typename Hash = std::hash<K>,
          typename Pred = std::equal_to<K> >
using fast_unordered_map = std::unordered_map<K, V, Hash, Pred>;

template <typename K, typename Hash = std::hash<K>,
          typename Pred = std::equal_to<K> >
using fast_unordered_set = std::unordered_set<K, Hash, Pred>;

#if __cplusplus >= 202002L
namespace pmr {
template <typename T>
using vector = std::pmr::vector<T>;

template <typename K, typename V, typename Compare = std::less<K> >
using map = std::pmr::map<K, V, Compare>;

template <typename K, typename V, typename Hash = std::hash<K>,
          typename Pred = std::equal_to<K> >
using unordered_map = std::pmr::unordered_map<K, V, Hash, Pred>;
}  // namespace pmr
#endif

#endif  // ATOM_HAS_BOOST_CONTAINER

}  // namespace atom::containers::hp

namespace atom::containers {

#if defined(ATOM_OPTIMIZE_FOR_SPEED)

// Use high-performance containers when optimization is enabled
template <typename K, typename V>
using HashMap = hp::fast_unordered_map<K, V>;

template <typename T>
using HashSet = hp::fast_unordered_set<T>;

template <typename T>
using Vector = hp::stable_vector<T>;

template <typename K, typename V>
using Map = hp::flat_map<K, V>;

template <typename T, size_t N = 16>
using SmallVector = hp::small_vector<T, N>;

using String = hp::bstring;

#else

// Use standard containers when not optimizing for speed
template <typename K, typename V>
using HashMap = std::unordered_map<K, V>;

template <typename T>
using HashSet = std::unordered_set<T>;

template <typename T>
using Vector = std::vector<T>;

template <typename K, typename V>
using Map = std::map<K, V>;

template <typename T, size_t N = 16>
using SmallVector = std::vector<T>;

using String = std::string;

#endif  // ATOM_OPTIMIZE_FOR_SPEED

}  // namespace atom::containers
