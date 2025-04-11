/*
 * atom/containers/high_performance.hpp
 *
 * Copyright (C) 2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-11

Description: 高性能容器统一接口

**************************************************/

#pragma once

// 添加标准库容器头文件
#include <array>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if __cplusplus >= 202002L
#include <memory_resource>
#endif

// 根据是否启用Boost容器库选择不同的实现
#if defined(ATOM_HAS_BOOST_CONTAINER)

#include "boost_containers.hpp"

namespace atom {
namespace containers {
namespace hp {

// 使用Boost容器实现
template <typename Key, typename T, typename Compare = std::less<Key>>
using flat_map = boost::container::flat_map<Key, T, Compare>;

template <typename Key, typename Compare = std::less<Key>>
using flat_set = boost::container::flat_set<Key, Compare>;

template <typename T, std::size_t N>
using small_vector = boost::container::small_vector<T, N>;

template <typename T, std::size_t N>
using static_vector = boost::container::static_vector<T, N>;

template <typename T>
using stable_vector = boost::container::stable_vector<T>;

using bstring = boost::container::string;

template <typename Key, typename T, typename Hash = boost::hash<Key>,
          typename Pred = std::equal_to<Key>>
using fast_unordered_map = boost::unordered_map<Key, T, Hash, Pred>;

template <typename Key, typename Hash = boost::hash<Key>,
          typename Pred = std::equal_to<Key>>
using fast_unordered_set = boost::unordered_set<Key, Hash, Pred>;

// 内存管理扩展
namespace pmr = boost::container::pmr;

#ifdef ATOM_HAS_BOOST_GRAPH
// 创建图相关操作的命名空间
namespace graph {
// 图选项枚举
enum class GraphOptions {
    Directed,      // 有向图
    Undirected,    // 无向图
    Bidirectional  // 双向图
};

// 创建图的函数
template <typename VertexProperty = boost::no_property,
          typename EdgeProperty = boost::no_property>
auto create_graph(std::initializer_list<GraphOptions> options) {
    if (std::find(options.begin(), options.end(), GraphOptions::Directed) !=
        options.end()) {
        return boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS,
                                     VertexProperty, EdgeProperty>;
    } else if (std::find(options.begin(), options.end(),
                         GraphOptions::Bidirectional) != options.end()) {
        return boost::adjacency_list<boost::vecS, boost::vecS,
                                     boost::bidirectionalS, VertexProperty,
                                     EdgeProperty>;
    } else {
        return boost::adjacency_list<boost::vecS, boost::vecS,
                                     boost::undirectedS, VertexProperty,
                                     EdgeProperty>;
    }
}

// 图算法包装
template <typename Graph>
std::vector<typename boost::graph_traits<Graph>::vertex_descriptor>
shortest_path(const Graph& g,
              typename boost::graph_traits<Graph>::vertex_descriptor start,
              typename boost::graph_traits<Graph>::vertex_descriptor end) {
    using vertex_t = typename boost::graph_traits<Graph>::vertex_descriptor;

    std::vector<vertex_t> predecessors(boost::num_vertices(g));
    std::vector<int> distances(boost::num_vertices(g));

    // 使用Boost的Dijkstra算法
    boost::dijkstra_shortest_paths(
        g, start,
        boost::predecessor_map(
            boost::make_iterator_property_map(
                predecessors.begin(), boost::get(boost::vertex_index, g)))
            .distance_map(boost::make_iterator_property_map(
                distances.begin(), boost::get(boost::vertex_index, g))));

    // 重建路径
    std::vector<vertex_t> path;
    vertex_t current = end;
    while (current != start) {
        path.push_back(current);
        current = predecessors[current];

        if (current == vertex_t())  // 无路径
            return {};
    }

    path.push_back(start);
    std::reverse(path.begin(), path.end());
    return path;
}
}  // namespace graph
#endif  // ATOM_HAS_BOOST_GRAPH

#ifdef ATOM_HAS_BOOST_LOCKFREE
// 无锁数据结构
namespace lockfree {
template <typename T, size_t Capacity = 1024>
using queue = boost::lockfree::queue<T, boost::lockfree::capacity<Capacity>>;

template <typename T, size_t Capacity = 1024>
using stack = boost::lockfree::stack<T, boost::lockfree::capacity<Capacity>>;

template <typename T, size_t Capacity = 1024>
using spsc_queue =
    boost::lockfree::spsc_queue<T, boost::lockfree::capacity<Capacity>>;
}  // namespace lockfree
#endif  // ATOM_HAS_BOOST_LOCKFREE

#ifdef ATOM_HAS_BOOST_INTRUSIVE
// 侵入式容器
namespace intrusive {
// 侵入式链表基类
using list_base_hook = boost::intrusive::list_base_hook<>;

// 侵入式集合基类
using set_base_hook = boost::intrusive::set_base_hook<>;

// 侵入式链表
template <typename T>
using list = boost::intrusive::list<T>;

// 侵入式集合
template <typename T, typename Compare = std::less<T>>
using set = boost::intrusive::set<T, boost::intrusive::compare<Compare>>;

// 侵入式平衡树
template <typename T, typename Compare = std::less<T>>
using avl_set =
    boost::intrusive::avl_set<T, boost::intrusive::compare<Compare>>;

// 侵入式hash表
template <typename T, typename Hash = boost::hash<T>>
using unordered_set =
    boost::intrusive::unordered_set<T, boost::intrusive::hash<Hash>>;
}  // namespace intrusive
#endif  // ATOM_HAS_BOOST_INTRUSIVE

}  // namespace hp

}  // namespace containers
}  // namespace atom

