/**
 * @file static_vector.cpp
 * @brief Demonstrates atom::type::StaticVector<T, N> (fixed-capacity, no heap).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>

#include "../atom/type/static_vector.hpp"

using atom::type::StaticVector;

int main() {
    StaticVector<int, 8> v;
    std::cout << "=== pushBack ===\n";
    for (int i = 1; i <= 5; ++i) {
        v.pushBack(i * i);
    }
    std::cout << "  size=" << v.size() << " capacity=" << v.capacity() << '\n';
    std::cout << "  contents:";
    for (std::size_t i = 0; i < v.size(); ++i) {
        std::cout << ' ' << v[i];
    }
    std::cout << '\n';

    std::cout << "\n=== front / back / popBack ===\n";
    std::cout << "  front=" << v.front() << " back=" << v.back() << '\n';
    v.popBack();
    std::cout << "  size after popBack=" << v.size() << '\n';
    return 0;
}
