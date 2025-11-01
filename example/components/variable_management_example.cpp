/*
 * variable_management_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Variable Management Comprehensive Example
Demonstrates the variable system with different types, constraints,
validation, tracking, serialization, and advanced features.

**************************************************/

#include <any>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/core/registry.hpp"
#include "atom/components/data/var.hpp"

// Note: Registry and Component are in the global namespace

/**
 * @brief Component demonstrating comprehensive variable management
 */
class VariableComponent : public Component {
public:
    explicit VariableComponent(const std::string& name) : Component(name) {
        std::cout << "Creating VariableComponent: " << name << std::endl;
        setupVariables();
        setupCommands();
    }

private:
    void setupVariables() {
        // Basic types
        addVariable<int>("integer_value", 42, "An integer variable");
        addVariable<double>("double_value", 3.14159,
                            "A double precision variable");
        addVariable<std::string>("string_value", "Hello World",
                                 "A string variable");
        addVariable<bool>("boolean_value", true, "A boolean variable");

        // Container types
        addVariable<std::vector<int>>("int_vector", {1, 2, 3, 4, 5},
                                      "Vector of integers");
        addVariable<std::vector<std::string>>("string_vector",
                                              {"apple", "banana", "cherry"},
                                              "Vector of strings");

        // Complex types
        struct Point {
            double x, y, z;
        };
        // Note: For complex types, we'd typically use JSON or custom
        // serialization
        addVariable<std::string>(
            "point_json", "{\"x\":1.0,\"y\":2.0,\"z\":3.0}", "Point as JSON");

        // Variables with constraints
        addVariable<int>("constrained_int", 50, "Integer with range [0, 100]");
        setRange<int>("constrained_int", 0, 100);

        addVariable<double>("constrained_double", 0.5,
                            "Double with range [0.0, 1.0]");
        setRange<double>("constrained_double", 0.0, 1.0);

        // String with predefined options
        addVariable<std::string>("enum_string", "option1",
                                 "String with predefined options");
        std::vector<std::string> stringOptions = {"option1", "option2",
                                                  "option3", "option4"};
        setStringOptions("enum_string", stringOptions);

        // Additional constrained variables for demonstration
        addVariable<int>("percentage", 50, "Percentage value [0-100]");
        setRange<int>("percentage", 0, 100);

        addVariable<double>("temperature", 20.0,
                            "Temperature in Celsius [-273.15, 1000.0]");
        setRange<double>("temperature", -273.15, 1000.0);

        addVariable<std::string>("log_level", "INFO", "Logging level");
        std::vector<std::string> logLevels = {"DEBUG", "INFO", "WARN", "ERROR",
                                              "FATAL"};
        setStringOptions("log_level", logLevels);
    }

