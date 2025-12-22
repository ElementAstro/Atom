/*
 * command_dispatch_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Command Dispatch System ExampleDemonstrates command registration,
execution, parameter handling, error management, preconditions, postconditions,
and advanced dispatch features.

**************************************************/

#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/core/registry.hpp"
#include "atom/components/dispatch.hpp"
#include "atom/meta/type_caster.hpp"

// Note: Registry and Component are in the global namespace, not
// atom::components

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

        // Command with precondition (simplified - precondition logic moved
        // inside)
        def(
            "protectedOperation",
            [this]() -> std::string {
                // Check precondition inside the command
                if (status_ != "ready") {
                    std::cout << "Precondition failed: status is '" << status_
                              << "', expected 'ready'" << std::endl;
                    throw std::runtime_error(
                        "Component not ready for protected operation");
                }

                auto result = "Protected operation executed successfully!";

                // Postcondition: set status to "busy" after execution
                std::cout << "Postcondition: setting status to 'busy'"
                          << std::endl;
                status_ = "busy";

                return result;
            },
            "security", "Operation that requires ready status");

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
    auto result1 = component->runCommand("hello", {});
    std::cout << "hello() -> " << std::any_cast<std::string>(result1)
              << std::endl;

    std::vector<std::any> addArgs = {std::any(std::string("10")),
                                     std::any(std::string("20"))};
    auto result2 = component->runCommand("add", addArgs);
    std::cout << "add(10, 20) -> " << std::any_cast<int>(result2) << std::endl;

    std::vector<std::any> multiplyArgs = {std::any(std::string("3.14")),
                                          std::any(std::string("2.0"))};
    auto result3 = component->runCommand("multiply", multiplyArgs);
    std::cout << "multiply(3.14, 2.0) -> " << std::any_cast<double>(result3)
              << std::endl;

    std::vector<std::any> concatArgs = {std::any(std::string("Hello")),
                                        std::any(std::string("World"))};
    auto result4 = component->runCommand("concatenate", concatArgs);
    std::cout << "concatenate('Hello', 'World') -> "
              << std::any_cast<std::string>(result4) << std::endl;
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
    auto counter1 = component->runCommand("getCounter", {});
    std::cout << "Initial counter: " << std::any_cast<int>(counter1)
              << std::endl;

    auto counter2 = component->runCommand("increment", {});
    std::cout << "After increment: " << std::any_cast<int>(counter2)
              << std::endl;

    auto counter3 = component->runCommand("increment", {});
    std::cout << "After increment: " << std::any_cast<int>(counter3)
              << std::endl;

    std::vector<std::any> setArgs = {std::any(std::string("100"))};
    [[maybe_unused]] auto setResult =
        component->runCommand("setCounter", setArgs);
    auto counter4 = component->runCommand("getCounter", {});
    std::cout << "After setCounter(100): " << std::any_cast<int>(counter4)
              << std::endl;

    auto counter5 = component->runCommand("decrement", {});
    std::cout << "After decrement: " << std::any_cast<int>(counter5)
              << std::endl;
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
    std::vector<std::any> vectorArgs = {std::any(std::string("5")),
                                        std::any(std::string("42"))};
    auto vectorResult = component->runCommand("createVector", vectorArgs);
    std::cout << "Vector creation result: "
              << std::any_cast<std::string>(vectorResult) << std::endl;

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
    auto status1 = component->runCommand("getStatus", {});
    std::cout << "Initial status: " << std::any_cast<std::string>(status1)
              << std::endl;

    // Execute protected operation (should work)
    std::cout << "\nExecuting protectedOperation (should succeed)..."
              << std::endl;
    auto result1 = component->runCommand("protectedOperation", {});
    std::cout << "Result: " << std::any_cast<std::string>(result1) << std::endl;

    // Check status after postcondition
    auto status2 = component->runCommand("getStatus", {});
    std::cout << "Status after operation: "
              << std::any_cast<std::string>(status2) << std::endl;

    // Try to execute again (should fail precondition)
    std::cout << "\nExecuting protectedOperation again (should fail)..."
              << std::endl;
    try {
        auto result2 = component->runCommand("protectedOperation", {});
        std::cout << "Unexpected success: "
                  << std::any_cast<std::string>(result2) << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected failure: " << e.what() << std::endl;
    }

    // Reset status and try again
    std::cout << "\nResetting status to 'ready'..." << std::endl;
    std::vector<std::any> statusArgs = {std::any(std::string("ready"))};
    [[maybe_unused]] auto statusResult =
        component->runCommand("setStatus", statusArgs);
    auto result3 = component->runCommand("protectedOperation", {});
    std::cout << "Result after reset: " << std::any_cast<std::string>(result3)
              << std::endl;
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
        std::vector<std::any> args1 = {std::any(std::string("50"))};
        auto result1 = component->runCommand("riskyOperation", args1);
        std::cout << "Success: " << std::any_cast<std::string>(result1)
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Unexpected error: " << e.what() << std::endl;
    }

    // Test invalid operation (negative value)
    std::cout << "\nTesting riskyOperation(-10) - should fail..." << std::endl;
    try {
        std::vector<std::any> args2 = {std::any(std::string("-10"))};
        auto result2 = component->runCommand("riskyOperation", args2);
        std::cout << "Unexpected success: "
                  << std::any_cast<std::string>(result2) << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected error: " << e.what() << std::endl;
    }

    // Test invalid operation (too large value)
    std::cout << "\nTesting riskyOperation(150) - should fail..." << std::endl;
    try {
        std::vector<std::any> args3 = {std::any(std::string("150"))};
        auto result3 = component->runCommand("riskyOperation", args3);
        std::cout << "Unexpected success: "
                  << std::any_cast<std::string>(result3) << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected error: " << e.what() << std::endl;
    }

    // Test nonexistent command
    std::cout << "\nTesting nonexistent command - should fail..." << std::endl;
    try {
        auto result4 = component->runCommand("nonexistentCommand", {});
        std::cout << "Unexpected success: "
                  << std::any_cast<std::string>(result4) << std::endl;
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
    std::vector<std::any> slowArgs = {std::any(std::string("100"))};
    auto result = component->runCommand("slowOperation", slowArgs);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Result: " << std::any_cast<std::string>(result) << std::endl;
    std::cout << "Actual execution time: " << duration.count() << "ms"
              << std::endl;

    // Test rapid command execution
    std::cout << "\nTesting rapid command execution..." << std::endl;
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        [[maybe_unused]] auto incrementResult =
            component->runCommand("increment", {});
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