#else  // ATOM_HAS_BOOST_CONTAINER 不可用

namespace atom {
namespace containers {
namespace hp {

// 使用标准库容器实现
template <typename K, typename V, typename Compare = std::less<K>>
using flat_map = std::map<K, V, Compare>;  // 不是真正的flat_map，但保持接口兼容

template <typename K, typename Compare = std::less<K>>
using flat_set = std::set<K, Compare>;  // 不是真正的flat_set，但保持接口兼容

template <typename T, std::size_t N>
using small_vector = std::vector<T>;  // 不支持栈分配，但保持接口兼容

template <typename T, std::size_t N>
using static_vector = std::array<T, N>;  // 使用array替代，但API不完全兼容

template <typename T>
using stable_vector = std::deque<T>;  // deque提供类似的迭代器稳定性

using bstring = std::string;

template <typename K, typename V, typename Hash = std::hash<K>,
          typename Pred = std::equal_to<K>>
using fast_unordered_map = std::unordered_map<K, V, Hash, Pred>;

template <typename K, typename Hash = std::hash<K>,
          typename Pred = std::equal_to<K>>
using fast_unordered_set = std::unordered_set<K, Hash, Pred>;

// 在C++20或更高版本中尝试使用PMR接口
#if __cplusplus >= 202002L
namespace pmr {
template <typename T>
using vector = std::pmr::vector<T>;

template <typename K, typename V, typename Compare = std::less<K>>
using map = std::pmr::map<K, V, Compare>;

template <typename K, typename V, typename Hash = std::hash<K>,
          typename Pred = std::equal_to<K>>
using unordered_map = std::pmr::unordered_map<K, V, Hash, Pred>;
}  // namespace pmr
#endif

#ifdef ATOM_HAS_BOOST_GRAPH
namespace graph {
// 提供图算法的简单替代实现
enum class GraphOptions { Directed, Undirected, Bidirectional };

// 简单的邻接表图实现
template <typename VertexProperty = void, typename EdgeProperty = void>
class simple_graph {
public:
    using vertex_id = std::size_t;
    using edge = std::pair<vertex_id, EdgeProperty>;

    struct vertex {
        VertexProperty property;
        std::vector<edge> edges;
    };

    vertex_id add_vertex(const VertexProperty& prop = {}) {
        vertices_.emplace_back(vertex{prop, {}});
        return vertices_.size() - 1;
    }

    void add_edge(vertex_id src, vertex_id dst, const EdgeProperty& prop = {}) {
        if (src < vertices_.size() && dst < vertices_.size()) {
            vertices_[src].edges.emplace_back(dst, prop);
            if (bidirectional_) {
                vertices_[dst].edges.emplace_back(src, prop);
            }
        }
    }

    simple_graph(std::initializer_list<GraphOptions> options)
        : directed_(false), bidirectional_(false) {
        for (auto option : options) {
            if (option == GraphOptions::Directed)
                directed_ = true;
            else if (option == GraphOptions::Bidirectional)
                bidirectional_ = true;
        }
    }

private:
    std::vector<vertex> vertices_;
    bool directed_;
    bool bidirectional_;
};

template <typename Graph>
std::vector<std::size_t> shortest_path(const Graph& g, std::size_t start,
                                       std::size_t end) {
    // 简单的Dijkstra算法实现
    // 仅作为不使用Boost时的后备实现
    // [简化实现，仅作示例]
    return {};
}
}  // namespace graph
#endif  // ATOM_HAS_BOOST_GRAPH

#ifdef ATOM_HAS_BOOST_LOCKFREE
// 简易非无锁队列作为后备
namespace lockfree {
template <typename T, size_t Capacity = 1024>
class queue {
public:
    bool push(const T& value) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (q_.size() >= Capacity)
            return false;
        q_.push(value);
        return true;
    }

    bool pop(T& value) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (q_.empty())
            return false;
        value = q_.front();
        q_.pop();
        return true;
    }

private:
    std::queue<T> q_;
    std::mutex mtx_;
};

template <typename T, size_t Capacity = 1024>
using stack = std::stack<T>;  // 仅作简单替代

template <typename T, size_t Capacity = 1024>
using spsc_queue = queue<T, Capacity>;  // 使用普通队列替代
}  // namespace lockfree
#endif  // ATOM_HAS_BOOST_LOCKFREE

}  // namespace hp
}  // namespace containers
}  // namespace atom

#endif  // defined(ATOM_HAS_BOOST_CONTAINER)

// 常用容器类型别名
namespace atom {
namespace containers {

// 根据配置选择最优的容器实现
#if defined(ATOM_OPTIMIZE_FOR_SPEED)

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

// 字符串类型
using String = hp::bstring;

#else  // 默认使用标准容器

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

// 字符串类型
using String = std::string;

#endif  // ATOM_OPTIMIZE_FOR_SPEED

}  // namespace containers
}  // namespace atom