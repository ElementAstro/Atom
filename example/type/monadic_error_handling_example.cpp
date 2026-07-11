/**
 * @file monadic_error_handling_example.cpp
 * @brief Demonstrates monadic error handling with atom::type::expected
 *        (and_then / map / or_else chains that short-circuit on the first
 * error).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>

#include "../atom/type/expected.hpp"

using atom::type::expected;
using atom::type::make_expected;
using atom::type::make_unexpected;

auto parse(const std::string& s) -> expected<int, std::string> {
    try {
        return std::stoi(s);
    } catch (...) {
        return make_unexpected<std::string>("not a number: " + s);
    }
}

auto reciprocalTimes100(int n) -> expected<int, std::string> {
    if (n == 0) {
        return make_unexpected<std::string>("division by zero");
    }
    return 100 / n;
}

int main() {
    auto run = [](const std::string& in) {
        auto result = parse(in).and_then(reciprocalTimes100).map([](int v) {
            return v + 1;
        });
        std::cout << "  \"" << in << "\" -> ";
        if (result.has_value()) {
            std::cout << "ok " << result.value() << '\n';
        } else {
            std::cout << "err: " << result.error().error() << '\n';
        }
    };

    std::cout << "=== and_then / map chain ===\n";
    run("4");    // parse 4 -> 25 -> 26
    run("0");    // division by zero
    run("abc");  // parse failure short-circuits the chain
    return 0;
}