    void setupCommands() {
        // Commands for variable manipulation
        def("printAllVariables", [this]() {
            std::cout << "\n--- All Variables ---" << std::endl;

            // Print basic variables
            if (auto var = getVariable<int>("integer_value")) {
                std::cout << "integer_value: " << var->get() << std::endl;
            }
            if (auto var = getVariable<double>("double_value")) {
                std::cout << "double_value: " << var->get() << std::endl;
            }
            if (auto var = getVariable<std::string>("string_value")) {
                std::cout << "string_value: " << var->get() << std::endl;
            }
            if (auto var = getVariable<bool>("boolean_value")) {
                std::cout << "boolean_value: "
                          << (var->get() ? "true" : "false") << std::endl;
            }

            // Print vector variables
            if (auto var = getVariable<std::vector<int>>("int_vector")) {
                std::cout << "int_vector: [";
                const auto& vec = var->get();
                for (size_t i = 0; i < vec.size(); ++i) {
                    std::cout << vec[i];
                    if (i < vec.size() - 1)
                        std::cout << ", ";
                }
                std::cout << "]" << std::endl;
            }

            if (auto var =
                    getVariable<std::vector<std::string>>("string_vector")) {
                std::cout << "string_vector: [";
                const auto& vec = var->get();
                for (size_t i = 0; i < vec.size(); ++i) {
                    std::cout << "\"" << vec[i] << "\"";
                    if (i < vec.size() - 1)
                        std::cout << ", ";
                }
                std::cout << "]" << std::endl;
            }

            std::cout << "------------------------" << std::endl;
        });

        def("modifyVariables", [this]() {
            std::cout << "\nModifying variables..." << std::endl;

            // Modify basic variables
            setValue("integer_value", 100);
            setValue("double_value", 2.71828);
            setValue("string_value", std::string("Modified String"));
            setValue("boolean_value", false);

            // Modify vector variables
            setValue("int_vector", std::vector<int>{10, 20, 30});
            setValue("string_vector",
                     std::vector<std::string>{"red", "green", "blue"});

            std::cout << "Variables modified successfully!" << std::endl;
        });

        def("testConstraints", [this]() {
            std::cout << "\nTesting variable constraints..." << std::endl;

            // Test valid range values
            std::cout << "Setting constrained_int to 75 (valid)..."
                      << std::endl;
            try {
                setValue("constrained_int", 75);
                auto var = getVariable<int>("constrained_int");
                std::cout << "Success! Value set to: " << (var ? var->get() : 0)
                          << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << std::endl;
            }

            // Test invalid range values
            std::cout << "Setting constrained_int to 150 (invalid)..."
                      << std::endl;
            try {
                setValue("constrained_int", 150);
                auto var = getVariable<int>("constrained_int");
                std::cout << "Unexpected success! Value set to: "
                          << (var ? var->get() : 0) << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Expected error: " << e.what() << std::endl;
            }

            // Test double range constraints
            std::cout << "Setting constrained_double to 0.8 (valid)..."
                      << std::endl;
            try {
                setValue("constrained_double", 0.8);
                auto var = getVariable<double>("constrained_double");
                std::cout << "Success! Value set to: "
                          << (var ? var->get() : 0.0) << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << std::endl;
            }

            std::cout << "Setting constrained_double to 1.5 (invalid)..."
                      << std::endl;
            try {
                setValue("constrained_double", 1.5);
                auto var = getVariable<double>("constrained_double");
                std::cout << "Unexpected success! Value set to: "
                          << (var ? var->get() : 0.0) << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Expected error: " << e.what() << std::endl;
            }

            // Test string options
            std::cout << "Setting enum_string to 'option2' (valid)..."
                      << std::endl;
            try {
                setValue("enum_string", std::string("option2"));
                auto var = getVariable<std::string>("enum_string");
                std::cout << "Success! Value set to: "
                          << (var ? var->get() : "unknown") << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << std::endl;
            }

            std::cout << "Setting enum_string to 'invalid_option' (invalid)..."
                      << std::endl;
            try {
                setValue("enum_string", std::string("invalid_option"));
                auto var = getVariable<std::string>("enum_string");
                std::cout << "Unexpected success! Value set to: "
                          << (var ? var->get() : "unknown") << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Expected error: " << e.what() << std::endl;
            }

            // Test percentage constraints
            std::cout << "Setting percentage to 85 (valid)..." << std::endl;
            try {
                setValue("percentage", 85);
                auto var = getVariable<int>("percentage");
                std::cout << "Success! Value set to: " << (var ? var->get() : 0)
                          << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << std::endl;
            }

            std::cout << "Setting percentage to 150 (invalid)..." << std::endl;
            try {
                setValue("percentage", 150);
                auto var = getVariable<int>("percentage");
                std::cout << "Unexpected success! Value set to: "
                          << (var ? var->get() : 0) << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Expected error: " << e.what() << std::endl;
            }
        });

        def("demonstrateTracking", []() {
            std::cout << "\nDemonstrating variable tracking..." << std::endl;

            // Note: getVariableManager() method doesn't exist in Component base
            // class Variable tracking not available in current implementation
            std::cout
                << "Variable tracking not available in current implementation"
                << std::endl;
        });

        def("serializeVariables", []() {
            std::cout << "\nSerializing variables to JSON..." << std::endl;

            // Note: getVariableManager() method doesn't exist in Component base
            // class Variable serialization not available in current
            // implementation
            std::cout << "Variable serialization not available in current "
                         "implementation"
                      << std::endl;
        });
    }
};

void demonstrateBasicVariables() {
    std::cout << "\n=== Basic Variable Management Demo ===" << std::endl;

    auto& registry = Registry::instance();

    // Create component with variables
    auto component =
        registry.createComponent<VariableComponent>("VarComponent");

    std::cout << "\n1. Initial variable state:" << std::endl;
    [[maybe_unused]] auto printResult =
        component->runCommand("printAllVariables", {});

    std::cout << "\n2. Modifying variables:" << std::endl;
    [[maybe_unused]] auto modifyResult =
        component->runCommand("modifyVariables", {});

    std::cout << "\n3. Printing variables after modification:" << std::endl;
    [[maybe_unused]] auto printResult2 =
        component->runCommand("printAllVariables", {});
}

void demonstrateConstraints() {
    std::cout << "\n=== Variable Constraints Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("VarComponent");

    if (component) {
        [[maybe_unused]] auto constraintResult =
            component->runCommand("testConstraints", {});
        std::cout << "Constraint testing completed successfully!" << std::endl;
    } else {
        std::cout << "Component not found!" << std::endl;
    }
}

void demonstrateTracking() {
    std::cout << "\n=== Variable Tracking Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("VarComponent");

    if (component) {
        [[maybe_unused]] auto trackingResult =
            component->runCommand("demonstrateTracking", {});
        std::cout << "Variable tracking demonstration completed!" << std::endl;
    } else {
        std::cout << "Component not found!" << std::endl;
    }
}

void demonstrateSerialization() {
    std::cout << "\n=== Variable Serialization Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("VarComponent");

    if (component) {
        [[maybe_unused]] auto serializationResult =
            component->runCommand("serializeVariables", {});
        std::cout << "Variable serialization demonstration completed!"
                  << std::endl;
    } else {
        std::cout << "Component not found!" << std::endl;
    }
}

int main() {
    std::cout << "=== Atom Component Variable Management Examples ==="
              << std::endl;

    try {
        demonstrateBasicVariables();
        demonstrateConstraints();
        demonstrateTracking();
        demonstrateSerialization();

        std::cout << "\n=== All Variable Management Examples Completed "
                     "Successfully! ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in variable management examples: " << e.what()
                  << std::endl;
        return 1;
    }

    return 0;
}
