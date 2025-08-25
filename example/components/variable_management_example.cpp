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
#include "atom/components/registry.hpp"
#include "atom/components/var.hpp"

using namespace atom::components;

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

        // Variables with constraints (using the variable manager)
        auto& varManager = getVariableManager();

        // Add range-constrained variables
        varManager.addVariable<int>("constrained_int", 50,
                                    "Integer with range [0, 100]");
        varManager.setRange<int>("constrained_int", 0, 100);

        varManager.addVariable<double>("constrained_double", 0.5,
                                       "Double with range [0.0, 1.0]");
        varManager.setRange<double>("constrained_double", 0.0, 1.0);

        // String with options
        varManager.addVariable<std::string>("enum_string", "option1",
                                            "String with predefined options");
        std::vector<std::string> options = {"option1", "option2", "option3"};
        varManager.setStringOptions("enum_string", options);
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

            auto& varManager = getVariableManager();

            // Test valid range values
            std::cout << "Setting constrained_int to 75 (valid)..."
                      << std::endl;
            try {
                varManager.setValue("constrained_int", 75);
                std::cout << "Success! Value set to: "
                          << varManager.getValue<int>("constrained_int")
                          << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << std::endl;
            }

            // Test invalid range values
            std::cout << "Setting constrained_int to 150 (invalid)..."
                      << std::endl;
            try {
                varManager.setValue("constrained_int", 150);
                std::cout << "Unexpected success!" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Expected error: " << e.what() << std::endl;
            }

            // Test string options
            std::cout << "Setting enum_string to 'option2' (valid)..."
                      << std::endl;
            try {
                varManager.setValue("enum_string", std::string("option2"));
                std::cout << "Success! Value set to: "
                          << varManager.getValue<std::string>("enum_string")
                          << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << std::endl;
            }

            std::cout << "Setting enum_string to 'invalid_option' (invalid)..."
                      << std::endl;
            try {
                varManager.setValue("enum_string",
                                    std::string("invalid_option"));
                std::cout << "Unexpected success!" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Expected error: " << e.what() << std::endl;
            }
        });

        def("demonstrateTracking", [this]() {
            std::cout << "\nDemonstrating variable tracking..." << std::endl;

            auto& varManager = getVariableManager();

            // Get variable info
            auto info = varManager.getVariableInfo("integer_value");
            if (info) {
                std::cout << "Variable 'integer_value' info:" << std::endl;
                std::cout << "  Description: " << info->description
                          << std::endl;
                std::cout << "  Group: " << info->group << std::endl;
                std::cout << "  Type: " << info->typeName << std::endl;
            }

            // List all variables
            auto allVars = varManager.getAllVariableNames();
            std::cout << "\nAll registered variables:" << std::endl;
            for (const auto& varName : allVars) {
                std::cout << "  - " << varName << std::endl;
            }
        });

        def("serializeVariables", [this]() {
            std::cout << "\nSerializing variables to JSON..." << std::endl;

            auto& varManager = getVariableManager();

            try {
                auto json = varManager.toJson();
                std::cout << "Serialized JSON:" << std::endl;
                std::cout << json.dump(2) << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Serialization error: " << e.what() << std::endl;
            }
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
    component->executeCommand("printAllVariables", {});

    std::cout << "\n2. Modifying variables:" << std::endl;
    component->executeCommand("modifyVariables", {});
    component->executeCommand("printAllVariables", {});
}

void demonstrateConstraints() {
    std::cout << "\n=== Variable Constraints Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("VarComponent");

    if (component) {
        component->executeCommand("testConstraints", {});
    }
}

void demonstrateTracking() {
    std::cout << "\n=== Variable Tracking Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("VarComponent");

    if (component) {
        component->executeCommand("demonstrateTracking", {});
    }
}

void demonstrateSerialization() {
    std::cout << "\n=== Variable Serialization Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("VarComponent");

    if (component) {
        component->executeCommand("serializeVariables", {});
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
