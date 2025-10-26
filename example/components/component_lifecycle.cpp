/*
 * basic_component_lifecycle.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Comprehensive Component Lifecycle Example
Demonstrates complete component lifecycle management including:
- Component creation and initialization
- Variable management and type safety
- Command registration and dispatch
- State transitions and monitoring
- Performance statistics
- Error handling and validation
- Lifecycle hooks and events
- Memory management and cleanup

**************************************************/

#include <any>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/core/registry.hpp"
#include "atom/components/lifecycle/lifecycle.hpp"

/**
 * @brief Comprehensive component demonstrating full lifecycle management
 */
class LifecycleComponent : public Component {
public:
    explicit LifecycleComponent(const std::string& name) : Component(name) {
        std::cout << "  [CONSTRUCTOR] Component '" << name << "' created"
                  << std::endl;

        // Set component documentation
        doc("Comprehensive lifecycle demonstration component with full API "
            "coverage");

        // Initialize component variables with different types
        addVariable<int>("counter", 0, "Counter for operations", "cnt",
                         "metrics");
        addVariable<std::string>("status", "created",
                                 "Current component status", "state", "info");
        addVariable<bool>("initialized", false, "Initialization flag", "init",
                          "flags");
        addVariable<double>("performance_score", 0.0, "Performance metric",
                            "perf", "metrics");
        addVariable<std::vector<std::string>>(
            "operation_history", std::vector<std::string>{},
            "History of operations", "history", "logs");

        // Set variable constraints
        setRange<int>("counter", 0, 1000);
        std::vector<std::string> statusOptions = {
            "created",  "initializing", "initialized", "active",
            "inactive", "error",        "destroyed"};
        setStringOptions("status", statusOptions);

        // Register comprehensive command set
        registerCommands();

        // Register lifecycle hooks
        registerLifecycleHooks();
    }

private:
    void registerCommands() {
        // Basic operations
        def(
            "increment",
            [this]() -> int {
                auto counter = getVariable<int>("counter");
                if (counter) {
                    int newValue = counter->get() + 1;
                    setValue("counter", newValue);
                    addToHistory("increment");
                    std::cout << "    Counter incremented to: " << newValue
                              << std::endl;
                    return newValue;
                }
                return -1;
            },
            "operations", "Increment the counter by 1");

        def(
            "decrement",
            [this]() -> int {
                auto counter = getVariable<int>("counter");
                if (counter) {
                    int newValue = std::max(0, counter->get() - 1);
                    setValue("counter", newValue);
                    addToHistory("decrement");
                    std::cout << "    Counter decremented to: " << newValue
                              << std::endl;
                    return newValue;
                }
                return -1;
            },
            "operations", "Decrement the counter by 1");

        def(
            "reset",
            [this]() {
                setValue("counter", 0);
                addToHistory("reset");
                std::cout << "    Counter reset to 0" << std::endl;
            },
            "operations", "Reset counter to zero");

        def(
            "setCounter",
            [this](int value) {
                if (value >= 0 && value <= 1000) {
                    setValue("counter", value);
                    addToHistory("setCounter:" + std::to_string(value));
                    std::cout << "    Counter set to: " << value << std::endl;
                } else {
                    throw std::out_of_range(
                        "Counter value must be between 0 and 1000");
                }
            },
            "operations", "Set counter to specific value");

        // Status and information commands
        def(
            "getStatus",
            [this]() -> std::string {
                auto status = getVariable<std::string>("status");
                return status ? status->get() : "unknown";
            },
            "info", "Get current component status");

        def(
            "getCounter",
            [this]() -> int {
                auto counter = getVariable<int>("counter");
                return counter ? counter->get() : -1;
            },
            "info", "Get current counter value");

        def(
            "getPerformanceScore",
            [this]() -> double {
                auto score = getVariable<double>("performance_score");
                return score ? score->get() : 0.0;
            },
            "info", "Get performance score");

        def(
            "getHistory",
            [this]() -> std::vector<std::string> {
                auto history =
                    getVariable<std::vector<std::string>>("operation_history");
                return history ? history->get() : std::vector<std::string>{};
            },
            "info", "Get operation history");

        def(
            "clearHistory",
            [this]() {
                setValue("operation_history", std::vector<std::string>{});
                std::cout << "    Operation history cleared" << std::endl;
            },
            "operations", "Clear operation history");

        // Performance and diagnostics
        def(
            "calculatePerformance",
            [this]() -> double {
                auto counter = getVariable<int>("counter");
                auto history =
                    getVariable<std::vector<std::string>>("operation_history");

                if (counter && history) {
                    double score =
                        static_cast<double>(counter->get()) /
                        std::max(1.0,
                                 static_cast<double>(history->get().size()));
                    setValue("performance_score", score);
                    std::cout << "    Performance score calculated: " << score
                              << std::endl;
                    return score;
                }
                return 0.0;
            },
            "diagnostics", "Calculate performance score");

        def(
            "getStats",
            [this]() -> std::string {
                const auto& stats = getPerformanceStats();
                std::string result = "Performance Statistics:\n";
                result += "  Command calls: " +
                          std::to_string(stats.commandCallCount.load()) + "\n";
                result += "  Command errors: " +
                          std::to_string(stats.commandErrorCount.load()) + "\n";
                result += "  Avg execution time: " +
                          std::to_string(stats.getAvgExecutionTime().count()) +
                          "μs";
                return result;
            },
            "diagnostics", "Get performance statistics");
    }

