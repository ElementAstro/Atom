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

// 只有在定义了ATOM_USE_BOOST_CONTAINER宏且Boost容器库可用时才启用
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
 * @brief 高性能平面映射（flat_map）实现
 *
 * boost::container::flat_map是一个基于排序向量的关联容器，
 * 比标准map具有更好的缓存局部性和内存使用效率。
 * 适用于频繁查询但较少修改的场景。
 */
template <typename Key, typename T, typename Compare = std::less<Key>>
using flat_map = boost::container::flat_map<Key, T, Compare>;

/**
 * @brief 高性能平面集合（flat_set）实现
 *
 * boost::container::flat_set是一个基于排序向量的关联容器，
 * 比标准set具有更好的缓存局部性和内存使用效率。
 * 适用于频繁查询但较少修改的场景。
 */
template <typename Key, typename Compare = std::less<Key>>
using flat_set = boost::container::flat_set<Key, Compare>;

/**
 * @brief 小型向量（small_vector）实现
 *
 * 适用于大小通常较小的向量，避免小型数据的堆分配。
 * 内部有一个固定大小的缓冲区，只有当元素数量超过这个缓冲区时才会使用堆分配。
 *
 * @tparam T 元素类型
 * @tparam N 内部缓冲区大小（元素个数）
 */
template <typename T, std::size_t N>
using small_vector = boost::container::small_vector<T, N>;

/**
 * @brief 静态向量（static_vector）实现
 *
 * 固定最大大小的向量，所有内存在栈上分配。
 * 永远不会使用堆内存，非常适合实时系统或性能关键型代码。
 *
 * @tparam T 元素类型
 * @tparam N 最大元素个数
 */
template <typename T, std::size_t N>
using static_vector = boost::container::static_vector<T, N>;

/**
 * @brief 稳定向量（stable_vector）实现
 *
 * 提供稳定的迭代器和引用，即使在插入和删除操作后也不会失效。
 * 适用于需要保持迭代器有效性的场景。
 */
template <typename T>
using stable_vector = boost::container::stable_vector<T>;

/**
 * @brief 高性能字符串实现
 *
 * 使用小字符串优化(SSO)和自定义内存管理
 */
using bstring = boost::container::string;

/**
 * @brief 高性能无序映射实现
 *
 * 比std::unordered_map有更好的性能特性，特别是在高并发环境下。
 */
template <typename Key, typename T, typename Hash = boost::hash<Key>,
          typename Pred = std::equal_to<Key>>
using fast_unordered_map = boost::unordered_map<Key, T, Hash, Pred>;

/**
 * @brief 高性能无序集合实现
 */
template <typename Key, typename Hash = boost::hash<Key>,
          typename Pred = std::equal_to<Key>>
using fast_unordered_set = boost::unordered_set<Key, Hash, Pred>;

// PMR内存资源使用示例
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