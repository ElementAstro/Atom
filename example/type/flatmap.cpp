/**
 * @file flatmap.cpp
 * @brief Demonstrates atom::type::FlatMap<K, V> (sorted-vector-backed map).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>

#include "../atom/type/flatmap.hpp"

using atom::type::FlatMap;

int main() {
    FlatMap<std::string, int> m;
    std::cout << "=== insert / operator[] ===\n";
    m.insert({"apple", 1});
    m.insert({"banana", 2});
    m["cherry"] = 3;
    std::cout << "  size=" << m.size() << '\n';

    std::cout << "\n=== at / contains / find ===\n";
    std::cout << "  at(\"banana\")=" << m.at("banana") << '\n';
    std::cout << "  contains(\"cherry\")=" << m.contains("cherry") << '\n';
    auto it = m.find("apple");
    if (it != m.end()) {
        std::cout << "  find(\"apple\")->second=" << it->second << '\n';
    }

    std::cout << "\n=== iterate ===\n";
    for (const auto& [k, v] : m) {
        std::cout << "  " << k << " = " << v << '\n';
    }

    std::cout << "\n=== erase ===\n";
    m.erase("apple");
    std::cout << "  contains(\"apple\") after erase=" << m.contains("apple")
              << '\n';
    return 0;
}
