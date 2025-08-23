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

namespace atom::containers::hp {

/*!
 * \brief Flat map implementation using Boost containers
 * \tparam Key Key type
 * \tparam T Value type
 * \tparam Compare Comparison function
 */
template <typename Key, typename T, typename Compare = std::less<Key>>
using flat_map = boost::container::flat_map<Key, T, Compare>;

/*!
 * \brief Flat set implementation using Boost containers
 * \tparam Key Key type
 * \tparam Compare Comparison function
 */
template <typename Key, typename Compare = std::less<Key>>
using flat_set = boost::container::flat_set<Key, Compare>;

/*!
 * \brief Small vector with stack allocation for small sizes
 * \tparam T Element type
 * \tparam N Small buffer size
 */
template <typename T, std::size_t N>
using small_vector = boost::container::small_vector<T, N>;

/*!
 * \brief Static vector with fixed capacity
 * \tparam T Element type
 * \tparam N Maximum capacity
 */
template <typename T, std::size_t N>
using static_vector = boost::container::static_vector<T, N>;

/*!
 * \brief Stable vector with iterator stability
 * \tparam T Element type
 */
template <typename T>
using stable_vector = boost::container::stable_vector<T>;

/*!
 * \brief Boost string implementation
 */
using bstring = boost::container::string;

/*!
 * \brief Fast unordered map using Boost implementation
 * \tparam Key Key type
 * \tparam T Value type
 * \tparam Hash Hash function
 * \tparam Pred Equality predicate
 */
template <typename Key, typename T, typename Hash = boost::hash<Key>,
          typename Pred = std::equal_to<Key>>
using fast_unordered_map = boost::unordered_map<Key, T, Hash, Pred>;

/*!
 * \brief Fast unordered set using Boost implementation
 * \tparam Key Key type
 * \tparam Hash Hash function
 * \tparam Pred Equality predicate
 */
template <typename Key, typename Hash = boost::hash<Key>,
          typename Pred = std::equal_to<Key>>
using fast_unordered_set = boost::unordered_set<Key, Hash, Pred>;

namespace pmr = boost::container::pmr;

#ifdef ATOM_HAS_BOOST_GRAPH
/*!
 * \namespace graph
 * \brief Graph algorithms and data structures
 */
namespace graph {

/*!
 * \enum GraphOptions
 * \brief Graph type options
 */
enum class GraphOptions {
    Directed,     /*!< Directed graph */
    Undirected,   /*!< Undirected graph */
    Bidirectional /*!< Bidirectional graph */
};

/*!
 * \brief Create a graph with specified options
 * \tparam VertexProperty Vertex property type
 * \tparam EdgeProperty Edge property type
 * \param options Graph configuration options
 * \return Configured graph instance
 */
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

/*!
 * \brief Find shortest path between two vertices
 * \tparam Graph Graph type
 * \param g The graph
 * \param start Starting vertex
 * \param end Ending vertex
 * \return Vector of vertices representing the shortest path
 */
template <typename Graph>
std::vector<typename boost::graph_traits<Graph>::vertex_descriptor>
shortest_path(const Graph& g,
              typename boost::graph_traits<Graph>::vertex_descriptor start,
              typename boost::graph_traits<Graph>::vertex_descriptor end) {
    using vertex_t = typename boost::graph_traits<Graph>::vertex_descriptor;

    std::vector<vertex_t> predecessors(boost::num_vertices(g));
    std::vector<int> distances(boost::num_vertices(g));

    boost::dijkstra_shortest_paths(
        g, start,
        boost::predecessor_map(
            boost::make_iterator_property_map(
                predecessors.begin(), boost::get(boost::vertex_index, g)))
            .distance_map(boost::make_iterator_property_map(
                distances.begin(), boost::get(boost::vertex_index, g))));

    std::vector<vertex_t> path;
    vertex_t current = end;
    while (current != start) {
        path.push_back(current);
        current = predecessors[current];

        if (current == vertex_t())
            return {};
    }

    path.push_back(start);
    std::reverse(path.begin(), path.end());
    return path;
}

}  // namespace graph
#endif  // ATOM_HAS_BOOST_GRAPH

#ifdef ATOM_HAS_BOOST_LOCKFREE
/*!
 * \namespace lockfree
 * \brief Lock-free data structures
 */
namespace lockfree {

/*!
 * \brief Lock-free queue with fixed capacity
 * \tparam T Element type
 * \tparam Capacity Maximum queue capacity
 */
template <typename T, size_t Capacity = 1024>
using queue = boost::lockfree::queue<T, boost::lockfree::capacity<Capacity>>;

/*!
 * \brief Lock-free stack with fixed capacity
 * \tparam T Element type
 * \tparam Capacity Maximum stack capacity
 */
template <typename T, size_t Capacity = 1024>
using stack = boost::lockfree::stack<T, boost::lockfree::capacity<Capacity>>;

/*!
 * \brief Single-producer single-consumer queue
 * \tparam T Element type
 * \tparam Capacity Maximum queue capacity
 */
template <typename T, size_t Capacity = 1024>
using spsc_queue =
    boost::lockfree::spsc_queue<T, boost::lockfree::capacity<Capacity>>;

}  // namespace lockfree
#endif  // ATOM_HAS_BOOST_LOCKFREE

#ifdef ATOM_HAS_BOOST_INTRUSIVE
/*!
 * \namespace intrusive
 * \brief Intrusive containers
 */
namespace intrusive {

/*!
 * \brief Base hook for intrusive lists
 */
using list_base_hook = boost::intrusive::list_base_hook<>;

/*!
 * \brief Base hook for intrusive sets
 */
using set_base_hook = boost::intrusive::set_base_hook<>;

/*!
 * \brief Intrusive list
 * \tparam T Element type
 */
template <typename T>
using list = boost::intrusive::list<T>;

/*!
 * \brief Intrusive set
 * \tparam T Element type
 * \tparam Compare Comparison function
 */
template <typename T, typename Compare = std::less<T>>
using set = boost::intrusive::set<T, boost::intrusive::compare<Compare>>;

/*!
 * \brief Intrusive AVL tree
 * \tparam T Element type
 * \tparam Compare Comparison function
 */
template <typename T, typename Compare = std::less<T>>
using avl_set =
    boost::intrusive::avl_set<T, boost::intrusive::compare<Compare>>;

/*!
 * \brief Intrusive hash set
 * \tparam T Element type
 * \tparam Hash Hash function
 */
template <typename T, typename Hash = boost::hash<T>>
using unordered_set =
    boost::intrusive::unordered_set<T, boost::intrusive::hash<Hash>>;

}  // namespace intrusive
#endif  // ATOM_HAS_BOOST_INTRUSIVE

}  // namespace atom::containers::hp

