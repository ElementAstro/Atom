/**
 * @file concurrent_map.cpp
 * @brief Demonstrates atom::type::ConcurrentMap<K, V> (thread-safe map with an
 *        internal worker pool and optional LRU cache).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>

#include "../atom/type/concurrent_map.hpp"

using atom::type::ConcurrentMap;

int main() {
    ConcurrentMap<int, std::string> map;
    std::cout << "=== insert ===\n";
    map.insert(1, "one");
    map.insert(2, "two");
    std::cout << "  size = " << map.size() << '\n';

    std::cout << "\n=== find (returns optional) ===\n";
    if (auto v = map.find(1)) {
        std::cout << "  find(1) = " << *v << '\n';
    }
    std::cout << "  find(9).has_value() = " << map.find(9).has_value() << '\n';

    std::cout << "\n=== find_or_insert ===\n";
    map.find_or_insert(3, "three");
    std::cout << "  size after find_or_insert(3) = " << map.size() << '\n';
    return 0;
}
