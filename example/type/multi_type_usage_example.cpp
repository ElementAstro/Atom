/**
 * @file multi_type_usage_example.cpp
 * @brief Demonstrates several atom::type utilities working together:
 *        Optional, SmallVector, and expected.
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>

#include "../atom/type/expected.hpp"
#include "../atom/type/optional.hpp"
#include "../atom/type/small_vector.hpp"

using atom::type::expected;
using atom::type::make_unexpected;
using atom::type::Optional;
using atom::type::SmallVector;

// Look up an index, returning Optional (absent when out of range).
Optional<int> lookup(const SmallVector<int, 4>& v, std::size_t i) {
    if (i >= v.size()) {
        return Optional<int>{};
    }
    return Optional<int>(v[i]);
}

// Validate a value, returning expected.
expected<int, std::string> requirePositive(int n) {
    if (n <= 0) {
        return make_unexpected<std::string>("not positive");
    }
    return n;
}

int main() {
    SmallVector<int, 4> data;
    data.pushBack(10);
    data.pushBack(-5);

    std::cout << "=== Optional lookup ===\n";
    auto a = lookup(data, 0);
    auto b = lookup(data, 9);
    std::cout << "  lookup(0) = "
              << (a.has_value() ? std::to_string(*a) : "none") << '\n';
    std::cout << "  lookup(9) = "
              << (b.has_value() ? std::to_string(*b) : "none") << '\n';

    std::cout << "\n=== expected validation ===\n";
    for (std::size_t i = 0; i < data.size(); ++i) {
        auto r = requirePositive(data[i]);
        std::cout << "  data[" << i << "] -> "
                  << (r.has_value() ? "ok" : r.error().error()) << '\n';
    }
    return 0;
}
