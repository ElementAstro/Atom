/*
 * command_dispatch_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Command Dispatch System Example
Demonstrates command registration, execution, parameter handling,
error management, preconditions, postconditions, and advanced dispatch features.

**************************************************/

#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/dispatch.hpp"
#include "atom/components/registry.hpp"
#include "atom/meta/type_caster.hpp"

using namespace atom::components;

/**
 * @brief Component demonstrating comprehensive command dispatch
 */
class CommandComponent : public Component {
public:
    explicit CommandComponent(const std::string& name) : Component(name) {
        std::cout << "Creating CommandComponent: " << name << std::endl;
        setupCommands();
    }

private:
    int counter_ = 0;
    std::string status_ = "ready";

    void setupCommands() {
        // Basic commands with different signatures
        def(
            "hello",
            []() -> std::string { return "Hello from command system!"; },
            "basic", "Simple greeting command");

        def(
            "add", [](int a, int b) -> int { return a + b; }, "math",
            "Add two integers");

        def(
            "multiply", [](double a, double b) -> double { return a * b; },
            "math", "Multiply two doubles");

        def(
            "concatenate",
            [](const std::string& a, const std::string& b) -> std::string {
                return a + " " + b;
            },
            "string", "Concatenate two strings");

        // Commands with member access
        def(
            "increment", [this]() -> int { return ++counter_; }, "counter",
            "Increment internal counter");

        def(
            "decrement", [this]() -> int { return --counter_; }, "counter",
            "Decrement internal counter");

        def(
            "getCounter", [this]() -> int { return counter_; }, "counter",
            "Get current counter value");

        def(
            "setCounter", [this](int value) { counter_ = value; }, "counter",
            "Set counter to specific value");

        // Commands with complex parameters
        def(
            "processVector",
            [](const std::vector<int>& vec) -> int {
                int sum = 0;
                for (int val : vec) {
                    sum += val;
                }
                return sum;
            },
            "vector", "Sum all elements in vector");

        def(
            "createVector",
            [](int size, int value) -> std::vector<int> {
                return std::vector<int>(size, value);
            },
            "vector", "Create vector of given size with default value");

        // Commands with status management
        def(
            "setStatus",
            [this](const std::string& newStatus) {
                std::cout << "Status changing from '" << status_ << "' to '"
                          << newStatus << "'" << std::endl;
                status_ = newStatus;
            },
            "status", "Set component status");

        def(
            "getStatus", [this]() -> std::string { return status_; }, "status",
            "Get current component status");

        // Command with precondition
        def(
            "protectedOperation",
            [this]() -> std::string {
                return "Protected operation executed successfully!";
            },
            "security", "Operation that requires ready status",
            // Precondition: only execute if status is "ready"
            [this]() -> bool {
                bool canExecute = (status_ == "ready");
                if (!canExecute) {
                    std::cout << "Precondition failed: status is '" << status_
                              << "', expected 'ready'" << std::endl;
                }
                return canExecute;
            },
            // Postcondition: set status to "busy" after execution
            [this]() {
                std::cout << "Postcondition: setting status to 'busy'"
                          << std::endl;
                status_ = "busy";
            });

        // Command with error handling
        def(
            "riskyOperation",
            [](int value) -> std::string {
                if (value < 0) {
                    throw std::invalid_argument("Value cannot be negative");
                }
                if (value > 100) {
                    throw std::out_of_range("Value cannot exceed 100");
                }
                return "Operation completed with value: " +
                       std::to_string(value);
            },
            "error", "Operation that may throw exceptions");

        // Timing command
        def(
            "slowOperation",
            [](int milliseconds) -> std::string {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(milliseconds));
                return "Slow operation completed after " +
                       std::to_string(milliseconds) + "ms";
            },
            "timing", "Operation that takes specified time");
    }
};

void demonstrateBasicCommands() {
    std::cout << "\n=== Basic Command Execution Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.createComponent<CommandComponent>("CmdComponent");

    std::cout << "\n1. Simple commands:" << std::endl;

    // Execute simple commands
    auto result1 = component->executeCommand("hello", {});
    std::cout << "hello() -> " << result1 << std::endl;

    auto result2 = component->executeCommand("add", {"10", "20"});
    std::cout << "add(10, 20) -> " << result2 << std::endl;

    auto result3 = component->executeCommand("multiply", {"3.14", "2.0"});
    std::cout << "multiply(3.14, 2.0) -> " << result3 << std::endl;

    auto result4 = component->executeCommand("concatenate", {"Hello", "World"});
    std::cout << "concatenate('Hello', 'World') -> " << result4 << std::endl;
}

void demonstrateStatefulCommands() {
    std::cout << "\n=== Stateful Command Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("CmdComponent");

    if (!component) {
        std::cout << "Component not found!" << std::endl;
        return;
    }

    std::cout << "\n2. Stateful commands:" << std::endl;

    // Test counter commands
    auto counter1 = component->executeCommand("getCounter", {});
    std::cout << "Initial counter: " << counter1 << std::endl;

    auto counter2 = component->executeCommand("increment", {});
    std::cout << "After increment: " << counter2 << std::endl;

    auto counter3 = component->executeCommand("increment", {});
    std::cout << "After increment: " << counter3 << std::endl;

    component->executeCommand("setCounter", {"100"});
    auto counter4 = component->executeCommand("getCounter", {});
    std::cout << "After setCounter(100): " << counter4 << std::endl;

    auto counter5 = component->executeCommand("decrement", {});
    std::cout << "After decrement: " << counter5 << std::endl;
}

