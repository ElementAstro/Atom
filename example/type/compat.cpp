#include <iostream>
#include <string>
#include <vector>

#include "atom/type/compat.hpp"

// Helper function to print section headers
void print_header(const std::string& title) {
    std::cout << "\n=== " << title << " ===" << std::endl;
    std::cout << std::string(title.length() + 8, '=') << std::endl;
}

// Function that returns expected<int, std::string>
atom::type::expected<int, std::string> divide(int a, int b) {
    if (b == 0) {
        return atom::type::unexpected<std::string>("Division by zero");
    }
    return a / b;
}

// Function that returns expected<std::string, std::string>
atom::type::expected<std::string, std::string> get_user_name(int user_id) {
    if (user_id <= 0) {
        return atom::type::unexpected<std::string>("Invalid user ID");
    }
    if (user_id > 1000) {
        return atom::type::unexpected<std::string>("User not found");
    }
    return "User_" + std::to_string(user_id);
}

// Function that returns expected<std::vector<int>, std::string>
atom::type::expected<std::vector<int>, std::string> parse_numbers(
    const std::string& input) {
    if (input.empty()) {
        return atom::type::unexpected<std::string>("Empty input");
    }

    std::vector<int> numbers;
    try {
        // Simple parsing - split by commas
        size_t start = 0;
        size_t end = input.find(',');

        while (end != std::string::npos) {
            std::string token = input.substr(start, end - start);
            numbers.push_back(std::stoi(token));
            start = end + 1;
            end = input.find(',', start);
        }

        // Last token
        std::string token = input.substr(start);
        numbers.push_back(std::stoi(token));

        return numbers;
    } catch (const std::exception& e) {
        return atom::type::unexpected<std::string>("Parse error: " +
                                                   std::string(e.what()));
    }
}

int main() {
    std::cout << "Compatibility Layer Usage Examples" << std::endl;
    std::cout << "==================================" << std::endl;

    std::cout << "This example demonstrates the compatibility layer for "
                 "expected/unexpected types."
              << std::endl;
    std::cout << "The layer automatically uses std::expected when available "
                 "(C++23) or falls back"
              << std::endl;
    std::cout << "to the custom implementation in atom::type::expected."
              << std::endl;

    // 1. Basic Expected Usage
    print_header("Basic Expected Usage");

    auto result1 = divide(10, 2);
    auto result2 = divide(10, 0);

    std::cout << "divide(10, 2): ";
    if (result1.has_value()) {
        std::cout << "Success: " << result1.value() << std::endl;
    } else {
        std::cout << "Error: " << result1.error() << std::endl;
    }

    std::cout << "divide(10, 0): ";
    if (result2.has_value()) {
        std::cout << "Success: " << result2.value() << std::endl;
    } else {
        std::cout << "Error: " << result2.error() << std::endl;
    }

    // 2. String Operations
    print_header("String Operations");

    auto name1 = get_user_name(42);
    auto name2 = get_user_name(-1);
    auto name3 = get_user_name(1001);

    std::cout << "get_user_name(42): ";
    if (name1) {
        std::cout << "Success: " << *name1 << std::endl;
    } else {
        std::cout << "Error: " << name1.error() << std::endl;
    }

    std::cout << "get_user_name(-1): ";
    if (name2) {
        std::cout << "Success: " << *name2 << std::endl;
    } else {
        std::cout << "Error: " << name2.error() << std::endl;
    }

    std::cout << "get_user_name(1001): ";
    if (name3) {
        std::cout << "Success: " << *name3 << std::endl;
    } else {
        std::cout << "Error: " << name3.error() << std::endl;
    }

    // 3. Complex Types
    print_header("Complex Types");

    auto numbers1 = parse_numbers("1,2,3,4,5");
    auto numbers2 = parse_numbers("");
    auto numbers3 = parse_numbers("1,2,invalid,4");

    std::cout << "parse_numbers(\"1,2,3,4,5\"): ";
    if (numbers1) {
        std::cout << "Success: [";
        for (size_t i = 0; i < numbers1->size(); ++i) {
            if (i > 0)
                std::cout << ", ";
            std::cout << (*numbers1)[i];
        }
        std::cout << "]" << std::endl;
    } else {
        std::cout << "Error: " << numbers1.error() << std::endl;
    }

    std::cout << "parse_numbers(\"\"): ";
    if (numbers2) {
        std::cout << "Success: [";
        for (size_t i = 0; i < numbers2->size(); ++i) {
            if (i > 0)
                std::cout << ", ";
            std::cout << (*numbers2)[i];
        }
        std::cout << "]" << std::endl;
    } else {
        std::cout << "Error: " << numbers2.error() << std::endl;
    }

    std::cout << "parse_numbers(\"1,2,invalid,4\"): ";
    if (numbers3) {
        std::cout << "Success: [";
        for (size_t i = 0; i < numbers3->size(); ++i) {
            if (i > 0)
                std::cout << ", ";
            std::cout << (*numbers3)[i];
        }
        std::cout << "]" << std::endl;
    } else {
        std::cout << "Error: " << numbers3.error() << std::endl;
    }

    // 4. Chaining Operations
    print_header("Chaining Operations");

    std::cout << "Demonstrating operation chaining:" << std::endl;

    auto chain_result =
        divide(20, 4)
            .and_then([](int value) -> atom::type::expected<int, std::string> {
                if (value > 10) {
                    return atom::type::unexpected<std::string>(
                        "Value too large");
                }
                return value * 2;
            })
            .and_then([](int value)
                          -> atom::type::expected<std::string, std::string> {
                return "Result: " + std::to_string(value);
            });

    std::cout
        << "Chain: divide(20, 4) -> check <= 10 -> multiply by 2 -> to string"
        << std::endl;
    if (chain_result) {
        std::cout << "Success: " << *chain_result << std::endl;
    } else {
        std::cout << "Error: " << chain_result.error() << std::endl;
    }

    // 5. Error Handling Patterns
    print_header("Error Handling Patterns");

    std::cout << "Different ways to handle expected values:" << std::endl;

    auto test_value = divide(15, 3);

    // Pattern 1: Direct check
    if (test_value.has_value()) {
        std::cout << "Pattern 1 - Direct check: " << test_value.value()
                  << std::endl;
    }

    // Pattern 2: Boolean conversion
    if (test_value) {
        std::cout << "Pattern 2 - Boolean conversion: " << *test_value
                  << std::endl;
    }

    // Pattern 3: Value or default
    int safe_value = test_value.value_or(-1);
    std::cout << "Pattern 3 - Value or default: " << safe_value << std::endl;

    // Pattern 4: Transform on success
    auto transformed = test_value.transform([](int val) { return val * 100; });
    if (transformed) {
        std::cout << "Pattern 4 - Transform on success: " << *transformed
                  << std::endl;
    }

    // 6. Compatibility Information
    print_header("Compatibility Information");

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    std::cout << "Using std::expected (C++23 standard library)" << std::endl;
#else
    std::cout << "Using atom::type::expected (custom implementation)"
              << std::endl;
#endif

    std::cout << "The compatibility layer ensures your code works regardless of"
              << std::endl;
    std::cout << "whether the standard library provides std::expected or not."
              << std::endl;

    std::cout << "\nAll compatibility examples completed successfully!"
              << std::endl;
    return 0;
}
