#pragma once

#include <concepts>
#include <functional>
#include <type_traits>

#include "point.hpp"

namespace atom::algorithm {

//=============================================================================
// Heuristic Functions & Concept
//=============================================================================
namespace heuristics {

// Heuristic concept
template <typename F, typename Node>
concept Heuristic =
    std::invocable<F, Node, Node> &&
    std::convertible_to<std::invoke_result_t<F, Node, Node>, f32>;

// Heuristic functions
f32 manhattan(const Point& a, const Point& b);
f32 euclidean(const Point& a, const Point& b);
f32 diagonal(const Point& a, const Point& b);
f32 zero(const Point& a, const Point& b);
f32 octile(const Point& a, const Point& b);  // Optimized diagonal heuristic

}  // namespace heuristics

}  // namespace atom::algorithm
