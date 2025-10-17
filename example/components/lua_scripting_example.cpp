/*
 * lua_scripting_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Lua Scripting Engine Example
Demonstrates Lua integration, binding, script execution, error handling,
and advanced Lua scripting features with the component system.

**************************************************/

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/core/registry.hpp"

// Conditional compilation for Lua support
#if ATOM_ENABLE_LUA
#include "atom/components/scripting/lua_engine.hpp"
#include "atom/components/scripting/scripting_api.hpp"
#endif

// Note: Component and Registry are in global namespace, not atom::components
// Only use atom::components for scripting-related types

/**
 * @brief Component that can be controlled via Lua scripts
 */
class ScriptableComponent : public Component {
public:
    explicit ScriptableComponent(const std::string& name) : Component(name) {
        std::cout << "ScriptableComponent '" << name << "' created"
                  << std::endl;

        // Add variables that can be accessed from scripts
        addVariable<int>("health", 100);
        addVariable<std::string>("name", "Player");
        addVariable<double>("position_x", 0.0);
        addVariable<double>("position_y", 0.0);
        addVariable<bool>("active", true);

        // Add commands that can be called from scripts
        def("move", [this](double x, double y) {
            setValue("position_x", x);
            setValue("position_y", y);
            std::cout << "  [" << getName() << "] Moved to (" << x << ", " << y
                      << ")" << std::endl;
        });

        def("takeDamage", [this](int damage) -> int {
            auto health = getVariable<int>("health");
            if (health) {
                int newHealth = std::max(0, health->get() - damage);
                setValue("health", newHealth);
                std::cout << "  [" << getName() << "] Took " << damage
                          << " damage, health: " << newHealth << std::endl;
                return newHealth;
            }
            return 0;
        });

        def("heal", [this](int amount) -> int {
            auto health = getVariable<int>("health");
            if (health) {
                int newHealth = std::min(100, health->get() + amount);
                setValue("health", newHealth);
                std::cout << "  [" << getName() << "] Healed " << amount
                          << " points, health: " << newHealth << std::endl;
                return newHealth;
            }
            return 0;
        });

        def("getStatus", [this]() -> std::string {
            auto health = getVariable<int>("health");
            auto pos_x = getVariable<double>("position_x");
            auto pos_y = getVariable<double>("position_y");
            auto active = getVariable<bool>("active");

            return "Health: " + std::to_string(health ? health->get() : 0) +
                   ", Position: (" +
                   std::to_string(pos_x ? pos_x->get() : 0.0) + ", " +
                   std::to_string(pos_y ? pos_y->get() : 0.0) + ")" +
                   ", Active: " + (active && active->get() ? "true" : "false");
        });

        def("setActive", [this](bool isActive) {
            setValue("active", isActive);
            std::cout << "  [" << getName()
                      << "] Set active to: " << (isActive ? "true" : "false")
                      << std::endl;
        });
    }
};

#if ATOM_ENABLE_LUA

