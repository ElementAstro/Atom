/**
 * @file optional.cpp
 * @brief Demonstrates atom::type::Optional<T>.
 *
 * Rewritten from scratch: the previous revision was corrupted (comments glued
 * into code) and did not compile.
 */

#include <iostream>
#include <string>

#include "../atom/type/optional.hpp"

using atom::type::Optional;

int main() {
    std::cout << "=== Empty vs engaged ===\n";
    Optional<int> empty;
    Optional<int> engaged(42);
    std::cout << "  empty.has_value()   = " << empty.has_value() << '\n';
    std::cout << "  engaged.has_value() = " << engaged.has_value() << '\n';
    std::cout << "  *engaged            = " << *engaged << '\n';

    std::cout << "\n=== value_or ===\n";
    std::cout << "  empty.value_or(-1)   = " << empty.value_or(-1) << '\n';
    std::cout << "  engaged.value_or(-1) = " << engaged.value_or(-1) << '\n';

    std::cout << "\n=== emplace / reset ===\n";
    Optional<std::string> s;
    s.emplace("hello");
    std::cout << "  after emplace: " << s.value() << '\n';
    s.reset();
    std::cout << "  after reset, has_value = " << s.has_value() << '\n';

    std::cout << "\n=== operator bool ===\n";
    if (engaged) {
        std::cout << "  engaged is truthy, value = " << engaged.value() << '\n';
    }

    return 0;
}
