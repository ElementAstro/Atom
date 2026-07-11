/**
 * @file indestructible.cpp
 * @brief Demonstrates atom::type::Indestructible<T> (a never-destroyed wrapper,
 *        useful for function-local statics that must not run their destructor).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>

#include "../atom/type/indestructible.hpp"

using atom::type::Indestructible;

int main() {
    std::cout << "=== Construct in place + access ===\n";
    Indestructible<std::string> s(std::in_place, "persistent");
    std::cout << "  get()    = " << s.get() << '\n';
    std::cout << "  ->size() = " << s->size() << '\n';
    const std::string& ref = s;  // implicit operator T&
    std::cout << "  as T&    = " << ref << '\n';

    std::cout << "\n=== Mutate through the wrapper ===\n";
    s.get() += " value";
    std::cout << "  get()   = " << s.get() << '\n';
    return 0;
}