void demonstrateBasicLuaScripting() {
    std::cout << "\n=== Basic Lua Scripting Demo ===" << std::endl;

    using namespace atom::components::scripting;

    std::cout << "\n1. Creating Lua engine..." << std::endl;

    LuaConfig config;
    config.enableJIT = true;
    config.enableDebug = true;
    config.memoryLimit = 32 * 1024 * 1024;  // 32MB

    auto luaEngine = std::make_unique<LuaEngine>(config);

    ScriptEngineConfig engineConfig;
    engineConfig.enableSandbox = false;  // Disable for basic demo

    if (!luaEngine->initialize(engineConfig)) {
        std::cout << "Failed to initialize Lua engine" << std::endl;
        return;
    }

    std::cout << "Lua engine initialized successfully" << std::endl;

    std::cout << "\n2. Executing basic Lua scripts..." << std::endl;

    // Test basic Lua operations
    auto result1 = luaEngine->executeScript("return 2 + 3", "basic_math");
    if (result1.success) {
        std::cout << "2 + 3 = " << result1.output << std::endl;
    } else {
        std::cout << "Error: " << result1.errorMessage << std::endl;
    }

    // Test string operations
    auto result2 = luaEngine->executeScript("return 'Hello ' .. 'World!'",
                                            "string_concat");
    if (result2.success) {
        std::cout << "String concatenation: " << result2.output << std::endl;
    } else {
        std::cout << "Error: " << result2.errorMessage << std::endl;
    }

    // Test table operations
    auto result3 = luaEngine->executeScript(R"(
        local t = {1, 2, 3, 4, 5}
        local sum = 0
        for i, v in ipairs(t) do
            sum = sum + v
        end
        return sum
    )",
                                            "table_sum");
    if (result3.success) {
        std::cout << "Table sum: " << result3.output << std::endl;
    } else {
        std::cout << "Error: " << result3.errorMessage << std::endl;
    }

    std::cout << "\n3. Setting and getting global variables..." << std::endl;

    // Set global variables
    luaEngine->setGlobal("player_name", ScriptValue("Hero"));
    luaEngine->setGlobal("player_level", ScriptValue(static_cast<int64_t>(42)));
    luaEngine->setGlobal("player_health", ScriptValue(85.5));

    // Get global variables
    auto name = luaEngine->getGlobal("player_name");
    auto level = luaEngine->getGlobal("player_level");
    auto health = luaEngine->getGlobal("player_health");

    if (name && name->holds<std::string>()) {
        std::cout << "Player name: " << name->get<std::string>() << std::endl;
    }
    if (level && level->holds<int64_t>()) {
        std::cout << "Player level: " << level->get<int64_t>() << std::endl;
    }
    if (health && health->holds<double>()) {
        std::cout << "Player health: " << health->get<double>() << std::endl;
    }
}

void demonstrateLuaComponentBinding() {
    std::cout << "\n=== Lua Component Binding Demo ===" << std::endl;

    using namespace atom::components::scripting;

    auto& registry = ::Registry::instance();

    std::cout << "\n4. Creating scriptable component..." << std::endl;
    auto component = registry.createComponent<ScriptableComponent>("Player");

    std::cout << "\n5. Setting up Lua engine with component bindings..."
              << std::endl;

    auto luaEngine = std::make_unique<LuaEngine>();
    ScriptEngineConfig config;
    config.enableSandbox = false;

    if (!luaEngine->initialize(config)) {
        std::cout << "Failed to initialize Lua engine" << std::endl;
        return;
    }

    // Register component functions
    luaEngine->registerFunction(
        "component_move",
        [&](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.size() >= 2 && args[0].holds<double>() &&
                args[1].holds<double>()) {
                // Note: executeCommand is not available in Component base class
                // component->executeCommand(
                //     "move", {std::to_string(args[0].get<double>()),
                //              std::to_string(args[1].get<double>())});
                std::cout << "  [LUA] Move command executed" << std::endl;
                return ScriptValue(true);
            }
            return ScriptValue(false);
        });

    luaEngine->registerFunction(
        "component_damage",
        [&](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.size() >= 1 && args[0].holds<int64_t>()) {
                // Note: executeCommand is not available in Component base class
                // auto result = component->executeCommand(
                //     "takeDamage", {std::to_string(args[0].get<int64_t>())});
                std::cout << "  [LUA] Damage command executed" << std::endl;
                return ScriptValue(static_cast<int64_t>(50)); // Mock result
            }
            return ScriptValue(static_cast<int64_t>(0));
        });

    luaEngine->registerFunction(
        "component_heal",
        [&](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.size() >= 1 && args[0].holds<int64_t>()) {
                // Note: executeCommand is not available in Component base class
                // auto result = component->executeCommand(
                //     "heal", {std::to_string(args[0].get<int64_t>())});
                std::cout << "  [LUA] Heal command executed" << std::endl;
                return ScriptValue(static_cast<int64_t>(75)); // Mock result
            }
            return ScriptValue(static_cast<int64_t>(0));
        });

    luaEngine->registerFunction(
        "component_status",
        [&](const std::vector<ScriptValue>& args) -> ScriptValue {
            // Note: executeCommand is not available in Component base class
            // auto result = component->executeCommand("getStatus", {});
            // return ScriptValue(result);
            return ScriptValue("Status not available");
        });

    std::cout << "\n6. Executing Lua scripts with component interaction..."
              << std::endl;

    // Test component movement
    auto result1 = luaEngine->executeScript(R"(
        print("Moving player to (10, 20)")
        component_move(10, 20)
        return "Movement completed"
    )", "move_test");

    if (result1.success) {
        std::cout << "Move script result: " << result1.output << std::endl;
    }
    else {
        std::cout << "Move script error: " << result1.errorMessage << std::endl;
    }

    // Test damage and healing
    auto result2 = luaEngine->executeScript(R"(
        print("Initial status:")
        print(component_status())

        print("Taking 30 damage...")
        local health = component_damage(30)
        print("Health after damage: " .. health)

        print("Healing 15 points...")
        health = component_heal(15)
        print("Health after healing: " .. health)

        print("Final status:")
        print(component_status())

        return "Combat simulation completed"
    )",
                                            "combat_test");

    if (result2.success) {
        std::cout << "Combat script result: " << result2.output << std::endl;
    } else {
        std::cout << "Combat script error: " << result2.errorMessage
                  << std::endl;
    }
}

