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

// Enable only if ATOM_USE_BOOST_GRAPH is defined and Boost Graph Library is
// available
#if defined(ATOM_HAS_BOOST_GRAPH)

#include "../macro.hpp"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/directed_graph.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/undirected_graph.hpp>
#include <boost/property_map/property_map.hpp>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace atom {
namespace containers {
namespace graph {

/**
 * @brief Base class for graph vertex properties
 *
 * Can be inherited to add custom vertex properties
 */
struct VertexProperties {
    std::string name;  // Vertex name
    std::size_t id;    // Vertex ID

    VertexProperties() : id(0) {}
    explicit VertexProperties(const std::string& n, std::size_t i = 0)
        : name(n), id(i) {}
};

/**
 * @brief Base class for graph edge properties
 *
 * Can be inherited to add custom edge properties
 */
struct EdgeProperties {
    double weight;      // Edge weight
    std::string label;  // Edge label

    EdgeProperties() : weight(1.0) {}
    explicit EdgeProperties(double w, const std::string& l = "")
        : weight(w), label(l) {}
};

/**
 * @brief Graph options enum
 */
enum class GraphOptions {
    Directed,       // Directed graph
    Undirected,     // Undirected graph
    Bidirectional,  // Bidirectional graph (optimization for directed graphs)
    AllowParallelEdges,    // Allow parallel edges
    DisallowParallelEdges  // Disallow parallel edges
};

/**
 * @brief High-performance graph implementation
 *
 * Based on the Boost.Graph library, provides efficient graph algorithms and
 * data structures
 *
 * @tparam VertexProperty Vertex property type
 * @tparam EdgeProperty Edge property type
 */
template <typename VertexProperty = VertexProperties,
          typename EdgeProperty = EdgeProperties>
class Graph {
public:
    // Define graph structure types
    using DirectedGraph =
        boost::adjacency_list<boost::vecS,  // Container for external edge list
                              boost::vecS,  // Container for vertex list
                              boost::directedS,    // Directed graph
                              VertexProperty,      // Vertex properties
                              EdgeProperty,        // Edge properties
                              boost::no_property,  // Graph properties
                              boost::listS         // Container for edge list
                              >;

    using UndirectedGraph =
        boost::adjacency_list<boost::vecS,  // Container for external edge list
                              boost::vecS,  // Container for vertex list
                              boost::undirectedS,  // Undirected graph
                              VertexProperty,      // Vertex properties
                              EdgeProperty,        // Edge properties
                              boost::no_property,  // Graph properties
                              boost::listS         // Container for edge list
                              >;

    using BidirectionalGraph =
        boost::adjacency_list<boost::vecS,  // Container for external edge list
                              boost::vecS,  // Container for vertex list
                              boost::bidirectionalS,  // Bidirectional graph
                              VertexProperty,         // Vertex properties
                              EdgeProperty,           // Edge properties
                              boost::no_property,     // Graph properties
                              boost::listS            // Container for edge list
                              >;

    // Union for selecting graph type
    union GraphUnion {
        DirectedGraph* directed;
        UndirectedGraph* undirected;
        BidirectionalGraph* bidirectional;

        GraphUnion() : directed(nullptr) {}
        ~GraphUnion() {
        }  // Destructor does nothing, manually managed in the Graph class
    };

    // Graph type
    enum class GraphType { Directed, Undirected, Bidirectional };

private:
    GraphUnion graph_;
    GraphType type_;
    bool allow_parallel_edges_;

    // Mapping of vertex names to IDs
    std::unordered_map<std::string, std::size_t> name_to_vertex_;

public:
    /**
     * @brief Constructor
     *
     * @param type Graph type
     * @param allow_parallel_edges Whether parallel edges are allowed
     */
    explicit Graph(GraphType type = GraphType::Directed,
                   bool allow_parallel_edges = false)
        : type_(type), allow_parallel_edges_(allow_parallel_edges) {
        switch (type_) {
            case GraphType::Directed:
                graph_.directed = new DirectedGraph();
                break;
            case GraphType::Undirected:
                graph_.undirected = new UndirectedGraph();
                break;
            case GraphType::Bidirectional:
                graph_.bidirectional = new BidirectionalGraph();
                break;
        }
    }

    /**
     * @brief Destructor
     */
    ~Graph() {
        switch (type_) {
            case GraphType::Directed:
                delete graph_.directed;
                break;
            case GraphType::Undirected:
                delete graph_.undirected;
                break;
            case GraphType::Bidirectional:
                delete graph_.bidirectional;
                break;
        }
    }

    // Disable copy
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;

    /**
     * @brief Move constructor
     */
    Graph(Graph&& other) noexcept
        : graph_(other.graph_),
          type_(other.type_),
          allow_parallel_edges_(other.allow_parallel_edges_),
          name_to_vertex_(std::move(other.name_to_vertex_)) {
        // Prevent double deletion
        switch (other.type_) {
            case GraphType::Directed:
                other.graph_.directed = nullptr;
                break;
            case GraphType::Undirected:
                other.graph_.undirected = nullptr;
                break;
            case GraphType::Bidirectional:
                other.graph_.bidirectional = nullptr;
                break;
        }
    }

