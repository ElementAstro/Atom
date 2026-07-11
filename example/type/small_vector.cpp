/**
 * @file small_vector.cpp
 * @brief Demonstrates atom::type::SmallVector<T, N> (inline small-buffer
 * vector).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <cassert>
#include <iostream>

#include "../atom/type/small_vector.hpp"

using atom::type::SmallVector;

int main() {
    SmallVector<int, 4> v;  // up to 4 elements live inline (no heap)
    std::cout << "=== push_back (within inline capacity) ===\n";
    for (int i = 1; i <= 3; ++i) {
        v.pushBack(i);
    }
    std::cout << "  size=" << v.size() << " capacity=" << v.capacity() << '\n';

    std::cout << "\n=== grow past inline N (spills to heap) ===\n";
    for (int i = 4; i <= 10; ++i) {
        v.pushBack(i);
    }
    std::cout << "  size=" << v.size() << " capacity=" << v.capacity() << '\n';

    std::cout << "  contents:";
    for (int x : v) {
        std::cout << ' ' << x;
    }
    std::cout << "\n  front=" << v.front() << " back=" << v.back() << '\n';

    v.popBack();
    std::cout << "  size after popBack=" << v.size() << '\n';
    return 0;
}