    void registerLifecycleHooks() {
        auto& lifecycle = atom::components::LifecycleManager::instance();

        // Register component-specific hooks
        lifecycle.registerHook(
            std::string(getName()),
            atom::components::LifecyclePhase::PreInitialization,
            [this](Component& comp, atom::components::LifecyclePhase phase) {
                (void)phase;  // Suppress unused parameter warning
                std::cout << "  [HOOK] Pre-initialization for "
                          << comp.getName() << std::endl;
                setValue("status", std::string("initializing"));
            });

        lifecycle.registerHook(
            std::string(getName()),
            atom::components::LifecyclePhase::PostInitialization,
            [this](Component& comp, atom::components::LifecyclePhase phase) {
                (void)phase;  // Suppress unused parameter warning
                std::cout << "  [HOOK] Post-initialization for "
                          << comp.getName() << std::endl;
                setValue("status", std::string("initialized"));
                setValue("initialized", true);
                addToHistory("initialized");
            });

        lifecycle.registerHook(
            std::string(getName()),
            atom::components::LifecyclePhase::PostActivation,
            [this](Component& comp, atom::components::LifecyclePhase phase) {
                (void)phase;  // Suppress unused parameter warning
                std::cout << "  [HOOK] Post-activation for " << comp.getName()
                          << std::endl;
                setValue("status", std::string("active"));
                addToHistory("activated");
            });

        lifecycle.registerHook(
            std::string(getName()),
            atom::components::LifecyclePhase::PreDeactivation,
            [this](Component& comp, atom::components::LifecyclePhase phase) {
                (void)phase;  // Suppress unused parameter warning
                std::cout << "  [HOOK] Pre-deactivation for " << comp.getName()
                          << std::endl;
                setValue("status", std::string("inactive"));
                addToHistory("deactivated");
            });
    }

