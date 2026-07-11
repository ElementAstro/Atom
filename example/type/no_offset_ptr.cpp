/**
 * @file no_offset_ptr.cpp
 * @brief Demonstrates atom::type::UnshiftedPtr<T> (an in-place value holder
 * that behaves like a pointer but stores the object inline, no heap offset).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>

#include "atom/type/no_offset_ptr.hpp"

using atom::type::UnshiftedPtr;

int main() {
    std::cout << "=== Inline-held value, pointer-like access ===\n";
    UnshiftedPtr<int> p(42);
    std::cout << "  *p        = " << *p << '\n';

    std::cout << "\n=== Mutate then reset ===\n";
    *p = 100;
    std::cout << "  after *p=100: " << *p << '\n';
    p.reset(7);
    std::cout << "  after reset(7): " << *p << '\n';
    return 0;
}
