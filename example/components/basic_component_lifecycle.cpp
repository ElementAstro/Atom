/*
 * basic_component_lifecycle.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Basic Component Lifecycle Example
Demonstrates component creation, state management, initialization,
activation, deactivation, and destruction with proper error handling.

**************************************************/

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "atom/components/component.hpp"
#include "atom/components/lifecycle.hpp"
#include "atom/components/registry.hpp"

using namespace atom::components;

/**
 * @brief Custom component demonstrating lifecycle management
 */
class LifecycleComponent : public Component {
public:
    explicit LifecycleComponent(const std::string& name) : Component(name) {
        std::cout << "  [CONSTRUCTOR] Component '" << name << "' created"
                  << std::endl;

        // Add some variables to demonstrate state
        addVariable<int>("counter", 0);
        addVariable<std::string>("status", "created");
        addVariable<bool>("initialized", false);

        // Add commands for lifecycle operations
        def("increment", [this]() {
            auto counter = getVariable<int>("counter");
            if (counter) {
                setValue("counter", counter->get() + 1);
                std::cout << "    Counter incremented to: "
                          << counter->get() + 1 << std::endl;
            }
        });

        def("reset", [this]() {
            setValue("counter", 0);
            std::cout << "    Counter reset to 0" << std::endl;
        });

        def("getStatus", [this]() -> std::string {
            auto status = getVariable<std::string>("status");
            return status ? status->get() : "unknown";
        });
    }

    ~LifecycleComponent() override {
        std::cout << "  [DESTRUCTOR] Component '" << getName() << "' destroyed"
                  << std::endl;
    }

    // Override lifecycle methods
    bool initialize() override {
        std::cout << "  [INITIALIZE] Initializing component '" << getName()
                  << "'" << std::endl;

        // Simulate initialization work
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        setValue("status", std::string("initialized"));
        setValue("initialized", true);

        std::cout << "  [INITIALIZE] Component '" << getName()
                  << "' initialized successfully" << std::endl;
        return Component::initialize();
    }

    bool activate() {
        std::cout << "  [ACTIVATE] Activating component '" << getName() << "'"
                  << std::endl;

        if (!getVariable<bool>("initialized")->get()) {
            std::cout << "  [ERROR] Cannot activate uninitialized component"
                      << std::endl;
            return false;
        }

        setValue("status", std::string("active"));
        setState(ComponentState::Active);
        std::cout << "  [ACTIVATE] Component '" << getName()
                  << "' activated successfully" << std::endl;
        return true;
    }

    bool deactivate() {
        std::cout << "  [DEACTIVATE] Deactivating component '" << getName()
                  << "'" << std::endl;

        setValue("status", std::string("inactive"));
        setState(ComponentState::Disabled);
        std::cout << "  [DEACTIVATE] Component '" << getName()
                  << "' deactivated successfully" << std::endl;
        return true;
    }

    void cleanup() {
        std::cout << "  [CLEANUP] Cleaning up component '" << getName() << "'"
                  << std::endl;

        setValue("status", std::string("cleaned"));
        setValue("counter", 0);

        std::cout << "  [CLEANUP] Component '" << getName() << "' cleaned up"
                  << std::endl;
    }
};

void demonstrateBasicLifecycle() {
    std::cout << "\n=== Basic Component Lifecycle Demo ===" << std::endl;

    // Create component
    std::cout << "\n1. Creating component..." << std::endl;
    auto component = std::make_shared<LifecycleComponent>("TestComponent");

    // Check initial state
    std::cout << "\n2. Initial state:" << std::endl;
    std::cout << "   State: " << static_cast<int>(component->getState())
              << std::endl;
    std::cout << "   Status: " << std::any_cast<std::string>(component->runCommand("getStatus", {}))
              << std::endl;

    // Initialize component
    std::cout << "\n3. Initializing component..." << std::endl;
    if (component->initialize()) {
        std::cout << "   Initialization successful" << std::endl;
    } else {
        std::cout << "   Initialization failed" << std::endl;
        return;
    }

    // Activate component
    std::cout << "\n4. Activating component..." << std::endl;
    if (component->activate()) {
        std::cout << "   Activation successful" << std::endl;
    } else {
        std::cout << "   Activation failed" << std::endl;
        return;
    }

    // Use component
    std::cout << "\n5. Using component..." << std::endl;
    component->runCommand("increment", {});
    component->runCommand("increment", {});
    component->runCommand("increment", {});

    auto counter = component->getVariable<int>("counter");
    std::cout << "   Final counter value: " << (counter ? counter->get() : -1)
              << std::endl;

    // Deactivate component
    std::cout << "\n6. Deactivating component..." << std::endl;
    if (component->deactivate()) {
        std::cout << "   Deactivation successful" << std::endl;
    }

    // Cleanup component
    std::cout << "\n7. Cleaning up component..." << std::endl;
    component->cleanup();

    std::cout << "\n   Final status: "
              << std::any_cast<std::string>(component->runCommand("getStatus", {})) << std::endl;
}

void demonstrateStateTransitions() {
    std::cout << "\n=== Component State Transitions Demo ===" << std::endl;

    auto component = std::make_shared<LifecycleComponent>("StateComponent");

    auto printState = [&]() {
        std::cout << "   Current state: ";
        switch (component->getState()) {
            case ComponentState::Created:
                std::cout << "Created";
                break;
            case ComponentState::Initializing:
                std::cout << "Initializing";
                break;
            case ComponentState::Active:
                std::cout << "Active";
                break;
            case ComponentState::Disabled:
                std::cout << "Disabled";
                break;
            case ComponentState::Error:
                std::cout << "Error";
                break;
            case ComponentState::Destroying:
                std::cout << "Destroying";
                break;
        }
        std::cout << std::endl;
    };

    std::cout << "\n1. After creation:" << std::endl;
    printState();

    std::cout << "\n2. After initialization:" << std::endl;
    component->initialize();
    printState();

    std::cout << "\n3. After activation:" << std::endl;
    component->activate();
    printState();

    std::cout << "\n4. After deactivation:" << std::endl;
    component->deactivate();
    printState();
}

void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling Demo ===" << std::endl;

    auto component = std::make_shared<LifecycleComponent>("ErrorComponent");

    std::cout << "\n1. Attempting to activate without initialization..."
              << std::endl;
    if (!component->activate()) {
        std::cout
            << "   Activation correctly failed (component not initialized)"
            << std::endl;
    }

    std::cout << "\n2. Proper initialization and activation..." << std::endl;
    component->initialize();
    if (component->activate()) {
        std::cout << "   Activation successful after initialization"
                  << std::endl;
    }

    std::cout << "\n3. Testing command execution..." << std::endl;
    try {
        component->runCommand("nonexistent", {});
    } catch (const std::exception& e) {
        std::cout << "   Command execution correctly failed: " << e.what()
                  << std::endl;
    }

    // Test valid command
    component->runCommand("increment", {});
    std::cout << "   Valid command executed successfully" << std::endl;
}

int main() {
    std::cout << "=== Atom Component Lifecycle Examples ===" << std::endl;

    try {
        demonstrateBasicLifecycle();
        demonstrateStateTransitions();
        demonstrateErrorHandling();

        std::cout << "\n=== All Lifecycle Examples Completed Successfully! ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in lifecycle examples: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