    void addToHistory(const std::string& operation) {
        auto history =
            getVariable<std::vector<std::string>>("operation_history");
        if (history) {
            auto currentHistory = history->get();
            currentHistory.push_back(operation);
            // Keep only last 50 operations
            if (currentHistory.size() > 50) {
                currentHistory.erase(currentHistory.begin());
            }
            setValue("operation_history", currentHistory);
        }
    }

public:
    ~LifecycleComponent() override {
        std::cout << "  [DESTRUCTOR] Component '" << getName() << "' destroyed"
                  << std::endl;

        // Execute destruction lifecycle hooks
        auto& lifecycle = atom::components::LifecycleManager::instance();
        lifecycle.executePhase(
            *this, atom::components::LifecyclePhase::PreDestruction);

        addToHistory("destroyed");
        setValue("status", std::string("destroyed"));

        lifecycle.executePhase(
            *this, atom::components::LifecyclePhase::PostDestruction);
    }

    // Override lifecycle methods with comprehensive functionality
    bool initialize() override {
        std::cout << "  [INITIALIZE] Initializing component '" << getName()
                  << "'" << std::endl;

        // Execute pre-initialization hooks
        auto& lifecycle = atom::components::LifecycleManager::instance();
        lifecycle.executePhase(
            *this, atom::components::LifecyclePhase::PreInitialization);

        // Simulate initialization work
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Call base class initialization
        bool success = Component::initialize();

        if (success) {
            // Execute post-initialization hooks
            lifecycle.executePhase(
                *this, atom::components::LifecyclePhase::PostInitialization);
            std::cout << "  [INITIALIZE] Component '" << getName()
                      << "' initialized successfully" << std::endl;
        } else {
            setValue("status", std::string("error"));
            std::cout << "  [ERROR] Component '" << getName()
                      << "' initialization failed" << std::endl;
        }

        return success;
    }

    bool activate() {
        std::cout << "  [ACTIVATE] Activating component '" << getName() << "'"
                  << std::endl;

        auto initialized = getVariable<bool>("initialized");
        if (!initialized || !initialized->get()) {
            std::cout << "  [ERROR] Cannot activate uninitialized component"
                      << std::endl;
            return false;
        }

        auto& lifecycle = atom::components::LifecycleManager::instance();

        // Execute pre-activation hooks
        lifecycle.executePhase(*this,
                               atom::components::LifecyclePhase::PreActivation);

        setState(ComponentState::Active);

        // Execute post-activation hooks
        lifecycle.executePhase(
            *this, atom::components::LifecyclePhase::PostActivation);

        std::cout << "  [ACTIVATE] Component '" << getName()
                  << "' activated successfully" << std::endl;
        return true;
    }

    bool deactivate() {
        std::cout << "  [DEACTIVATE] Deactivating component '" << getName()
                  << "'" << std::endl;

        auto& lifecycle = atom::components::LifecycleManager::instance();

        // Execute pre-deactivation hooks
        lifecycle.executePhase(
            *this, atom::components::LifecyclePhase::PreDeactivation);

        setState(ComponentState::Disabled);

        // Execute post-deactivation hooks
        lifecycle.executePhase(
            *this, atom::components::LifecyclePhase::PostDeactivation);

        std::cout << "  [DEACTIVATE] Component '" << getName()
                  << "' deactivated successfully" << std::endl;
        return true;
    }

    void cleanup() {
        std::cout << "  [CLEANUP] Cleaning up component '" << getName() << "'"
                  << std::endl;

        setValue("status", std::string("cleaned"));
        setValue("counter", 0);
        setValue("performance_score", 0.0);
        setValue("operation_history", std::vector<std::string>{});

        // Reset performance statistics
        resetPerformanceStats();

        std::cout << "  [CLEANUP] Component '" << getName() << "' cleaned up"
                  << std::endl;
    }