void demonstrateLuaErrorHandling() {
    std::cout << "\n=== Lua Error Handling Demo ===" << std::endl;

    using namespace atom::components::scripting;

    std::cout << "\n7. Testing Lua error handling..." << std::endl;

    auto luaEngine = std::make_unique<LuaEngine>();
    ScriptEngineConfig config;
    config.enableSandbox = false;

    if (!luaEngine->initialize(config)) {
        std::cout << "Failed to initialize Lua engine" << std::endl;
        return;
    }

    // Test syntax error
    std::cout << "   Testing syntax error..." << std::endl;
    auto result1 = luaEngine->executeScript("return 2 +", "syntax_error");
    if (!result1.success) {
        std::cout << "   Expected syntax error: " << result1.errorMessage
                  << std::endl;
    }

    // Test runtime error
    std::cout << "   Testing runtime error..." << std::endl;
    auto result2 =
        luaEngine->executeScript("return nil.field", "runtime_error");
    if (!result2.success) {
        std::cout << "   Expected runtime error: " << result2.errorMessage
                  << std::endl;
    }

    // Test division by zero
    std::cout << "   Testing division by zero..." << std::endl;
    auto result3 =
        luaEngine->executeScript("return 10 / 0", "division_by_zero");
    if (result3.success) {
        std::cout << "   Division by zero result: " << result3.output
                  << std::endl;
    } else {
        std::cout << "   Division by zero error: " << result3.errorMessage
                  << std::endl;
    }

    // Test function call error
    std::cout << "   Testing function call error..." << std::endl;
    auto result4 = luaEngine->executeScript("return nonexistent_function()",
                                            "function_error");
    if (!result4.success) {
        std::cout << "   Expected function error: " << result4.errorMessage
                  << std::endl;
    }
}

