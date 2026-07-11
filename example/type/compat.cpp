/**
 * @file compat.cpp
 * @brief Demonstrates atom::type::compat — aliases that map to std::expected
 *        when available, otherwise to atom's self-built expected.
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>

#include "../atom/type/compat.hpp"

auto half(int n) -> atom::type::compat::expected<int, std::string> {
    if (n % 2 != 0) {
        return atom::type::compat::unexpected<std::string>("odd input");
    }
    return n / 2;
}

int main() {
    std::cout << "=== compat::expected ===\n";
    for (int n : {8, 5}) {
        auto r = half(n);
        std::cout << "  half(" << n << ") -> ";
        if (r.has_value()) {
            std::cout << "value " << r.value() << '\n';
        } else {
            std::cout << "error\n";
        }
    }
    return 0;
}
