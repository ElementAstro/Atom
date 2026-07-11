/**
 * @file iter.cpp
 * @brief Demonstrates atom::type iterator utilities (zip iteration).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>
#include <vector>

#include "../atom/type/iter.hpp"

using atom::type::makeZipIterator;

int main() {
    std::vector<int> ids{1, 2, 3};
    std::vector<std::string> names{"alpha", "beta", "gamma"};

    std::cout << "=== Zip two sequences ===\n";
    auto begin = makeZipIterator(ids.begin(), names.begin());
    auto end = makeZipIterator(ids.end(), names.end());
    for (auto it = begin; it != end; ++it) {
        auto [id, name] = *it;
        std::cout << "  " << id << " -> " << name << '\n';
    }
    return 0;
}
