/**
 * @file string.cpp
 * @brief Demonstrates atom::type::String.
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>

#include "../atom/type/string.hpp"

using atom::type::String;

int main() {
    String s("Hello, World");
    std::cout << "=== Basics ===\n";
    std::cout << "  c_str()  = " << s.cStr() << '\n';
    std::cout << "  length() = " << s.length() << '\n';
    std::cout << "  empty()  = " << s.empty() << '\n';

    std::cout << "\n=== substr ===\n";
    String sub = s.substr(7, 5);
    std::cout << "  substr(7,5) = " << sub.cStr() << '\n';
    return 0;
}
