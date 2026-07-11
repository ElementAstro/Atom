/**
 * @file cstream.cpp
 * @brief Demonstrates atom::type::CStream (a fluent wrapper over a container).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <vector>

#include "../atom/type/cstream.hpp"

using atom::type::makeStreamCopy;

int main() {
    std::vector<int> data{5, 3, 1, 4, 2};

    std::cout << "=== sorted ===\n";
    auto sortedVec = makeStreamCopy(data).sorted().get();
    std::cout << "  ";
    for (int x : sortedVec) {
        std::cout << x << ' ';
    }
    std::cout << '\n';

    std::cout << "\n=== transform (x -> x*x) ===\n";
    auto squared = makeStreamCopy(data)
                       .transform<std::vector<int>>([](int x) { return x * x; })
                       .get();
    std::cout << "  ";
    for (int x : squared) {
        std::cout << x << ' ';
    }
    std::cout << '\n';
    return 0;
}
