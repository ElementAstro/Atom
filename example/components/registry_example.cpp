/*
 * registry_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Comprehensive Atom Component Registry Example
Demonstrates complete Registry API including component creation, initialization,
dependency management, lifecycle integration, performance monitoring, and
advanced registry features.

**************************************************/

// Define feature flags before including headers
#ifndef ENABLE_FASTHASH
#define ENABLE_FASTHASH 0
#endif
#ifndef ENABLE_EVENT_SYSTEM
#define ENABLE_EVENT_SYSTEM 0
#endif
#ifndef ENABLE_HOT_RELOAD
#define ENABLE_HOT_RELOAD 0
#endif

#include <any>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/core/registry.hpp"
#include "atom/components/lifecycle/lifecycle.hpp"

// Note: Registry and Component are in the global namespace
// LifecycleManager and related types are in atom::components namespace

/**
 * @brief Example component demonstrating registry features
 */
class RegistryComponent : public Component {
public:
    explicit RegistryComponent(const std::string& name) : Component(name) {
        std::cout << "RegistryComponent '" << name << "' created" << std::endl;

        // Add comprehensive variables
        addVariable<int>("counter", 0);
        addVariable<std::string>("status", "created");
        addVariable<bool>("active", false);
        addVariable<double>("version", 1.0);
        addVariable<std::vector<std::string>>(
            "tags", std::vector<std::string>{"registry", "example"});

        // Add commands for demonstration
        def(
            "increment",
            [this]() -> int {
                auto counter = getVariable<int>("counter");
                if (counter) {
                    int newValue = counter->get() + 1;
                    setValue("counter", newValue);
                    return newValue;
                }
                return 0;
            },
            "operations", "Increment the counter");

        def(
            "setStatus",
            [this](const std::string& status) {
                setValue("status", status);
                std::cout << "  [" << getName() << "] Status set to: " << status
                          << std::endl;
            },
            "operations", "Set component status");

        def(
            "getInfo",
            [this]() -> std::string {
                auto counter = getVariable<int>("counter");
                auto status = getVariable<std::string>("status");
                auto version = getVariable<double>("version");

                std::string info = "Component: " + std::string(getName());
                if (counter)
                    info += ", Counter: " + std::to_string(counter->get());
                if (status)
                    info += ", Status: " + status->get();
                if (version)
                    info += ", Version: " + std::to_string(version->get());

                return info;
            },
            "diagnostics", "Get component information");

        def(
            "activate",
            [this]() -> bool {
                setValue("active", true);
                setValue("status", std::string("active"));
                std::cout << "  [" << getName() << "] Component activated"
                          << std::endl;
                return true;
            },
            "lifecycle", "Activate the component");

        def(
            "deactivate",
            [this]() -> bool {
                setValue("active", false);
                setValue("status", std::string("inactive"));
                std::cout << "  [" << getName() << "] Component deactivated"
                          << std::endl;
                return true;
            },
            "lifecycle", "Deactivate the component");
    }

    bool initialize() override {
        std::cout << "  [" << getName() << "] Initializing..." << std::endl;
        setValue("status", std::string("initialized"));
        return Component::initialize();
    }
};

/**
 * @brief Service component with dependencies
 */
class ServiceComponent : public RegistryComponent {
public:
    explicit ServiceComponent(const std::string& name)
        : RegistryComponent(name) {
        std::cout << "ServiceComponent '" << name << "' created" << std::endl;

        // Add service-specific variables
        addVariable<int>("port", 8080);
        addVariable<std::string>("host", "localhost");
        addVariable<bool>("running", false);

        // Add service commands
        def(
            "start",
            [this]() -> bool {
                setValue("running", true);
                setValue("status", std::string("running"));
                std::cout << "  [" << getName() << "] Service started"
                          << std::endl;
                return true;
            },
            "service", "Start the service");

        def(
            "stop",
            [this]() -> bool {
                setValue("running", false);
                setValue("status", std::string("stopped"));
                std::cout << "  [" << getName() << "] Service stopped"
                          << std::endl;
                return true;
            },
            "service", "Stop the service");

        def(
            "getServiceInfo",
            [this]() -> std::string {
                auto port = getVariable<int>("port");
                auto host = getVariable<std::string>("host");
                auto running = getVariable<bool>("running");

                std::string info = "Service: " + std::string(getName());
                if (host && port) {
                    info +=
                        " @ " + host->get() + ":" + std::to_string(port->get());
                }
                if (running) {
                    info += " (Running: " +
                            std::string(running->get() ? "Yes" : "No") + ")";
                }

                return info;
            },
            "diagnostics", "Get service information");
    }
};