#else  // Fallback to standard library containers

namespace atom::containers::hp {

template <typename K, typename V, typename Compare = std::less<K>>
using flat_map = std::map<K, V, Compare>;

template <typename K, typename Compare = std::less<K>>
using flat_set = std::set<K, Compare>;

template <typename T, std::size_t N>
using small_vector = std::vector<T>;

template <typename T, std::size_t N>
using static_vector = std::array<T, N>;

template <typename T>
using stable_vector = std::deque<T>;

using bstring = std::string;

template <typename K, typename V, typename Hash = std::hash<K>,
          typename Pred = std::equal_to<K>>
using fast_unordered_map = std::unordered_map<K, V, Hash, Pred>;

template <typename K, typename Hash = std::hash<K>,
          typename Pred = std::equal_to<K>>
using fast_unordered_set = std::unordered_set<K, Hash, Pred>;

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
enum class GraphOptions { Directed, Undirected, Bidirectional };

/*!
 * \brief Simple adjacency list graph implementation
 * \tparam VertexProperty Vertex property type
 * \tparam EdgeProperty Edge property type
 */
template <typename VertexProperty = void, typename EdgeProperty = void>
class simple_graph {
public:
    using vertex_id = std::size_t;
    using edge = std::pair<vertex_id, EdgeProperty>;

    struct vertex {
        VertexProperty property;
        std::vector<edge> edges;
    };

    /*!
     * \brief Add a vertex to the graph
     * \param prop Vertex property
     * \return Vertex ID
     */
    vertex_id add_vertex(const VertexProperty& prop = {}) {
        vertices_.emplace_back(vertex{prop, {}});
        return vertices_.size() - 1;
    }

    /*!
     * \brief Add an edge to the graph
     * \param src Source vertex
     * \param dst Destination vertex
     * \param prop Edge property
     */
    void add_edge(vertex_id src, vertex_id dst, const EdgeProperty& prop = {}) {
        if (src < vertices_.size() && dst < vertices_.size()) {
            vertices_[src].edges.emplace_back(dst, prop);
            if (bidirectional_) {
                vertices_[dst].edges.emplace_back(src, prop);
            }
        }
    }

    /*!
     * \brief Constructor with graph options
     * \param options Graph configuration options
     */
    explicit simple_graph(std::initializer_list<GraphOptions> options)
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
    return {};
}
}  // namespace graph
#endif  // ATOM_HAS_BOOST_GRAPH

#ifdef ATOM_HAS_BOOST_LOCKFREE
namespace lockfree {
/*!
 * \brief Thread-safe queue fallback implementation
 * \tparam T Element type
 * \tparam Capacity Maximum capacity
 */
template <typename T, size_t Capacity = 1024>
class queue {
public:
    /*!
     * \brief Push an element to the queue
     * \param value Element to push
     * \return true if successful, false if queue is full
     */
    bool push(const T& value) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (q_.size() >= Capacity)
            return false;
        q_.push(value);
        return true;
    }

    /*!
     * \brief Pop an element from the queue
     * \param value Reference to store the popped element
     * \return true if successful, false if queue is empty
     */
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
using stack = std::stack<T>;

template <typename T, size_t Capacity = 1024>
using spsc_queue = queue<T, Capacity>;
}  // namespace lockfree
#endif  // ATOM_HAS_BOOST_LOCKFREE

}  // namespace atom::containers::hp

#endif  // defined(ATOM_HAS_BOOST_CONTAINER)

namespace atom::containers {

#if defined(ATOM_OPTIMIZE_FOR_SPEED)

/*!
 * \brief Optimized hash map type alias
 * \tparam K Key type
 * \tparam V Value type
 */
template <typename K, typename V>
using HashMap = hp::fast_unordered_map<K, V>;

/*!
 * \brief Optimized hash set type alias
 * \tparam T Element type
 */
template <typename T>
using HashSet = hp::fast_unordered_set<T>;

/*!
 * \brief Optimized vector type alias
 * \tparam T Element type
 */
template <typename T>
using Vector = hp::stable_vector<T>;

/*!
 * \brief Optimized map type alias
 * \tparam K Key type
 * \tparam V Value type
 */
template <typename K, typename V>
using Map = hp::flat_map<K, V>;

/*!
 * \brief Small vector optimized for small sizes
 * \tparam T Element type
 * \tparam N Small buffer size
 */
template <typename T, size_t N = 16>
using SmallVector = hp::small_vector<T, N>;

/*!
 * \brief Optimized string type alias
 */
using String = hp::bstring;

#else  // Use standard containers

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
