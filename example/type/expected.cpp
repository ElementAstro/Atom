/**
 * @file expected.cpp
 * @brief Demonstrates atom::type::expected<T, E> (a Rust-style Result).
 *
 * This example was rewritten from scratch: the previous revision was corrupted
 * (comments glued into code, scrambled line breaks) and did not compile.
 */

#include <iostream>
#include <string>

#include "atom/type/expected.hpp"

using atom::type::expected;
using atom::type::make_expected;
using atom::type::make_unexpected;

namespace {

// A function that may fail: returns the parsed value or an error message.
auto parsePositive(const std::string& text) -> expected<int, std::string> {
    try {
        size_t consumed = 0;
        int value = std::stoi(text, &consumed);
        if (consumed != text.size()) {
            return make_unexpected<std::string>("trailing characters in '" +
                                                text + "'");
        }
        if (value <= 0) {
            return make_unexpected<std::string>("value must be positive");
        }
        return value;
    } catch (const std::exception&) {
        return make_unexpected<std::string>("'" + text + "' is not a number");
    }
}

void printResult(const std::string& input,
                 const expected<int, std::string>& result) {
    std::cout << "  parse(\"" << input << "\") -> ";
    if (result.has_value()) {
        std::cout << "value " << result.value() << '\n';
    } else {
        std::cout << "error: " << result.error().error() << '\n';
    }
}

}  // namespace

int main() {
    std::cout << "=== Basic success / failure ===\n";
    printResult("42", parsePositive("42"));
    printResult("-3", parsePositive("-3"));
    printResult("abc", parsePositive("abc"));

    std::cout << "\n=== value_or (default on error) ===\n";
    std::cout << "  parse(\"bad\").value_or(0) = "
              << parsePositive("bad").value_or(0) << '\n';

    std::cout << "\n=== Monadic chaining (and_then / map) ===\n";
    // Double the parsed value, then format it — short-circuits on the error.
    auto doubled =
        parsePositive("21")
            .and_then([](int v) { return make_expected(v * 2); })
            .map([](int v) { return "doubled = " + std::to_string(v); });
    if (doubled.has_value()) {
        std::cout << "  " << doubled.value() << '\n';
    }

    auto shortCircuit = parsePositive("oops").and_then(
        [](int v) { return make_expected(v * 2); });
    std::cout << "  chained on bad input -> "
              << (shortCircuit.has_value() ? "value"
                                           : shortCircuit.error().error())
              << '\n';

    std::cout << "\n=== operator bool ===\n";
    if (auto r = parsePositive("7")) {
        std::cout << "  truthy, value = " << r.value() << '\n';
    }

    return 0;
}
