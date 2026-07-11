/**
 * @file robin_hood.cpp
 * @brief Demonstrates atom::type::unordered_flat_map (Robin Hood hashing).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>

#include "../atom/type/robin_hood.hpp"

using atom::type::unordered_flat_map;

int main() {
    unordered_flat_map<int, std::string> map;
    std::cout << "=== insert ===\n";
    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(3, "three");
    std::cout << "  size = " << map.size() << '\n';

    std::cout << "\n=== at / find ===\n";
    std::cout << "  at(2) = " << map.at(2) << '\n';
    auto it = map.find(3);
    if (it != map.end()) {
        std::cout << "  find(3)->second = " << it->second << '\n';
    }
    std::cout << "  find(9) == end() : " << (map.find(9) == map.end()) << '\n';

    std::cout << "\n=== iterate ===\n";
    for (const auto& [k, v] : map) {
        std::cout << "  " << k << " -> " << v << '\n';
    }
    return 0;
}