void demonstrateBasicRegistry() {
    std::cout << "\n=== Basic Registry Operations Demo ===" << std::endl;

    auto& registry = Registry::instance();

    std::cout << "\n1. Creating components with registry..." << std::endl;

    // Create components using the registry
    auto comp1 = registry.createComponent<RegistryComponent>("Component1");
    auto comp2 = registry.createComponent<RegistryComponent>("Component2");
    auto service1 = registry.createComponent<ServiceComponent>("WebService");
    auto service2 =
        registry.createComponent<ServiceComponent>("DatabaseService");

    std::cout << "   Created components: Component1, Component2, WebService, "
                 "DatabaseService"
              << std::endl;

    std::cout << "\n2. Initializing components..." << std::endl;

    // Initialize all components
    registry.initializeAll();

    std::cout << "\n3. Retrieving and using components..." << std::endl;

    // Retrieve components from registry
    auto retrieved1 = registry.getComponent("Component1");
    auto retrieved2 = registry.getComponent("Component2");
    auto retrievedService = registry.getComponent("WebService");

    if (retrieved1 && retrieved2 && retrievedService) {
        std::cout << "   Successfully retrieved all components" << std::endl;

        // Use component commands
        auto result1 = retrieved1->runCommand("increment", {});
        auto result2 = retrieved1->runCommand("increment", {});
        std::cout << "   Component1 increment results: "
                  << std::any_cast<int>(result1) << ", "
                  << std::any_cast<int>(result2) << std::endl;

        // Set status
        std::vector<std::any> statusArgs = {std::any(std::string("testing"))};
        [[maybe_unused]] auto statusResult =
            retrieved1->runCommand("setStatus", statusArgs);

        // Get component info
        auto info1 = retrieved1->runCommand("getInfo", {});
        auto info2 = retrieved2->runCommand("getInfo", {});
        std::cout << "   " << std::any_cast<std::string>(info1) << std::endl;
        std::cout << "   " << std::any_cast<std::string>(info2) << std::endl;

        // Use service commands
        [[maybe_unused]] auto startResult =
            retrievedService->runCommand("start", {});
        auto serviceInfo = retrievedService->runCommand("getServiceInfo", {});
        std::cout << "   " << std::any_cast<std::string>(serviceInfo)
                  << std::endl;
    }

    std::cout << "\n4. Listing all components..." << std::endl;
    auto componentNames = registry.getAllComponentNames();
    std::cout << "   Registered components (" << componentNames.size() << "): ";
    for (const auto& name : componentNames) {
        std::cout << name << " ";
    }
    std::cout << std::endl;

    // Get all components
    auto allComponents = registry.getAllComponents();
    std::cout << "   Component details:" << std::endl;
    for (const auto& comp : allComponents) {
        if (comp) {
            std::cout << "     - " << comp->getName()
                      << " (State: " << static_cast<int>(comp->getState())
                      << ")" << std::endl;
        }
    }
}

