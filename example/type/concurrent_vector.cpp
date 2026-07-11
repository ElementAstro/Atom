/**
 * @file concurrent_vector.cpp
 * @brief Demonstrates atom::type::ConcurrentVector<T> (thread-safe vector with
 *        an internal worker pool).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>

#include "../atom/type/concurrent_vector.hpp"

using atom::type::ConcurrentVector;

int main() {
    ConcurrentVector<int> v(2);  // 2 worker threads
    std::cout << "=== push_back / emplace_back ===\n";
    for (int i = 1; i <= 5; ++i) {
        v.push_back(i);
    }
    v.emplace_back(6);
    std::cout << "  size = " << v.size() << '\n';

    std::cout << "\n=== indexed access ===\n";
    std::cout << "  elements:";
    for (size_t i = 0; i < v.size(); ++i) {
        std::cout << ' ' << v[i];
    }
    std::cout << '\n';

    std::cout << "\n=== clear ===\n";
    v.clear();
    std::cout << "  size after clear = " << v.size() << '\n';
    return 0;
}
