/**
 * @file auto_table.cpp
 * @brief Demonstrates atom::type::CountingHashTable<K, V> (a hash table that
 *        tracks per-key access counts).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>

#include "../atom/type/auto_table.hpp"

using atom::type::CountingHashTable;

int main() {
    CountingHashTable<std::string, int> table;
    std::cout << "=== insert / get ===\n";
    table.insert("alpha", 1);
    table.insert("beta", 2);

    auto a = table.get("alpha");
    auto missing = table.get("gamma");
    std::cout << "  get(\"alpha\")  = " << (a ? std::to_string(*a) : "none")
              << '\n';
    std::cout << "  get(\"gamma\")  = "
              << (missing ? std::to_string(*missing) : "none") << '\n';

    std::cout << "\n=== access counts ===\n";
    table.get("alpha");  // bump the access count
    auto count = table.getAccessCount("alpha");
    std::cout << "  getAccessCount(\"alpha\") = "
              << (count ? std::to_string(*count) : "none") << '\n';
    return 0;
}