void demonstrateAdvancedRegistry() {
    std::cout << "\n=== Advanced Registry Features Demo ===" << std::endl;

    auto& registry = Registry::instance();

    std::cout << "\n5. Component lifecycle management..." << std::endl;

    // Test component removal
    std::cout << "   Removing Component2..." << std::endl;
    registry.removeComponent("Component2");

    // Try to retrieve removed component
    try {
        auto removedComponent = registry.getComponent("Component2");
        if (removedComponent) {
            std::cout << "   ERROR: Component2 still exists after removal!"
                      << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "   Component2 correctly removed: " << e.what()
                  << std::endl;
    }

    // Show remaining components
    auto remainingNames = registry.getAllComponentNames();
    std::cout << "   Remaining components (" << remainingNames.size() << "): ";
    for (const auto& name : remainingNames) {
        std::cout << name << " ";
    }
    std::cout << std::endl;

    std::cout << "\n6. Component initialization with custom initializers..."
              << std::endl;

    // Add custom initializer
    registry.addInitializer("CustomComponent", [](Component& comp) {
        std::cout << "   Custom initializer for " << comp.getName()
                  << std::endl;
        comp.addVariable<std::string>("custom_property", "initialized_value");
        comp.addVariable<int>("init_timestamp",
                              static_cast<int>(std::time(nullptr)));
    });

    // Create component with custom initializer
    auto customComp =
        registry.createComponent<RegistryComponent>("CustomComponent");

    // Verify custom initialization
    auto customProp = customComp->getVariable<std::string>("custom_property");
    auto timestamp = customComp->getVariable<int>("init_timestamp");
    if (customProp && timestamp) {
        std::cout << "   Custom property: " << customProp->get() << std::endl;
        std::cout << "   Init timestamp: " << timestamp->get() << std::endl;
    }

    std::cout << "\n7. Performance and statistics..." << std::endl;

    // Demonstrate performance monitoring
    auto comp1 = registry.getComponent("Component1");
    if (comp1) {
        // Execute multiple commands to generate statistics
        for (int i = 0; i < 5; ++i) {
            [[maybe_unused]] auto incResult =
                comp1->runCommand("increment", {});
        }

        const auto& stats = comp1->getPerformanceStats();
        std::cout << "   Component1 performance stats:" << std::endl;
        std::cout << "     Command calls: " << stats.commandCallCount.load()
                  << std::endl;
        std::cout << "     Command errors: " << stats.commandErrorCount.load()
                  << std::endl;
        std::cout << "     Avg execution time: "
                  << stats.getAvgExecutionTime().count() << "μs" << std::endl;
    }
}

void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling Demo ===" << std::endl;

    auto& registry = Registry::instance();

    std::cout << "\n8. Testing error conditions..." << std::endl;

    // Test retrieving non-existent component
    try {
        auto nonExistent = registry.getComponent("NonExistentComponent");
        std::cout << "   ERROR: Should not have found NonExistentComponent!"
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "   Correctly handled non-existent component: " << e.what()
                  << std::endl;
    }

    // Test invalid command execution
    auto comp1 = registry.getComponent("Component1");
    if (comp1) {
        try {
            [[maybe_unused]] auto result =
                comp1->runCommand("nonexistent_command", {});
            std::cout
                << "   ERROR: Should not have executed non-existent command!"
                << std::endl;
        } catch (const std::exception& e) {
            std::cout << "   Correctly handled invalid command: " << e.what()
                      << std::endl;
        }
    }

    std::cout << "\n9. Registry cleanup..." << std::endl;

    // Clean up remaining components
    auto allNames = registry.getAllComponentNames();
    std::cout << "   Cleaning up " << allNames.size() << " components..."
              << std::endl;

    for (const auto& name : allNames) {
        try {
            registry.removeComponent(name);
            std::cout << "     Removed: " << name << std::endl;
        } catch (const std::exception& e) {
            std::cout << "     Failed to remove " << name << ": " << e.what()
                      << std::endl;
        }
    }

    auto finalNames = registry.getAllComponentNames();
    std::cout << "   Final component count: " << finalNames.size() << std::endl;
}

int main() {
    std::cout << "=== Comprehensive Atom Component Registry Example ==="
              << std::endl;

    try {
        demonstrateBasicRegistry();
        demonstrateAdvancedRegistry();
        demonstrateErrorHandling();

        std::cout << "\n=== All Registry Examples Completed Successfully! ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in registry examples: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
