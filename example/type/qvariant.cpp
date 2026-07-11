/**
 * @file qvariant.cpp
 * @brief Demonstrates atom::type::VariantWrapper<Types...> (a Qt-style
 * variant).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>

#include "../atom/type/qvariant.hpp"

using atom::type::VariantWrapper;

int main() {
    using Var = VariantWrapper<int, std::string, double>;

    Var v(42);
    std::cout << "=== Holding an int ===\n";
    std::cout << "  hasValue() = " << v.hasValue() << '\n';
    std::cout << "  typeName() = " << v.typeName() << '\n';
    std::cout << "  get<int>() = " << v.get<int>() << '\n';

    std::cout << "\n=== Reassign to a string ===\n";
    v = std::string("hello");
    std::cout << "  typeName()         = " << v.typeName() << '\n';
    std::cout << "  get<std::string>() = " << v.get<std::string>() << '\n';
    std::cout << "  index()            = " << v.index() << '\n';
    return 0;
}