    /**
     * @brief Move assignment operator
     */
    Graph& operator=(Graph&& other) noexcept {
        if (this != &other) {
            // Release current resources
            switch (type_) {
                case GraphType::Directed:
                    delete graph_.directed;
                    break;
                case GraphType::Undirected:
                    delete graph_.undirected;
                    break;
                case GraphType::Bidirectional:
                    delete graph_.bidirectional;
                    break;
            }

            // Move data
            graph_ = other.graph_;
            type_ = other.type_;
            allow_parallel_edges_ = other.allow_parallel_edges_;
            name_to_vertex_ = std::move(other.name_to_vertex_);

            // Prevent double deletion
            switch (other.type_) {
                case GraphType::Directed:
                    other.graph_.directed = nullptr;
                    break;
                case GraphType::Undirected:
                    other.graph_.undirected = nullptr;
                    break;
                case GraphType::Bidirectional:
                    other.graph_.bidirectional = nullptr;
                    break;
            }
        }
        return *this;
    }

    /**
     * @brief Add a vertex
     *
     * @param name Vertex name
     * @param props Vertex properties
     * @return std::size_t Vertex ID
     */
    std::size_t add_vertex(const std::string& name,
                           const VertexProperty& props = VertexProperty()) {
        // Check if the name already exists
        auto it = name_to_vertex_.find(name);
        if (it != name_to_vertex_.end()) {
            return it->second;  // Return existing vertex ID
        }

        // Create a vertex with properties
        VertexProperty vertex_props = props;
        vertex_props.name = name;

        std::size_t vertex_id = 0;
        switch (type_) {
            case GraphType::Directed: {
                vertex_id = boost::add_vertex(vertex_props, *graph_.directed);
                vertex_props.id = vertex_id;
                (*graph_.directed)[vertex_id] = vertex_props;
                break;
            }
            case GraphType::Undirected: {
                vertex_id = boost::add_vertex(vertex_props, *graph_.undirected);
                vertex_props.id = vertex_id;
                (*graph_.undirected)[vertex_id] = vertex_props;
                break;
            }
            case GraphType::Bidirectional: {
                vertex_id =
                    boost::add_vertex(vertex_props, *graph_.bidirectional);
                vertex_props.id = vertex_id;
                (*graph_.bidirectional)[vertex_id] = vertex_props;
                break;
            }
        }

        // Store the mapping of name to ID
        name_to_vertex_[name] = vertex_id;
        return vertex_id;
    }

    /**
     * @brief Add an edge
     *
     * @param source Source vertex name
     * @param target Target vertex name
     * @param props Edge properties
     * @return bool Whether the addition was successful
     */
    bool add_edge(const std::string& source, const std::string& target,
                  const EdgeProperty& props = EdgeProperty()) {
        // Ensure vertices exist
        std::size_t source_id = add_vertex(source);
        std::size_t target_id = add_vertex(target);

        return add_edge(source_id, target_id, props);
    }

    /**
     * @brief Add an edge (using vertex IDs)
     *
     * @param source_id Source vertex ID
     * @param target_id Target vertex ID
     * @param props Edge properties
     * @return bool Whether the addition was successful
     */
    bool add_edge(std::size_t source_id, std::size_t target_id,
                  const EdgeProperty& props = EdgeProperty()) {
        bool success = false;
        switch (type_) {
            case GraphType::Directed: {
                auto result = boost::add_edge(source_id, target_id, props,
                                              *graph_.directed);
                success = result.second || allow_parallel_edges_;
                break;
            }
            case GraphType::Undirected: {
                auto result = boost::add_edge(source_id, target_id, props,
                                              *graph_.undirected);
                success = result.second || allow_parallel_edges_;
                break;
            }
            case GraphType::Bidirectional: {
                auto result = boost::add_edge(source_id, target_id, props,
                                              *graph_.bidirectional);
                success = result.second || allow_parallel_edges_;
                break;
            }
        }
        return success;
    }

    /**
     * @brief Get the number of vertices
     *
     * @return std::size_t Number of vertices
     */
    std::size_t vertex_count() const {
        switch (type_) {
            case GraphType::Directed:
                return boost::num_vertices(*graph_.directed);
            case GraphType::Undirected:
                return boost::num_vertices(*graph_.undirected);
            case GraphType::Bidirectional:
                return boost::num_vertices(*graph_.bidirectional);
        }
        return 0;
    }

    /**
     * @brief Get the number of edges
     *
     * @return std::size_t Number of edges
     */
    std::size_t edge_count() const {
        switch (type_) {
            case GraphType::Directed:
                return boost::num_edges(*graph_.directed);
            case GraphType::Undirected:
                return boost::num_edges(*graph_.undirected);
            case GraphType::Bidirectional:
                return boost::num_edges(*graph_.bidirectional);
        }
        return 0;
    }

