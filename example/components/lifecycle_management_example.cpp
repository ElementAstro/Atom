/*
 * lifecycle_management_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Lifecycle Management and Dependency Example
Demonstrates lifecycle hooks, dependency constraints, event handling,
and advanced lifecycle management features.

**************************************************/

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/lifecycle.hpp"
#include "atom/components/registry.hpp"

using namespace atom::components;

/**
 * @brief Service component demonstrating lifecycle hooks
 */
class ServiceComponent : public Component {
public:
    explicit ServiceComponent(const std::string& name) : Component(name) {
        std::cout << "ServiceComponent '" << name << "' constructed"
                  << std::endl;

        addVariable<std::string>("service_type", "generic");
        addVariable<bool>("is_running", false);
        addVariable<int>("start_count", 0);

        def("start", [this]() -> bool {
            auto count = getVariable<int>("start_count");
            if (count) {
                setValue("start_count", count->get() + 1);
            }
            setValue("is_running", true);
            std::cout << "  [" << getName() << "] Service started (count: "
                      << (count ? count->get() + 1 : 1) << ")" << std::endl;
            return true;
        });

        def("stop", [this]() {
            setValue("is_running", false);
            std::cout << "  [" << getName() << "] Service stopped" << std::endl;
        });

        def("isRunning", [this]() -> bool {
            auto running = getVariable<bool>("is_running");
            return running ? running->get() : false;
        });
    }

    bool initialize() override {
        std::cout << "  [" << getName() << "] Initializing service..."
                  << std::endl;
        setValue("service_type", getName());
        return Component::initialize();
    }

    bool activate() override {
        std::cout << "  [" << getName() << "] Activating service..."
                  << std::endl;
        executeCommand("start", {});
        return Component::activate();
    }

    bool deactivate() override {
        std::cout << "  [" << getName() << "] Deactivating service..."
                  << std::endl;
        executeCommand("stop", {});
        return Component::deactivate();
    }

    void cleanup() override {
        std::cout << "  [" << getName() << "] Cleaning up service..."
                  << std::endl;
        setValue("is_running", false);
        Component::cleanup();
    }
};

/**
 * @brief Database service with specific requirements
 */
class DatabaseService : public ServiceComponent {
public:
    explicit DatabaseService(const std::string& name) : ServiceComponent(name) {
        addVariable<std::string>("connection_string", "localhost:5432");
        addVariable<int>("connection_pool_size", 10);
        addVariable<bool>("connected", false);
    }

    bool initialize() override {
        std::cout << "  [" << getName() << "] Initializing database service..."
                  << std::endl;
        // Simulate database connection
        setValue("connected", true);
        return ServiceComponent::initialize();
    }

    void cleanup() override {
        std::cout << "  [" << getName() << "] Disconnecting from database..."
                  << std::endl;
        setValue("connected", false);
        ServiceComponent::cleanup();
    }
};

/**
 * @brief Web service that depends on database
 */
class WebService : public ServiceComponent {
public:
    explicit WebService(const std::string& name) : ServiceComponent(name) {
        addVariable<int>("port", 8080);
        addVariable<std::string>("host", "localhost");
        addVariable<bool>("ssl_enabled", false);
    }

