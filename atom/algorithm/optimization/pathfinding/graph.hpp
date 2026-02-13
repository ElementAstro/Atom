#pragma once

#include <concepts>
#include <ranges>
#include <vector>

#include "point.hpp"

namespace atom::algorithm {

//=============================================================================
// Graph Interface & Concept
//=============================================================================
// Abstract graph interface
template <typename NodeType>
class IGraph {
public:
    using node_type = NodeType;

    virtual ~IGraph() = default;
    virtual std::vector<NodeType> neighbors(const NodeType& node) const = 0;
    virtual f32 cost(const NodeType& from, const NodeType& to) const = 0;
};

// Concept for a valid Graph type
template <typename G>
concept Graph = requires(G g, typename G::node_type n) {
    { g.neighbors(n) } -> std::ranges::range;
    { g.cost(n, n) } -> std::convertible_to<f32>;
};

}  // namespace atom::algorithm