    // Additional utility methods
    void printComponentInfo() {
        std::cout << "\n--- Component Information ---" << std::endl;
        std::cout << "Name: " << getName() << std::endl;
        std::cout << "Documentation: " << getDoc() << std::endl;
        std::cout << "State: " << static_cast<int>(getState()) << std::endl;

        auto status = getVariable<std::string>("status");
        auto counter = getVariable<int>("counter");
        auto score = getVariable<double>("performance_score");

        if (status)
            std::cout << "Status: " << status->get() << std::endl;
        if (counter)
            std::cout << "Counter: " << counter->get() << std::endl;
        if (score)
            std::cout << "Performance Score: " << score->get() << std::endl;

        const auto& stats = getPerformanceStats();
        std::cout << "Command Calls: " << stats.commandCallCount.load()
                  << std::endl;
        std::cout << "Command Errors: " << stats.commandErrorCount.load()
                  << std::endl;
        std::cout << "Avg Execution Time: "
                  << stats.getAvgExecutionTime().count() << "μs" << std::endl;
        std::cout << "----------------------------" << std::endl;
    }
};

/**
 * @brief Demonstrates comprehensive component lifecycle management
 */
void demonstrateBasicLifecycle() {
    std::cout << "\n=== Comprehensive Component Lifecycle Demo ==="
              << std::endl;

    // Create component
    std::cout << "\n1. Creating component..." << std::endl;
    auto component = std::make_shared<LifecycleComponent>("TestComponent");
    component->printComponentInfo();

    // Initialize component
    std::cout << "\n2. Initializing component..." << std::endl;
    if (component->initialize()) {
        std::cout << "   Initialization successful" << std::endl;
        component->printComponentInfo();
    } else {
        std::cout << "   Initialization failed" << std::endl;
        return;
    }

    // Activate component
    std::cout << "\n3. Activating component..." << std::endl;
    if (component->activate()) {
        std::cout << "   Activation successful" << std::endl;
    } else {
        std::cout << "   Activation failed" << std::endl;
        return;
    }

    // Use component - demonstrate command execution
    std::cout << "\n4. Using component commands..." << std::endl;

    // Execute commands and capture results
    auto result1 = component->runCommand("increment", {});
    auto result2 = component->runCommand("increment", {});
    auto result3 = component->runCommand("increment", {});

    std::cout << "   Increment results: " << std::any_cast<int>(result1) << ", "
              << std::any_cast<int>(result2) << ", "
              << std::any_cast<int>(result3) << std::endl;

    // Test other commands
    std::vector<std::any> setArgs = {std::any(10)};
    [[maybe_unused]] auto setResult =
        component->runCommand("setCounter", setArgs);
    auto counterResult = component->runCommand("getCounter", {});
    std::cout << "   Counter after setting to 10: "
              << std::any_cast<int>(counterResult) << std::endl;

    // Calculate and display performance
    auto perfResult = component->runCommand("calculatePerformance", {});
    std::cout << "   Performance score: " << std::any_cast<double>(perfResult)
              << std::endl;

    // Show operation history
    auto historyResult = component->runCommand("getHistory", {});
    auto history = std::any_cast<std::vector<std::string>>(historyResult);
    std::cout << "   Operation history (" << history.size() << " operations): ";
    for (const auto& op : history) {
        std::cout << op << " ";
    }
    std::cout << std::endl;

    // Display performance statistics
    auto statsResult = component->runCommand("getStats", {});
    std::cout << "   " << std::any_cast<std::string>(statsResult) << std::endl;

    // Deactivate component
    std::cout << "\n5. Deactivating component..." << std::endl;
    if (component->deactivate()) {
        std::cout << "   Deactivation successful" << std::endl;
        component->printComponentInfo();
    }

    // Cleanup component
    std::cout << "\n6. Cleaning up component..." << std::endl;
    component->cleanup();
    component->printComponentInfo();

    auto finalStatus = component->runCommand("getStatus", {});
    std::cout << "\n   Final status: "
              << std::any_cast<std::string>(finalStatus) << std::endl;
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
        [[maybe_unused]] auto failResult =
            component->runCommand("nonexistent", {});
    } catch (const std::exception& e) {
        std::cout << "   Command execution correctly failed: " << e.what()
                  << std::endl;
    }

    // Test valid command
    [[maybe_unused]] auto validResult = component->runCommand("increment", {});
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
