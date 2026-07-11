/**
 * @file concurrent_set.cpp
 * @brief Demonstrates atom::type::ConcurrentSet<T> (thread-safe set with an
 *        internal worker pool and optional LRU cache).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>

#include "../atom/type/concurrent_set.hpp"

using atom::type::ConcurrentSet;

int main() {
    ConcurrentSet<int> set;
    std::cout << "=== insert ===\n";
    set.insert(1);
    set.insert(2);
    set.insert(2);  // duplicate ignored
    std::cout << "  size = " << set.size() << '\n';

    std::cout << "\n=== find ===\n";
    std::cout << "  find(2).has_value() = " << set.find(2).has_value() << '\n';
    std::cout << "  find(9).has_value() = " << set.find(9).has_value() << '\n';

    std::cout << "\n=== async_insert + wait_for_tasks ===\n";
    set.async_insert(3);
    set.wait_for_tasks(1000);
    std::cout << "  size after async insert = " << set.size() << '\n';

    std::cout << "\n=== erase ===\n";
    set.erase(1);
    std::cout << "  find(1).has_value() = " << set.find(1).has_value() << '\n';
    return 0;
}
