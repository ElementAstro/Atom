/*
 * registry_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Demonstration of Atom Component Registry Usage
Shows how to register, retrieve, and manage components using
the Atom component registry system.

**************************************************/

#include <iostream>
#include <string>
#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"

using namespace atom::components;

int main() {
    std::cout << "=== Atom Component Registry Example ===" << std::endl;

    try {
        // Get the registry singleton instance
        auto& registry = Registry::instance();

        std::cout << "\n1. Creating and registering components..." << std::endl;

        // Register components using proper InitFunc signature
        registry.addInitializer("comp1", [](Component& comp) {
            // Initialize the component with variables only
            comp.addVariable<int>("counter", 42);
            comp.addVariable<std::string>("message", "Hello from comp1");
        });

        registry.addInitializer("comp2", [](Component& comp) {
            // Initialize the second component with variables only
            comp.addVariable<int>("counter", 100);
            comp.addVariable<std::string>("message", "Hello from comp2");
        });

        std::cout << "   - Registered component 'comp1'" << std::endl;
        std::cout << "   - Registered component 'comp2'" << std::endl;

        std::cout << "\n2. Retrieving and using components..." << std::endl;

        // Retrieve components from registry
        auto retrieved1 = registry.getComponent("comp1");
        auto retrieved2 = registry.getComponent("comp2");

        if (retrieved1 && retrieved2) {
            std::cout << "   - Successfully retrieved both components"
                      << std::endl;

            // Access component variables
            auto counter1 = retrieved1->getVariable<int>("counter");
            auto message1 = retrieved1->getVariable<std::string>("message");
            auto counter2 = retrieved2->getVariable<int>("counter");
            auto message2 = retrieved2->getVariable<std::string>("message");

            if (counter1 && message1 && counter2 && message2) {
                std::cout << "   - Component 1 counter: " << counter1->get()
                          << std::endl;
                std::cout << "   - Component 1 message: " << message1->get()
                          << std::endl;
                std::cout << "   - Component 2 counter: " << counter2->get()
                          << std::endl;
                std::cout << "   - Component 2 message: " << message2->get()
                          << std::endl;

                // Modify component variables
                retrieved1->setValue("counter", counter1->get() + 10);
                retrieved1->setValue("message",
                                     std::string("Modified message for comp1"));

                std::cout << "   - Component 1 updated counter: "
                          << retrieved1->getVariable<int>("counter")->get()
                          << std::endl;
                std::cout
                    << "   - Component 1 updated message: "
                    << retrieved1->getVariable<std::string>("message")->get()
                    << std::endl;
            }
        }

        std::cout << "\n3. Listing registered components..." << std::endl;
        auto componentNames = registry.getAllComponentNames();
        std::cout << "   - Registered components: ";
        for (const auto& name : componentNames) {
            std::cout << name << " ";
        }
        std::cout << std::endl;

        std::cout << "\n4. Component lifecycle management..." << std::endl;

        // Demonstrate component removal
        registry.removeComponent("comp2");
        std::cout << "   - Removed component 'comp2'" << std::endl;

        // Try to retrieve removed component
        try {
            auto removedComponent = registry.getComponent("comp2");
            std::cout << "   - Unexpectedly found component 'comp2'"
                      << std::endl;
        } catch (const Registry::RegistryException& e) {
            std::cout << "   - Component 'comp2' is no longer available (as "
                         "expected): "
                      << e.what() << std::endl;
        }

        // Show remaining components
        componentNames = registry.getAllComponentNames();
        std::cout << "   - Remaining components: ";
        for (const auto& name : componentNames) {
            std::cout << name << " ";
        }
        std::cout << std::endl;

        std::cout
            << "\n=== Component Registry Example Completed Successfully! ==="
            << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in component registry example: " << e.what()
                  << std::endl;
        return 1;
    }

    return 0;
}
