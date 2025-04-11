/*
 * config.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-10

Description: Component System Configuration

**************************************************/

#ifndef ATOM_COMPONENT_CONFIG_HPP
#define ATOM_COMPONENT_CONFIG_HPP

// 默认使用标准库容器
#define USE_STD_CONTAINERS 1

// 设置是否启用 Boost 高性能容器
#ifndef USE_BOOST_CONTAINERS
#define USE_BOOST_CONTAINERS 0
#endif

// 设置是否启用 EMHASH 高性能哈希表
#ifndef ENABLE_FASTHASH
#define ENABLE_FASTHASH 0
#endif

// 设置是否启用 Boost Flat 容器
#ifndef ENABLE_BOOST_FLAT
#define ENABLE_BOOST_FLAT 0
#endif

// 设置是否启用 Boost 无锁数据结构
#ifndef ENABLE_BOOST_LOCKFREE
#define ENABLE_BOOST_LOCKFREE 0
#endif

// 设置是否启用 Boost 对象池
#ifndef ENABLE_BOOST_POOL
#define ENABLE_BOOST_POOL 0
#endif

// 设置是否启用懒加载组件
#ifndef ENABLE_LAZY_LOADING
#define ENABLE_LAZY_LOADING 1
#endif

// 设置是否启用热重载组件
#ifndef ENABLE_HOT_RELOAD
#define ENABLE_HOT_RELOAD 1
#endif

// 设置是否启用事件系统
#ifndef ENABLE_EVENT_SYSTEM
#define ENABLE_EVENT_SYSTEM 1
#endif

// 设置是否启用组件生命周期钩子
#ifndef ENABLE_LIFECYCLE_HOOKS
#define ENABLE_LIFECYCLE_HOOKS 1
#endif

// 组件容器类型选择
#if USE_BOOST_CONTAINERS

// 包含必要的 Boost 头文件
#if ENABLE_BOOST_FLAT
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#endif

#if ENABLE_BOOST_LOCKFREE
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/stack.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#endif

#if ENABLE_BOOST_POOL
#include <boost/pool/object_pool.hpp>
#include <boost/pool/singleton_pool.hpp>
#endif

// 定义命名空间别名和类型别名
namespace atom {
namespace components {
namespace containers {

#if ENABLE_BOOST_FLAT
template <typename Key, typename Value>
using flat_map = boost::container::flat_map<Key, Value>;

template <typename Key>
using flat_set = boost::container::flat_set<Key>;
#else
template <typename Key, typename Value>
using flat_map = std::unordered_map<Key, Value>;

template <typename Key>
using flat_set = std::unordered_set<Key>;
#endif

#if ENABLE_BOOST_LOCKFREE
template <typename T>
using lockfree_queue = boost::lockfree::queue<T>;

template <typename T>
using lockfree_stack = boost::lockfree::stack<T>;

template <typename T>
using lockfree_spsc_queue = boost::lockfree::spsc_queue<T>;
#endif

#if ENABLE_BOOST_POOL
template <typename T>
using object_pool = boost::pool::object_pool<T>;

template <typename Tag, typename T>
using singleton_pool = boost::pool::singleton_pool<Tag, sizeof(T)>;
#endif

// 高性能字符串类型
#include <boost/container/string.hpp>
using fast_string = boost::container::string;

} // namespace containers
} // namespace components
} // namespace atom

#else // 使用标准库容器

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <queue>
#include <stack>
#include <string>

namespace atom {
namespace components {
namespace containers {

template <typename Key, typename Value>
using flat_map = std::unordered_map<Key, Value>;

template <typename Key>
using flat_set = std::unordered_set<Key>;

template <typename T>
using object_pool = std::shared_ptr<T>;

// 标准库容器的别名
template <typename T>
using queue = std::queue<T>;

template <typename T>
using stack = std::stack<T>;

// 标准字符串别名
using fast_string = std::string;

} // namespace containers
} // namespace components
} // namespace atom

#endif // USE_BOOST_CONTAINERS

#include <any>
#include <functional>
#include <chrono>

namespace atom {
namespace components {

#if ENABLE_FASTHASH
#include "emhash/hash_set8.hpp"
#include "emhash/hash_table8.hpp"
using StringSet = emhash::HashSet<std::string>;
using StringMap = emhash::HashMap<std::string, std::string>;
#else
using StringSet = std::unordered_set<std::string>;
using StringMap = std::unordered_map<std::string, std::string>;
#endif

// 事件系统相关数据类型
#if ENABLE_EVENT_SYSTEM


struct Event {
    std::string name;
    std::any data;
    std::string source;
    std::chrono::steady_clock::time_point timestamp;
};

using EventCallback = std::function<void(const Event&)>;
using EventCallbackId = uint64_t;
#endif

} // namespace components
} // namespace atom

#endif // ATOM_COMPONENT_CONFIG_HPP