/*
 * atom/containers/graph.hpp
 *
 * Copyright (C) 2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-11

Description: High-performance graph data structure

**************************************************/

#pragma once

#if defined(ATOM_HAS_BOOST_GRAPH)

#include "../macro.hpp"  // IWYU pragma: keep

#include <algorithm>
#include <cstddef>
#include <limits>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/property_map/function_property_map.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/property_map/static_property_map.hpp>

namespace atom::containers::graph {

/**
 * @brief Base class for graph vertex properties.
 *
 * Can be inherited to add custom vertex properties.
 */
struct VertexProperties {
    std::string name{};  ///< Vertex name
    std::size_t id{};    ///< Vertex identifier
};

/**
 * @brief Base class for graph edge properties.
 *
 * Can be inherited to add custom edge properties.
 */
struct EdgeProperties {
    double weight{1.0};   ///< Edge weight for shortest-path algorithms
    std::string label{};  ///< Optional descriptive label
};

/**
 * @brief Runtime graph configuration options.
 */
enum class GraphOptions {
    Directed,              ///< Directed graph
    Undirected,            ///< Undirected graph
    Bidirectional,         ///< Bidirectional graph
    AllowParallelEdges,    ///< Allow multiple edges between two vertices
    DisallowParallelEdges  ///< Disallow multiple edges between two vertices
};

/**
 * @brief Graph implementation that leverages Boost.Graph while embracing
 * modern C++ idioms.
 */
template <typename VertexProperty = VertexProperties,
          typename EdgeProperty = EdgeProperties>
class Graph {
    static_assert(std::default_initializable<VertexProperty>,
                  "VertexProperty must be default constructible");
    static_assert(std::default_initializable<EdgeProperty>,
                  "EdgeProperty must be default constructible");

public:
    enum class GraphType { Directed, Undirected, Bidirectional };

    Graph() : Graph(GraphType::Directed, false) {}

    explicit Graph(GraphType type, bool allow_parallel_edges = false)
        : graph_(make_graph_variant(type, allow_parallel_edges)),
          type_(type),
          allow_parallel_edges_(allow_parallel_edges) {}

    Graph(const Graph&) = default;
    Graph(Graph&&) noexcept = default;
    Graph& operator=(const Graph&) = default;
    Graph& operator=(Graph&&) noexcept = default;
    ~Graph() = default;

    [[nodiscard]] GraphType type() const noexcept { return type_; }

    [[nodiscard]] bool allows_parallel_edges() const noexcept {
        return allow_parallel_edges_;
    }

    [[nodiscard]] std::size_t add_vertex(
        std::string_view name, const VertexProperty& props = VertexProperty()) {
        const std::string name_str(name);
        if (const auto it = name_to_vertex_.find(name_str);
            it != name_to_vertex_.end()) {
            return it->second;
        }

        VertexProperty vertex_props = props;
        std::string canonical_name = name_str;

        if constexpr (requires(VertexProperty& v) { v.name; }) {
            vertex_props.name = name;
            canonical_name = vertex_props.name;
        }

        const std::size_t vertex_id = std::visit(
            [&](auto& g) {
                const auto descriptor = boost::add_vertex(vertex_props, g);
                auto& stored = g[descriptor];
                if constexpr (requires(VertexProperty& v, std::size_t id) {
                                  v.id = id;
                              }) {
                    stored.id = static_cast<std::size_t>(descriptor);
                }
                return static_cast<std::size_t>(descriptor);
            },
            graph_);

        if (vertex_id >= vertex_names_.size()) {
            vertex_names_.resize(vertex_id + 1);
        }
        vertex_names_[vertex_id] = canonical_name;
        name_to_vertex_.emplace(vertex_names_[vertex_id], vertex_id);
        return vertex_id;
    }

    bool add_edge(std::string_view source, std::string_view target,
                  const EdgeProperty& props = EdgeProperty()) {
        const auto source_id = add_vertex(source);
        const auto target_id = add_vertex(target);
        return add_edge(source_id, target_id, props);
    }

    bool add_edge(std::size_t source_id, std::size_t target_id,
                  const EdgeProperty& props = EdgeProperty()) {
        if (source_id >= vertex_count() || target_id >= vertex_count()) {
            return false;
        }

        return std::visit(
            [&](auto& g) {
                const auto [edge, inserted] =
                    boost::add_edge(source_id, target_id, props, g);
                (void)edge;
                return inserted || allow_parallel_edges_;
            },
            graph_);
    }