    /**
     * @brief Calculate the shortest path (Dijkstra's algorithm)
     *
     * @param source Source vertex name
     * @param target Target vertex name
     * @return std::vector<std::string> List of vertex names on the path
     */
    std::vector<std::string> shortest_path(const std::string& source,
                                           const std::string& target) {
        // Get vertex IDs
        auto src_it = name_to_vertex_.find(source);
        auto tgt_it = name_to_vertex_.find(target);

        if (src_it == name_to_vertex_.end() ||
            tgt_it == name_to_vertex_.end()) {
            return {};  // Source or target vertex does not exist
        }

        return shortest_path(src_it->second, tgt_it->second);
    }

    /**
     * @brief Calculate the shortest path (Dijkstra's algorithm, using vertex
     * IDs)
     *
     * @param source_id Source vertex ID
     * @param target_id Target vertex ID
     * @return std::vector<std::string> List of vertex names on the path
     */
    std::vector<std::string> shortest_path(std::size_t source_id,
                                           std::size_t target_id) {
        std::vector<std::string> path;
        std::vector<std::size_t> predecessors(vertex_count());
        std::vector<double> distances(vertex_count());

        switch (type_) {
            case GraphType::Directed:
                calculate_shortest_path(*graph_.directed, source_id,
                                        predecessors, distances);
                break;
            case GraphType::Undirected:
                calculate_shortest_path(*graph_.undirected, source_id,
                                        predecessors, distances);
                break;
            case GraphType::Bidirectional:
                calculate_shortest_path(*graph_.bidirectional, source_id,
                                        predecessors, distances);
                break;
        }

        // Build the path
        if (distances[target_id] == std::numeric_limits<double>::max()) {
            return {};  // Cannot reach the target
        }

        // Trace back from target to source
        for (std::size_t v = target_id; v != source_id; v = predecessors[v]) {
            path.push_back(get_vertex_name(v));
            if (v == predecessors[v])
                break;  // Prevent loop
        }
        path.push_back(get_vertex_name(source_id));

        // Reverse the path to make it from source to target
        std::reverse(path.begin(), path.end());
        return path;
    }

    /**
     * @brief Get vertex name
     *
     * @param vertex_id Vertex ID
     * @return std::string Vertex name
     */
    std::string get_vertex_name(std::size_t vertex_id) const {
        switch (type_) {
            case GraphType::Directed:
                return (*graph_.directed)[vertex_id].name;
            case GraphType::Undirected:
                return (*graph_.undirected)[vertex_id].name;
            case GraphType::Bidirectional:
                return (*graph_.bidirectional)[vertex_id].name;
        }
        return "";
    }

    /**
     * @brief Get vertex ID
     *
     * @param name Vertex name
     * @return std::size_t Vertex ID, returns -1 if it does not exist
     */
    std::size_t get_vertex_id(const std::string& name) const {
        auto it = name_to_vertex_.find(name);
        if (it != name_to_vertex_.end()) {
            return it->second;
        }
        return static_cast<std::size_t>(-1);  // Indicates an invalid ID
    }

private:
    // Helper function to calculate the shortest path
    template <typename GraphType>
    void calculate_shortest_path(const GraphType& g, std::size_t source_id,
                                 std::vector<std::size_t>& predecessors,
                                 std::vector<double>& distances) {
        // Initialize
        predecessors.resize(boost::num_vertices(g));
        distances.resize(boost::num_vertices(g));

        auto weight_map = boost::get(&EdgeProperty::weight, g);
        auto predecessor_map = boost::make_iterator_property_map(
            predecessors.begin(), boost::get(boost::vertex_index, g));
        auto distance_map = boost::make_iterator_property_map(
            distances.begin(), boost::get(boost::vertex_index, g));

        // Execute Dijkstra's algorithm
        boost::dijkstra_shortest_paths(g, source_id,
                                       boost::predecessor_map(predecessor_map)
                                           .distance_map(distance_map)
                                           .weight_map(weight_map));
    }
};

/**
 * @brief Convenience factory function for creating graphs
 *
 * @tparam VertexProperty Vertex property type
 * @tparam EdgeProperty Edge property type
 * @param options Graph options
 * @return Graph<VertexProperty, EdgeProperty> The created graph
 */
template <typename VertexProperty = VertexProperties,
          typename EdgeProperty = EdgeProperties>
Graph<VertexProperty, EdgeProperty> create_graph(
    std::initializer_list<GraphOptions> options = {}) {
    typename Graph<VertexProperty, EdgeProperty>::GraphType type =
        Graph<VertexProperty, EdgeProperty>::GraphType::Directed;
    bool allow_parallel_edges = false;

    for (auto opt : options) {
        switch (opt) {
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

}  // namespace graph
}  // namespace containers
}  // namespace atom

#endif  // defined(ATOM_HAS_BOOST_GRAPH)