void demonstrateComplexParameters() {
    std::cout << "\n=== Complex Parameter Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("CmdComponent");

    if (!component) {
        std::cout << "Component not found!" << std::endl;
        return;
    }

    std::cout << "\n3. Complex parameter handling:" << std::endl;

    // Note: For vector parameters, we'd typically need JSON or custom
    // serialization This is a simplified demonstration
    std::cout << "Creating vector with createVector(5, 42)..." << std::endl;
    auto vectorResult = component->executeCommand("createVector", {"5", "42"});
    std::cout << "Vector creation result: " << vectorResult << std::endl;

    // For actual vector processing, we'd need proper type conversion
    std::cout << "Note: Vector processing requires proper type conversion "
                 "implementation"
              << std::endl;
}

void demonstratePreconditionsPostconditions() {
    std::cout << "\n=== Preconditions and Postconditions Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("CmdComponent");

    if (!component) {
        std::cout << "Component not found!" << std::endl;
        return;
    }

    std::cout << "\n4. Preconditions and postconditions:" << std::endl;

    // Check initial status
    auto status1 = component->executeCommand("getStatus", {});
    std::cout << "Initial status: " << status1 << std::endl;

    // Execute protected operation (should work)
    std::cout << "\nExecuting protectedOperation (should succeed)..."
              << std::endl;
    auto result1 = component->executeCommand("protectedOperation", {});
    std::cout << "Result: " << result1 << std::endl;

    // Check status after postcondition
    auto status2 = component->executeCommand("getStatus", {});
    std::cout << "Status after operation: " << status2 << std::endl;

    // Try to execute again (should fail precondition)
    std::cout << "\nExecuting protectedOperation again (should fail)..."
              << std::endl;
    try {
        auto result2 = component->executeCommand("protectedOperation", {});
        std::cout << "Unexpected success: " << result2 << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected failure: " << e.what() << std::endl;
    }

    // Reset status and try again
    std::cout << "\nResetting status to 'ready'..." << std::endl;
    component->executeCommand("setStatus", {"ready"});
    auto result3 = component->executeCommand("protectedOperation", {});
    std::cout << "Result after reset: " << result3 << std::endl;
}

void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("CmdComponent");

    if (!component) {
        std::cout << "Component not found!" << std::endl;
        return;
    }

    std::cout << "\n5. Error handling:" << std::endl;

    // Test valid operation
    std::cout << "Testing riskyOperation(50) - should succeed..." << std::endl;
    try {
        auto result1 = component->executeCommand("riskyOperation", {"50"});
        std::cout << "Success: " << result1 << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Unexpected error: " << e.what() << std::endl;
    }

    // Test invalid operation (negative value)
    std::cout << "\nTesting riskyOperation(-10) - should fail..." << std::endl;
    try {
        auto result2 = component->executeCommand("riskyOperation", {"-10"});
        std::cout << "Unexpected success: " << result2 << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected error: " << e.what() << std::endl;
    }

    // Test invalid operation (too large value)
    std::cout << "\nTesting riskyOperation(150) - should fail..." << std::endl;
    try {
        auto result3 = component->executeCommand("riskyOperation", {"150"});
        std::cout << "Unexpected success: " << result3 << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected error: " << e.what() << std::endl;
    }

    // Test nonexistent command
    std::cout << "\nTesting nonexistent command - should fail..." << std::endl;
    try {
        auto result4 = component->executeCommand("nonexistentCommand", {});
        std::cout << "Unexpected success: " << result4 << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected error: " << e.what() << std::endl;
    }
}

void demonstratePerformance() {
    std::cout << "\n=== Performance Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("CmdComponent");

    if (!component) {
        std::cout << "Component not found!" << std::endl;
        return;
    }

    std::cout << "\n6. Performance measurement:" << std::endl;

    // Test timing
    auto start = std::chrono::high_resolution_clock::now();
    auto result = component->executeCommand("slowOperation", {"100"});
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Result: " << result << std::endl;
    std::cout << "Actual execution time: " << duration.count() << "ms"
              << std::endl;

    // Test rapid command execution
    std::cout << "\nTesting rapid command execution..." << std::endl;
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        component->executeCommand("increment", {});
    }
    end = std::chrono::high_resolution_clock::now();

    auto rapidDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "1000 increment commands executed in: "
              << rapidDuration.count() << " microseconds" << std::endl;
    std::cout << "Average per command: " << (rapidDuration.count() / 1000.0)
              << " microseconds" << std::endl;
}

int main() {
    std::cout << "=== Atom Component Command Dispatch Examples ==="
              << std::endl;

    try {
        demonstrateBasicCommands();
        demonstrateStatefulCommands();
        demonstrateComplexParameters();
        demonstratePreconditionsPostconditions();
        demonstrateErrorHandling();
        demonstratePerformance();

        std::cout
            << "\n=== All Command Dispatch Examples Completed Successfully! ==="
            << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in command dispatch examples: " << e.what()
                  << std::endl;
        return 1;
    }

    return 0;
}