    [[nodiscard]] std::size_t vertex_count() const {
        return std::visit(
            [](const auto& g) {
                return static_cast<std::size_t>(boost::num_vertices(g));
            },
            graph_);
    }

    [[nodiscard]] std::size_t edge_count() const {
        return std::visit(
            [](const auto& g) {
                return static_cast<std::size_t>(boost::num_edges(g));
            },
            graph_);
    }

    [[nodiscard]] std::vector<std::string> shortest_path(
        std::string_view source, std::string_view target) {
        const auto source_id = get_vertex_id(source);
        const auto target_id = get_vertex_id(target);
        if (source_id == invalid_vertex || target_id == invalid_vertex) {
            return {};
        }
        return shortest_path(source_id, target_id);
    }

    [[nodiscard]] std::vector<std::string> shortest_path(
        std::size_t source_id, std::size_t target_id) {
        if (source_id >= vertex_count() || target_id >= vertex_count()) {
            return {};
        }

        const std::vector<std::size_t> path_indices = std::visit(
            [&](auto& g) {
                return shortest_path_impl(g, source_id, target_id);
            },
            graph_);

        std::vector<std::string> result;
        result.reserve(path_indices.size());
        for (const auto id : path_indices) {
            result.emplace_back(get_vertex_name(id));
        }
        return result;
    }

    [[nodiscard]] std::string get_vertex_name(std::size_t vertex_id) const {
        if (vertex_id >= vertex_names_.size()) {
            return {};
        }
        return vertex_names_[vertex_id];
    }

    [[nodiscard]] std::size_t get_vertex_id(std::string_view name) const {
        const std::string name_str(name);
        if (const auto it = name_to_vertex_.find(name_str);
            it != name_to_vertex_.end()) {
            return it->second;
        }
        return invalid_vertex;
    }

private:
    static constexpr std::size_t invalid_vertex =
        std::numeric_limits<std::size_t>::max();

    template <bool AllowParallel>
    using OutEdgeSelector =
        std::conditional_t<AllowParallel, boost::listS, boost::vecS>;

    template <bool AllowParallel>
    using DirectedGraphImpl =
        boost::adjacency_list<OutEdgeSelector<AllowParallel>, boost::vecS,
                              boost::directedS, VertexProperty, EdgeProperty,
                              boost::no_property, boost::listS>;

    template <bool AllowParallel>
    using UndirectedGraphImpl =
        boost::adjacency_list<OutEdgeSelector<AllowParallel>, boost::vecS,
                              boost::undirectedS, VertexProperty, EdgeProperty,
                              boost::no_property, boost::listS>;

    template <bool AllowParallel>
    using BidirectionalGraphImpl =
        boost::adjacency_list<OutEdgeSelector<AllowParallel>, boost::vecS,
                              boost::bidirectionalS, VertexProperty,
                              EdgeProperty, boost::no_property, boost::listS>;

    using GraphVariant =
        std::variant<DirectedGraphImpl<false>, DirectedGraphImpl<true>,
                     UndirectedGraphImpl<false>, UndirectedGraphImpl<true>,
                     BidirectionalGraphImpl<false>,
                     BidirectionalGraphImpl<true>>;

    [[nodiscard]] static GraphVariant make_graph_variant(GraphType type,
                                                         bool allow_parallel) {
        if (allow_parallel) {
            switch (type) {
                case GraphType::Directed:
                    return GraphVariant{
                        std::in_place_type<DirectedGraphImpl<true>>};
                case GraphType::Undirected:
                    return GraphVariant{
                        std::in_place_type<UndirectedGraphImpl<true>>};
                case GraphType::Bidirectional:
                    return GraphVariant{
                        std::in_place_type<BidirectionalGraphImpl<true>>};
            }
        }

        switch (type) {
            case GraphType::Directed:
                return GraphVariant{
                    std::in_place_type<DirectedGraphImpl<false>>};
            case GraphType::Undirected:
                return GraphVariant{
                    std::in_place_type<UndirectedGraphImpl<false>>};
            case GraphType::Bidirectional:
                return GraphVariant{
                    std::in_place_type<BidirectionalGraphImpl<false>>};
        }
        return GraphVariant{std::in_place_type<DirectedGraphImpl<false>>};
    }

