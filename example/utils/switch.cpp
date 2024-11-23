#include "atom/utils/switch.hpp"

#include <iostream>
#include <variant>

using namespace atom::utils;

int main() {
    // Create a StringSwitch instance
    StringSwitch<int> switcher;

    // Register cases with string keys and corresponding functions
    switcher.registerCase(
        "case1", [](int x) -> std::variant<std::monostate, int, std::string> {
            return x * 2;
        });
    switcher.registerCase(
        "case2", [](int x) -> std::variant<std::monostate, int, std::string> {
            return std::to_string(x) + " is the input";
        });

    // Set a default function to be called if no match is found
    switcher.setDefault(
        [](int x) -> std::variant<std::monostate, int, std::string> {
            return std::monostate{};
        });

    // Match a string key and execute the corresponding function
    auto result1 = switcher.match("case1", 5);
    if (result1) {
        if (std::holds_alternative<int>(*result1)) {
            std::cout << "Result of case1: " << std::get<int>(*result1)
                      << std::endl;
        }
    }

    auto result2 = switcher.match("case2", 10);
    if (result2) {
        if (std::holds_alternative<std::string>(*result2)) {
            std::cout << "Result of case2: " << std::get<std::string>(*result2)
                      << std::endl;
        }
    }

    auto result3 = switcher.match("case3", 15);
    if (result3) {
        if (std::holds_alternative<std::monostate>(*result3)) {
            std::cout << "Result of case3: default case executed" << std::endl;
        }
    }

    // Get a vector of all registered cases
    auto cases = switcher.getCases();
    std::cout << "Registered cases: ";
    for (const auto& c : cases) {
        std::cout << c << " ";
    }
    std::cout << std::endl;

    // Unregister a case
    switcher.unregisterCase("case1");

    // Clear all registered cases
    switcher.clearCases();

    return 0;
}