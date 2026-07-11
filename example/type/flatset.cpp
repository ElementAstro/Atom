/**
 * @file flatset.cpp
 * @brief Demonstrates atom::type::FlatSet<T> (a sorted-vector-backed set).
 *
 * Rewritten from scratch: the previous revision was corrupted (comments glued
 * into code) and did not compile.
 */

#include <iostream>

#include "../atom/type/flatset.hpp"

using atom::type::FlatSet;

int main() {
    std::cout << "=== Insert (duplicates ignored, kept sorted) ===\n";
    FlatSet<int> set;
    for (int x : {5, 1, 3, 1, 4, 2, 5}) {
        set.insert(x);
    }
    std::cout << "  size = " << set.size() << '\n';
    std::cout << "  sorted contents:";
    for (int x : set) {
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    std::cout << "\n=== contains / find ===\n";
    std::cout << "  contains(3) = " << set.contains(3) << '\n';
    std::cout << "  contains(9) = " << set.contains(9) << '\n';

    std::cout << "\n=== erase ===\n";
    set.erase(3);
    std::cout << "  contains(3) after erase = " << set.contains(3) << '\n';
    std::cout << "  size = " << set.size() << '\n';

    return 0;
}