    template <typename Impl>
    [[nodiscard]] static double extract_edge_weight(
        const Impl& g, typename boost::graph_traits<Impl>::edge_descriptor e) {
        const auto& property = g[e];
        if constexpr (requires { property.weight; }) {
            return static_cast<double>(property.weight);
        } else if constexpr (std::is_arithmetic_v<EdgeProperty>) {
            return static_cast<double>(property);
        } else {
            return 1.0;
        }
    }

    template <typename Impl>
    [[nodiscard]] static auto make_weight_map(const Impl& g) {
        using edge_descriptor =
            typename boost::graph_traits<Impl>::edge_descriptor;
        if constexpr (std::is_same_v<EdgeProperty, boost::no_property>) {
            return boost::make_static_property_map<edge_descriptor>(1.0);
        } else {
            return boost::make_function_property_map<edge_descriptor>(
                [&g](edge_descriptor e) noexcept {
                    return extract_edge_weight(g, e);
                });
        }
    }

    template <typename Impl>
    [[nodiscard]] static std::vector<std::size_t> shortest_path_impl(
        const Impl& g, std::size_t source_id, std::size_t target_id) {
        const auto vertex_total =
            static_cast<std::size_t>(boost::num_vertices(g));
        if (source_id >= vertex_total || target_id >= vertex_total) {
            return {};
        }

        using vertex_descriptor =
            typename boost::graph_traits<Impl>::vertex_descriptor;
        std::vector<vertex_descriptor> predecessors(vertex_total);
        std::vector<double> distances(vertex_total,
                                      std::numeric_limits<double>::infinity());

        const auto index_map = boost::get(boost::vertex_index, g);
        auto predecessor_map =
            boost::make_iterator_property_map(predecessors.begin(), index_map);
        auto distance_map =
            boost::make_iterator_property_map(distances.begin(), index_map);

        distances[source_id] = 0.0;
        predecessors[source_id] = static_cast<vertex_descriptor>(source_id);

        const auto weight_map = make_weight_map(g);
        boost::dijkstra_shortest_paths(
            g, static_cast<vertex_descriptor>(source_id),
            boost::predecessor_map(predecessor_map)
                .distance_map(distance_map)
                .weight_map(weight_map));

        if (distances[target_id] == std::numeric_limits<double>::infinity()) {
            return {};
        }

        std::vector<std::size_t> path;
        auto current = static_cast<vertex_descriptor>(target_id);
        while (current != static_cast<vertex_descriptor>(source_id)) {
            path.push_back(static_cast<std::size_t>(current));
            const auto predecessor = predecessors[current];
            if (predecessor == current) {
                return {};
            }
            current = predecessor;
        }
        path.push_back(source_id);
        std::ranges::reverse(path);
        return path;
    }

    GraphVariant graph_;
    GraphType type_;
    bool allow_parallel_edges_;
    std::unordered_map<std::string, std::size_t> name_to_vertex_;
    std::vector<std::string> vertex_names_;
};

/**
 * @brief Convenience helper to create a graph with a set of options.
 */
template <typename VertexProperty = VertexProperties,
          typename EdgeProperty = EdgeProperties>
[[nodiscard]] Graph<VertexProperty, EdgeProperty> create_graph(
    std::initializer_list<GraphOptions> options = {}) {
    typename Graph<VertexProperty, EdgeProperty>::GraphType type =
        Graph<VertexProperty, EdgeProperty>::GraphType::Directed;
    bool allow_parallel_edges = false;

    for (const auto option : options) {
        switch (option) {
            case GraphOptions::Directed:
                type = Graph<VertexProperty, EdgeProperty>::GraphType::Directed;
                break;
            case GraphOptions::Undirected:
                type =
                    Graph<VertexProperty, EdgeProperty>::GraphType::Undirected;
                break;
            case GraphOptions::Bidirectional:
                type = Graph<VertexProperty,
                             EdgeProperty>::GraphType::Bidirectional;
                break;
            case GraphOptions::AllowParallelEdges:
                allow_parallel_edges = true;
                break;
            case GraphOptions::DisallowParallelEdges:
                allow_parallel_edges = false;
                break;
        }
    }

    return Graph<VertexProperty, EdgeProperty>(type, allow_parallel_edges);
}

}  // namespace atom::containers::graph

#endif  // defined(ATOM_HAS_BOOST_GRAPH)
