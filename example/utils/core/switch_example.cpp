/**
 * @file switch_example.cpp
 * @brief Examples for atom::utils StringSwitch
 */

#include <iostream>
#include <string>
#include <vector>
#include "atom/utils/core/switch.hpp"

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void demonstrateBasicUsage() {
    printSection("1. Basic StringSwitch Usage");

    StringSwitch<false> sw;

    sw.registerCase("hello", []() -> StringSwitch<false>::ReturnType {
        return String("Hello, World!");
    });

    sw.registerCase("goodbye", []() -> StringSwitch<false>::ReturnType {
        return String("Goodbye!");
    });

    sw.registerCase("count",
                    []() -> StringSwitch<false>::ReturnType { return 42; });

    sw.setDefault([]() -> StringSwitch<false>::ReturnType {
        return String("Unknown command");
    });

    std::vector<std::string> commands = {"hello", "goodbye", "count",
                                         "unknown"};
    for (const auto& cmd : commands) {
        std::cout << "  match(\"" << cmd << "\"): ";
        auto result = sw.match(cmd);
        std::visit(
            [](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    std::cout << "(no result)";
                } else if constexpr (std::is_same_v<T, int>) {
                    std::cout << "int: " << arg;
                } else if constexpr (std::is_same_v<T, String>) {
                    std::cout << "string: \"" << arg << "\"";
                }
            },
            result);
        std::cout << std::endl;
    }
}

void demonstrateCaseManagement() {
    printSection("2. Case Management");

    StringSwitch<false> sw;

    sw.registerCase("add",
                    []() -> StringSwitch<false>::ReturnType { return 1; });
    sw.registerCase("sub",
                    []() -> StringSwitch<false>::ReturnType { return 2; });
    sw.registerCase("mul",
                    []() -> StringSwitch<false>::ReturnType { return 3; });

    std::cout << "Registered cases: add, sub, mul" << std::endl;
    std::cout << "Has 'add': " << (sw.hasCase("add") ? "Yes" : "No")
              << std::endl;
    std::cout << "Has 'div': " << (sw.hasCase("div") ? "Yes" : "No")
              << std::endl;

    sw.unregisterCase("sub");
    std::cout << "\nAfter unregistering 'sub':" << std::endl;
    std::cout << "Has 'sub': " << (sw.hasCase("sub") ? "Yes" : "No")
              << std::endl;

    sw.clearCases();
    std::cout << "\nAfter clearing all cases:" << std::endl;
    std::cout << "Has 'add': " << (sw.hasCase("add") ? "Yes" : "No")
              << std::endl;
}

void demonstrateStatistics() {
    printSection("3. Performance Statistics");

    StringSwitch<false> sw;

    sw.registerCase("op1",
                    []() -> StringSwitch<false>::ReturnType { return 1; });
    sw.registerCase("op2",
                    []() -> StringSwitch<false>::ReturnType { return 2; });

    for (int i = 0; i < 100; ++i) {
        sw.match("op1");
        sw.match("op2");
        sw.match("unknown");
    }

    auto stats = sw.getStats().getSnapshot();
    std::cout << "Statistics after 300 calls:" << std::endl;
    std::cout << "  Total calls: " << stats.totalCalls << std::endl;
    std::cout << "  Cache hits: " << stats.cacheHits << std::endl;
    std::cout << "  Cache misses: " << stats.cacheMisses << std::endl;
    std::cout << "  Hit ratio: " << (stats.hitRatio * 100) << "%" << std::endl;
}

void demonstrateThreadSafe() {
    printSection("4. Thread-Safe StringSwitch");

    StringSwitch<true> sw;

    sw.registerCase("task", []() -> StringSwitch<true>::ReturnType {
        return String("Task executed");
    });

    std::cout << "Thread-safe switch created" << std::endl;
    std::cout << "Multiple threads can safely access this switch" << std::endl;

    auto result = sw.match("task");
    std::visit(
        [](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, String>) {
                std::cout << "Result: " << arg << std::endl;
            }
        },
        result);
}

void demonstrateCommandProcessor() {
    printSection("5. Command Processor Example");

    StringSwitch<false> processor;

    processor.registerCase("help", []() -> StringSwitch<false>::ReturnType {
        return String("Available commands: help, version, quit");
    });

    processor.registerCase("version", []() -> StringSwitch<false>::ReturnType {
        return String("Version 1.0.0");
    });

    processor.registerCase(
        "quit", []() -> StringSwitch<false>::ReturnType { return 0; });

    processor.setDefault([]() -> StringSwitch<false>::ReturnType {
        return String("Unknown command. Type 'help' for available commands.");
    });

    std::vector<std::string> inputs = {"help", "version", "invalid", "quit"};
    for (const auto& input : inputs) {
        std::cout << "> " << input << std::endl;
        auto result = processor.match(input);
        std::visit(
            [](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, int>) {
                    std::cout << "  Exit code: " << arg << std::endl;
                } else if constexpr (std::is_same_v<T, String>) {
                    std::cout << "  " << arg << std::endl;
                }
            },
            result);
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  StringSwitch Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateBasicUsage();
        demonstrateCaseManagement();
        demonstrateStatistics();
        demonstrateThreadSafe();
        demonstrateCommandProcessor();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All StringSwitch examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