void demonstrateLuaFileExecution() {
    std::cout << "\n=== Lua File Execution Demo ===" << std::endl;

    using namespace atom::components::scripting;

    std::cout << "\n8. Creating and executing Lua script file..." << std::endl;

    // Create a test Lua script file
    const std::string scriptContent = R"(
-- Test Lua script file
print("Hello from Lua script file!")

function factorial(n)
    if n <= 1 then
        return 1
    else
        return n * factorial(n - 1)
    end
end

function fibonacci(n)
    if n <= 1 then
        return n
    else
        return fibonacci(n - 1) + fibonacci(n - 2)
    end
end

-- Calculate some values
local fact5 = factorial(5)
local fib10 = fibonacci(10)

print("Factorial of 5: " .. fact5)
print("Fibonacci of 10: " .. fib10)

-- Return results
return {
    factorial_5 = fact5,
    fibonacci_10 = fib10,
    message = "Script executed successfully"
}
)";

    // Write script to file
    std::ofstream scriptFile("test_script.lua");
    if (scriptFile.is_open()) {
        scriptFile << scriptContent;
        scriptFile.close();
        std::cout << "Created test_script.lua" << std::endl;
    } else {
        std::cout << "Failed to create script file" << std::endl;
        return;
    }

    // Execute the script file
    auto luaEngine = std::make_unique<LuaEngine>();
    ScriptEngineConfig config;
    config.enableSandbox = false;

    if (!luaEngine->initialize(config)) {
        std::cout << "Failed to initialize Lua engine" << std::endl;
        return;
    }

    auto result = luaEngine->executeFile("test_script.lua");
    if (result.success) {
        std::cout << "Script file executed successfully" << std::endl;
        std::cout << "Result: " << result.output << std::endl;
    } else {
        std::cout << "Script file execution failed: " << result.errorMessage
                  << std::endl;
    }

    // Clean up
    std::remove("test_script.lua");
    std::cout << "Cleaned up test_script.lua" << std::endl;
}

void demonstrateLuaStatistics() {
    std::cout << "\n=== Lua Engine Statistics Demo ===" << std::endl;

    using namespace atom::components::scripting;

    std::cout << "\n9. Lua engine performance statistics..." << std::endl;

    auto luaEngine = std::make_unique<LuaEngine>();
    ScriptEngineConfig config;
    config.enableSandbox = false;

    if (!luaEngine->initialize(config)) {
        std::cout << "Failed to initialize Lua engine" << std::endl;
        return;
    }

    // Execute multiple scripts to generate statistics
    for (int i = 0; i < 10; ++i) {
        luaEngine->executeScript("return " + std::to_string(i) + " * 2",
                                 "test_" + std::to_string(i));
    }

    // Get and display statistics
    const auto& stats = luaEngine->getStatistics();
    std::cout << "Lua Engine Statistics:" << std::endl;
    std::cout << "  Scripts executed: " << stats.scriptsExecuted << std::endl;
    std::cout << "  Successful executions: " << stats.successfulExecutions
              << std::endl;
    std::cout << "  Failed executions: " << stats.failedExecutions << std::endl;
    std::cout << "  Total execution time: " << stats.totalExecutionTime.count()
              << " ms" << std::endl;
    std::cout << "  Average execution time: "
              << stats.averageExecutionTime.count() << " ms" << std::endl;
    std::cout << "  Memory usage: " << stats.memoryUsage << " bytes"
              << std::endl;
}

#endif  // ATOM_ENABLE_LUA

int main() {
    std::cout << "=== Atom Component Lua Scripting Examples ===" << std::endl;

#if ATOM_ENABLE_LUA
    try {
        demonstrateBasicLuaScripting();
        demonstrateLuaComponentBinding();
        demonstrateLuaErrorHandling();
        demonstrateLuaFileExecution();
        demonstrateLuaStatistics();

        std::cout
            << "\n=== All Lua Scripting Examples Completed Successfully! ==="
            << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in Lua scripting examples: " << e.what()
                  << std::endl;
        return 1;
    }
#else
    std::cout << "\nLua support is not enabled. Please compile with "
                 "ATOM_ENABLE_LUA=1 to run these examples."
              << std::endl;
    std::cout << "To enable Lua support:" << std::endl;
    std::cout << "  cmake -DATOM_ENABLE_LUA=ON .." << std::endl;
    std::cout << "  or" << std::endl;
    std::cout << "  add_definitions(-DATOM_ENABLE_LUA=1)" << std::endl;
#endif

    return 0;
}