    bool initialize() override {
        std::cout << "  [" << getName() << "] Initializing web service..."
                  << std::endl;

        // Check database dependency
        auto& registry = Registry::instance();
        try {
            auto db = registry.getComponent("Database");
            if (db) {
                auto connected = db->getVariable<bool>("connected");
                if (connected && connected->get()) {
                    std::cout
                        << "    Database dependency is available and connected"
                        << std::endl;
                } else {
                    std::cout << "    Warning: Database is not connected"
                              << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cout << "    Error: Database dependency not available: "
                      << e.what() << std::endl;
            return false;
        }

        return ServiceComponent::initialize();
    }
};

/**
 * @brief Application service that depends on both database and web services
 */
class ApplicationService : public ServiceComponent {
public:
    explicit ApplicationService(const std::string& name)
        : ServiceComponent(name) {
        addVariable<std::string>("app_name", "MyApp");
        addVariable<std::string>("version", "1.0.0");
        addVariable<bool>("ready", false);
    }

    bool initialize() override {
        std::cout << "  [" << getName()
                  << "] Initializing application service..." << std::endl;

        // Verify all dependencies
        auto& registry = Registry::instance();
        std::vector<std::string> dependencies = {"Database", "WebService"};

        for (const auto& dep : dependencies) {
            try {
                auto component = registry.getComponent(dep);
                if (!component) {
                    std::cout << "    Error: Dependency '" << dep
                              << "' not found" << std::endl;
                    return false;
                }

                if (component->getState() != ComponentState::Active) {
                    std::cout << "    Error: Dependency '" << dep
                              << "' is not active" << std::endl;
                    return false;
                }

                std::cout << "    Dependency '" << dep << "' is ready"
                          << std::endl;
            } catch (const std::exception& e) {
                std::cout << "    Error checking dependency '" << dep
                          << "': " << e.what() << std::endl;
                return false;
            }
        }

        setValue("ready", true);
        return ServiceComponent::initialize();
    }
};

void setupLifecycleHooks() {
    std::cout << "\n=== Setting up Lifecycle Hooks ===" << std::endl;

    auto& lifecycle = LifecycleManager::instance();

    // Global hooks that apply to all components
    lifecycle.registerGlobalHook(
        LifecyclePhase::PreInitialization, [](Component& component) {
            std::cout << "  [GLOBAL] Pre-initialization hook for: "
                      << component.getName() << std::endl;
        });

    lifecycle.registerGlobalHook(
        LifecyclePhase::PostInitialization, [](Component& component) {
            std::cout << "  [GLOBAL] Post-initialization hook for: "
                      << component.getName() << std::endl;
        });

    lifecycle.registerGlobalHook(
        LifecyclePhase::PreActivation, [](Component& component) {
            std::cout << "  [GLOBAL] Pre-activation hook for: "
                      << component.getName() << std::endl;
        });

    lifecycle.registerGlobalHook(
        LifecyclePhase::PostActivation, [](Component& component) {
            std::cout << "  [GLOBAL] Post-activation hook for: "
                      << component.getName() << std::endl;
        });

    // Component-specific hooks
    lifecycle.registerHook(
        "Database", LifecyclePhase::PostInitialization,
        [](Component& component) {
            std::cout << "  [DATABASE] Database-specific post-init hook"
                      << std::endl;
        });

    lifecycle.registerHook(
        "WebService", LifecyclePhase::PreActivation, [](Component& component) {
            std::cout << "  [WEB] Web service pre-activation hook" << std::endl;
        });

    lifecycle.registerHook(
        "Application", LifecyclePhase::PostActivation,
        [](Component& component) {
            std::cout
                << "  [APP] Application post-activation hook - system ready!"
                << std::endl;
        });

    std::cout << "Lifecycle hooks registered successfully" << std::endl;
}

void setupDependencies() {
    std::cout << "\n=== Setting up Dependencies ===" << std::endl;

    auto& lifecycle = LifecycleManager::instance();

    // WebService depends on Database (required)
    lifecycle.addDependency(
        "WebService",
        DependencyConstraint("Database", DependencyType::Required));

    // Application depends on both Database and WebService (required)
    lifecycle.addDependency(
        "Application",
        DependencyConstraint("Database", DependencyType::Required));
    lifecycle.addDependency(
        "Application",
        DependencyConstraint("WebService", DependencyType::Required));

    std::cout << "Dependencies configured:" << std::endl;
    std::cout << "  WebService -> Database (Required)" << std::endl;
    std::cout << "  Application -> Database (Required)" << std::endl;
    std::cout << "  Application -> WebService (Required)" << std::endl;
}

void demonstrateLifecycleExecution() {
    std::cout << "\n=== Lifecycle Execution Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto& lifecycle = LifecycleManager::instance();

    std::cout << "\n1. Creating components..." << std::endl;

    // Create components (order doesn't matter)
    auto app = registry.createComponent<ApplicationService>("Application");
    auto web = registry.createComponent<WebService>("WebService");
    auto db = registry.createComponent<DatabaseService>("Database");

    std::cout << "\n2. Resolving dependencies..." << std::endl;

    // Resolve dependencies for Application
    auto dependencies = lifecycle.resolveDependencies("Application");
    std::cout << "Initialization order for Application: ";
    for (const auto& dep : dependencies) {
        std::cout << dep << " -> ";
    }
    std::cout << "Application" << std::endl;

    std::cout << "\n3. Executing lifecycle phases..." << std::endl;

    // Initialize components in dependency order
    for (const auto& componentName : dependencies) {
        auto component = registry.getComponent(componentName);
        if (component) {
            std::cout << "\nInitializing: " << componentName << std::endl;

            // Execute pre-initialization hooks
            lifecycle.executePhase(*component,
                                   LifecyclePhase::PreInitialization);

            // Initialize component
            if (component->initialize()) {
                // Execute post-initialization hooks
                lifecycle.executePhase(*component,
                                       LifecyclePhase::PostInitialization);
                std::cout << "  " << componentName
                          << " initialized successfully" << std::endl;
            } else {
                std::cout << "  " << componentName << " initialization failed"
                          << std::endl;
                return;
            }
        }
    }

    // Initialize the main component
    std::cout << "\nInitializing: Application" << std::endl;
    lifecycle.executePhase(*app, LifecyclePhase::PreInitialization);
    if (app->initialize()) {
        lifecycle.executePhase(*app, LifecyclePhase::PostInitialization);
        std::cout << "  Application initialized successfully" << std::endl;
    } else {
        std::cout << "  Application initialization failed" << std::endl;
        return;
    }

    std::cout << "\n4. Activating components..." << std::endl;

    // Activate components in dependency order
    for (const auto& componentName : dependencies) {
        auto component = registry.getComponent(componentName);
        if (component) {
            std::cout << "\nActivating: " << componentName << std::endl;

            lifecycle.executePhase(*component, LifecyclePhase::PreActivation);
            if (component->activate()) {
                lifecycle.executePhase(*component,
                                       LifecyclePhase::PostActivation);
                std::cout << "  " << componentName << " activated successfully"
                          << std::endl;
            } else {
                std::cout << "  " << componentName << " activation failed"
                          << std::endl;
            }
        }
    }

    // Activate the main component
    std::cout << "\nActivating: Application" << std::endl;
    lifecycle.executePhase(*app, LifecyclePhase::PreActivation);
    if (app->activate()) {
        lifecycle.executePhase(*app, LifecyclePhase::PostActivation);
        std::cout << "  Application activated successfully" << std::endl;
    } else {
        std::cout << "  Application activation failed" << std::endl;
    }
}

void demonstrateLifecycleShutdown() {
    std::cout << "\n=== Lifecycle Shutdown Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto& lifecycle = LifecycleManager::instance();

    std::cout << "\n5. Shutting down components..." << std::endl;

    // Shutdown in reverse dependency order
    std::vector<std::string> shutdownOrder = {"Application", "WebService",
                                              "Database"};

    for (const auto& componentName : shutdownOrder) {
        auto component = registry.getComponent(componentName);
        if (component) {
            std::cout << "\nShutting down: " << componentName << std::endl;

            // Deactivate
            lifecycle.executePhase(*component, LifecyclePhase::PreDeactivation);
            component->deactivate();
            lifecycle.executePhase(*component,
                                   LifecyclePhase::PostDeactivation);

            // Cleanup
            lifecycle.executePhase(*component, LifecyclePhase::PreDestruction);
            component->cleanup();
            lifecycle.executePhase(*component, LifecyclePhase::PostDestruction);

            std::cout << "  " << componentName << " shut down successfully"
                      << std::endl;
        }
    }
}

void demonstrateLifecycleHistory() {
    std::cout << "\n=== Lifecycle History Demo ===" << std::endl;

    auto& lifecycle = LifecycleManager::instance();

    std::cout << "\n6. Lifecycle event history:" << std::endl;

    auto history = lifecycle.getHistory();
    std::cout << "Total lifecycle events: " << history.size() << std::endl;

    // Show last few events
    size_t showCount = std::min(size_t(10), history.size());
    std::cout << "Last " << showCount << " events:" << std::endl;

    for (size_t i = history.size() - showCount; i < history.size(); ++i) {
        const auto& event = history[i];
        std::cout << "  " << event.componentName
                  << " - Phase: " << static_cast<int>(event.phase)
                  << " - Success: " << (event.success ? "Yes" : "No")
                  << std::endl;
    }
}

int main() {
    std::cout << "=== Atom Component Lifecycle Management Examples ==="
              << std::endl;

    try {
        setupLifecycleHooks();
        setupDependencies();
        demonstrateLifecycleExecution();
        demonstrateLifecycleShutdown();
        demonstrateLifecycleHistory();

        std::cout << "\n=== All Lifecycle Management Examples Completed "
                     "Successfully! ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in lifecycle management examples: " << e.what()
                  << std::endl;
        return 1;
    }

    return 0;
}
