/*
 * string_switch_example.cpp
 *
 * Copyright (C) 2024 Max Q.
 *
 * Example usage of the atom::utils::StringSwitch class
 */

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <tuple>
#include <variant>
#include <vector>

#include "atom/utils/switch.hpp"

// Helper function to print variant return values
void printReturnValue(
    const std::optional<std::variant<std::monostate, int, std::string>>&
        result) {
    if (!result) {
        std::cout << "No match found (nullopt)" << std::endl;
        return;
    }

    if (std::holds_alternative<std::monostate>(*result)) {
        std::cout << "Return: <monostate>" << std::endl;
    } else if (std::holds_alternative<int>(*result)) {
        std::cout << "Return: " << std::get<int>(*result) << " (int)"
                  << std::endl;
    } else if (std::holds_alternative<std::string>(*result)) {
        std::cout << "Return: \"" << std::get<std::string>(*result)
                  << "\" (string)" << std::endl;
    }
}

// Helper function to print a collection
template <typename Collection>
void printCollection(const Collection& coll, const std::string& title) {
    std::cout << title << ":\n";
    for (const auto& item : coll) {
        std::cout << "  - " << item << std::endl;
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "=== StringSwitch Comprehensive Example ===\n\n";

    std::cout << "Example 1: Basic StringSwitch Usage\n";
    {
        // Create a non-thread-safe StringSwitch with no additional parameters
        atom::utils::StringSwitch<false> commandSwitch;

        // Register cases with different return types
        commandSwitch.registerCase(
            "help", []() -> std::variant<std::monostate, int, std::string> {
                return "Displays help information";
            });

        commandSwitch.registerCase(
            "version", []() -> std::variant<std::monostate, int, std::string> {
                return "v1.0.0";
            });

        commandSwitch.registerCase(
            "exit", []() -> std::variant<std::monostate, int, std::string> {
                return 0;  // Return exit code
            });

        commandSwitch.registerCase(
            "void", []() -> std::variant<std::monostate, int, std::string> {
                return std::monostate{};  // Return nothing
            });

        // Match cases
        std::cout << "Matching 'help': ";
        printReturnValue(commandSwitch.match("help"));

        std::cout << "Matching 'version': ";
        printReturnValue(commandSwitch.match("version"));

        std::cout << "Matching 'exit': ";
        printReturnValue(commandSwitch.match("exit"));

        std::cout << "Matching 'void': ";
        printReturnValue(commandSwitch.match("void"));

        std::cout << "Matching 'unknown': ";
        printReturnValue(commandSwitch.match("unknown"));
    }
    std::cout << std::endl;

    std::cout << "Example 2: StringSwitch with Default Case\n";
    {
        atom::utils::StringSwitch<false> colorSwitch;

        // Register cases
        colorSwitch.registerCase(
            "red", []() -> std::variant<std::monostate, int, std::string> {
                return "#FF0000";
            });

        colorSwitch.registerCase(
            "green", []() -> std::variant<std::monostate, int, std::string> {
                return "#00FF00";
            });

        colorSwitch.registerCase(
            "blue", []() -> std::variant<std::monostate, int, std::string> {
                return "#0000FF";
            });

        // Set default case
        colorSwitch.setDefault(
            []() -> std::variant<std::monostate, int, std::string> {
                return "Unknown color";
            });

        // Match valid and unknown cases
        std::cout << "Matching 'red': ";
        printReturnValue(colorSwitch.match("red"));

        std::cout << "Matching 'yellow' (will use default): ";
        printReturnValue(colorSwitch.match("yellow"));
    }
    std::cout << std::endl;

    std::cout << "Example 3: StringSwitch with Parameters\n";
    {
        // Create a switch that accepts int and string parameters
        atom::utils::StringSwitch<false, int, std::string> mathSwitch;

        // Register cases with functions that take parameters
        mathSwitch.registerCase(
            "add",
            [](int x, const std::string& op)
                -> std::variant<std::monostate, int, std::string> {
                return x + std::stoi(op);
            });

        mathSwitch.registerCase(
            "multiply",
            [](int x, const std::string& op)
                -> std::variant<std::monostate, int, std::string> {
                return x * std::stoi(op);
            });

        mathSwitch.registerCase(
            "describe",
            [](int x, const std::string& op)
                -> std::variant<std::monostate, int, std::string> {
                return "Operation: " + op + " with " + std::to_string(x);
            });

        // Match with parameters
        std::cout << "Matching 'add' with parameters (5, \"3\"): ";
        printReturnValue(mathSwitch.match("add", 5, "3"));

        std::cout << "Matching 'multiply' with parameters (4, \"7\"): ";
        printReturnValue(mathSwitch.match("multiply", 4, "7"));

        std::cout << "Matching 'describe' with parameters (10, \"square\"): ";
        printReturnValue(mathSwitch.match("describe", 10, "square"));
    }
    std::cout << std::endl;

    std::cout << "Example 4: Thread-safe StringSwitch\n";
    {
        // Create a thread-safe StringSwitch
        atom::utils::StringSwitch<true> safeSwitch;

        // Register some cases
        safeSwitch.registerCase(
            "thread1", []() -> std::variant<std::monostate, int, std::string> {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                return "Response from thread 1";
            });

        safeSwitch.registerCase(
            "thread2", []() -> std::variant<std::monostate, int, std::string> {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                return "Response from thread 2";
            });

        // Access from multiple threads
        auto thread1 = std::async(std::launch::async, [&safeSwitch]() {
            auto result = safeSwitch.match("thread1");
            std::cout << "Thread 1: ";
            printReturnValue(result);
        });

        auto thread2 = std::async(std::launch::async, [&safeSwitch]() {
            auto result = safeSwitch.match("thread2");
            std::cout << "Thread 2: ";
            printReturnValue(result);
        });

        // Wait for both threads to complete
        thread1.wait();
        thread2.wait();
    }
    std::cout << std::endl;

    std::cout << "Example 5: Case Management\n";
    {
        atom::utils::StringSwitch<false> managedSwitch;

        // Register initial cases
        managedSwitch.registerCase(
            "case1", []() -> std::variant<std::monostate, int, std::string> {
                return "First case";
            });

        managedSwitch.registerCase(
            "case2", []() -> std::variant<std::monostate, int, std::string> {
                return "Second case";
            });

        managedSwitch.registerCase(
            "case3", []() -> std::variant<std::monostate, int, std::string> {
                return "Third case";
            });

        // Get all registered cases
        auto cases = managedSwitch.getCases();
        printCollection(cases, "Initially registered cases");

        // Check case existence
        std::cout << "Has 'case1': "
                  << (managedSwitch.hasCase("case1") ? "Yes" : "No")
                  << std::endl;
        std::cout << "Has 'case4': "
                  << (managedSwitch.hasCase("case4") ? "Yes" : "No")
                  << std::endl;

        // Unregister a case
        bool unregistered = managedSwitch.unregisterCase("case2");
        std::cout << "Unregistered 'case2': " << (unregistered ? "Yes" : "No")
                  << std::endl;

        // Get updated cases
        cases = managedSwitch.getCases();
        printCollection(cases, "Cases after unregistering 'case2'");

        // Clear all cases
        managedSwitch.clearCases();
        std::cout << "After clearCases(): "
                  << (managedSwitch.empty() ? "Empty" : "Not empty")
                  << std::endl;
        std::cout << "Size: " << managedSwitch.size() << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Example 6: StringSwitch with Initializer List\n";
    {
        // Create a StringSwitch using initializer list
        atom::utils::StringSwitch<false> initSwitch{
            {"item1",
             []() -> std::variant<std::monostate, int, std::string> {
                 return "First item";
             }},
            {"item2",
             []() -> std::variant<std::monostate, int, std::string> {
                 return "Second item";
             }},
            {"item3", []() -> std::variant<std::monostate, int, std::string> {
                 return "Third item";
             }}};

        // Get all registered cases
        auto cases = initSwitch.getCases();
        printCollection(cases, "Cases initialized with initializer list");

        // Match a case
        std::cout << "Matching 'item2': ";
        printReturnValue(initSwitch.match("item2"));
    }
    std::cout << std::endl;

    std::cout << "Example 7: Working with Spans\n";
    {
        atom::utils::StringSwitch<false, int, std::string> spanSwitch;

        // Register cases
        spanSwitch.registerCase(
            "format",
            [](int num, const std::string& fmt)
                -> std::variant<std::monostate, int, std::string> {
                if (fmt == "hex") {
                    std::stringstream ss;
                    ss << "0x" << std::hex << num;
                    return ss.str();
                } else if (fmt == "dec") {
                    return std::to_string(num);
                } else {
                    return "Unknown format";
                }
            });

        // Create a span of arguments
        std::vector<std::tuple<int, std::string>> argsList = {
            {42, "hex"}, {255, "dec"}, {123, "unknown"}};

        // Match with the first set of arguments
        std::cout << "Matching 'format' with first argument set: ";
        printReturnValue(
            spanSwitch.matchWithSpan("format", std::span(argsList.data(), 1)));

        // Try with different arguments
        std::cout << "Matching 'format' with second argument set: ";
        printReturnValue(spanSwitch.matchWithSpan(
            "format", std::span(argsList.data() + 1, 1)));

        // Try with unknown format
        std::cout << "Matching 'format' with third argument set: ";
        printReturnValue(spanSwitch.matchWithSpan(
            "format", std::span(argsList.data() + 2, 1)));
    }
    std::cout << std::endl;

    std::cout << "Example 8: Parallel Matching\n";
    {
        atom::utils::StringSwitch<true, int> parallelSwitch;

        // Register cases with simulated work
        parallelSwitch.registerCase(
            "quick",
            [](int x) -> std::variant<std::monostate, int, std::string> {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return x * 2;
            });

        parallelSwitch.registerCase(
            "medium",
            [](int x) -> std::variant<std::monostate, int, std::string> {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                return x * 3;
            });

        parallelSwitch.registerCase(
            "slow",
            [](int x) -> std::variant<std::monostate, int, std::string> {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return x * 4;
            });

        // Set default for unknown cases
        parallelSwitch.setDefault(
            [](int x) -> std::variant<std::monostate, int, std::string> {
                return "Unknown operation with " + std::to_string(x);
            });

        // Create a list of keys to match
        std::vector<std::string> keys = {"quick", "medium", "slow", "unknown"};

        // Run parallel matching
        auto start = std::chrono::high_resolution_clock::now();
        auto results = parallelSwitch.matchParallel(keys, 10);
        auto end = std::chrono::high_resolution_clock::now();

        // Calculate elapsed time
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Parallel matching results (with input 10):" << std::endl;
        for (size_t i = 0; i < keys.size(); ++i) {
            std::cout << "  " << keys[i] << ": ";
            printReturnValue(results[i]);
        }

        std::cout << "Total time for parallel execution: " << duration.count()
                  << "ms" << std::endl;
        std::cout << "(Would be ~160ms if executed sequentially)" << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Example 9: Error Handling\n";
    {
        atom::utils::StringSwitch<false> errorSwitch;

        // Register a case that might throw
        errorSwitch.registerCase(
            "divide", []() -> std::variant<std::monostate, int, std::string> {
                int a = 10;
                int b = 0;
                if (b == 0) {
                    throw std::runtime_error("Division by zero");
                }
                return a / b;
            });

        // Register another case
        errorSwitch.registerCase(
            "valid", []() -> std::variant<std::monostate, int, std::string> {
                return "This is valid";
            });

        // Set default that also throws
        errorSwitch.setDefault(
            []() -> std::variant<std::monostate, int, std::string> {
                throw std::runtime_error("Default handler error");
            });

        // Try to register duplicate case
        try {
            std::cout << "Attempting to register duplicate case: ";
            errorSwitch.registerCase(
                "valid",
                []() -> std::variant<std::monostate, int, std::string> {
                    return "Duplicate";
                });
            std::cout << "Success (shouldn't happen)" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Error caught: " << e.what() << std::endl;
        }

        // Try to match a case that throws
        std::cout << "Matching 'divide' (will throw): ";
        printReturnValue(errorSwitch.match("divide"));

        // Try to match unknown case with throwing default
        std::cout << "Matching 'unknown' (default will throw): ";
        printReturnValue(errorSwitch.match("unknown"));
    }
    std::cout << std::endl;

    std::cout << "Example 10: StringSwitch with Different Key Types\n";
    {
        atom::utils::StringSwitch<false> keySwitch;

        // Register cases with different key types
        keySwitch.registerCase(
            std::string("string"),
            []() -> std::variant<std::monostate, int, std::string> {
                return "Registered with std::string";
            });

        keySwitch.registerCase(
            "literal", []() -> std::variant<std::monostate, int, std::string> {
                return "Registered with string literal";
            });

        // Create a string_view
        std::string_view viewKey = "view";
        keySwitch.registerCase(
            viewKey, []() -> std::variant<std::monostate, int, std::string> {
                return "Registered with string_view";
            });

        // Match with different key types
        std::cout << "Matching with std::string: ";
        printReturnValue(keySwitch.match(std::string("string")));

        std::cout << "Matching with string literal: ";
        printReturnValue(keySwitch.match("literal"));

        std::cout << "Matching with string_view: ";
        std::string_view matchView = "view";
        printReturnValue(keySwitch.match(matchView));
    }

    return 0;
}
