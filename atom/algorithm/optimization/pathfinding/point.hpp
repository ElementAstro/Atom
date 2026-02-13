#pragma once

#include <functional>

#include "../../core/rust_numeric.hpp"

namespace atom::algorithm {

//=============================================================================
// Point Structure
//=============================================================================
struct Point {
    i32 x;
    i32 y;

    // Using C++20 spaceship operator
    auto operator<=>(const Point&) const = default;
    bool operator==(const Point&) const = default;

    // Utility functions for point arithmetic
    Point operator+(const Point& other) const {
        return {x + other.x, y + other.y};
    }
    Point operator-(const Point& other) const {
        return {x - other.x, y - other.y};
    }
};

}  // namespace atom::algorithm

// Hash function for Point
namespace std {
template <>
struct hash<atom::algorithm::Point> {
    size_t operator()(const atom::algorithm::Point& p) const {
        return hash<atom::algorithm::i32>()(p.x) ^
               (hash<atom::algorithm::i32>()(p.y) << 1);
    }
};
}  // namespace std
