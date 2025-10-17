/*
 * unified_scripting_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Unified Scripting API Example
Demonstrates cross-language scripting interface, engine abstraction,
and unified scripting features that work across multiple scripting engines.

**************************************************/

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/core/registry.hpp"
#include "atom/components/scripting/scripting_api.hpp"
#include <any>

// Conditional includes for scripting engines
#if ATOM_ENABLE_LUA
#include "atom/components/scripting/lua_engine.hpp"
#endif

#if ATOM_ENABLE_PYTHON
#include "atom/components/scripting/python_engine.hpp"
#endif

// Note: Component and Registry are in global namespace, not atom::components
using namespace atom::components::scripting;

/**
 * @brief Component that can be controlled via any scripting engine
 */
class UniversalScriptableComponent : public Component {
public:
    explicit UniversalScriptableComponent(const std::string& name)
        : Component(name) {
        std::cout << "UniversalScriptableComponent '" << name << "' created"
                  << std::endl;

        // Add variables accessible from any scripting language
        addVariable<int>("level", 1);
        addVariable<std::string>("character_name", "Universal Hero");
        addVariable<double>("experience", 0.0);
        addVariable<bool>("is_alive", true);
        addVariable<std::vector<std::string>>("skills",
                                              {"basic_attack", "defend"});

        // Add commands callable from any scripting language
        def("levelUp", [this]() -> int {
            auto level = getVariable<int>("level");
            if (level) {
                int newLevel = level->get() + 1;
                setValue("level", newLevel);
                std::cout << "  [" << getName() << "] Leveled up to "
                          << newLevel << std::endl;
                return newLevel;
            }
            return 1;
        });

        def("gainExperience", [this](double exp) -> double {
            auto experience = getVariable<double>("experience");
            if (experience) {
                double newExp = experience->get() + exp;
                setValue("experience", newExp);
                std::cout << "  [" << getName() << "] Gained " << exp
                          << " experience, total: " << newExp << std::endl;

                // Auto level up every 100 experience
                if (static_cast<int>(newExp / 100) >
                    static_cast<int>(experience->get() / 100)) {
                    [[maybe_unused]] auto result = runCommand("levelUp", {});
                }

                return newExp;
            }
            return 0.0;
        });

        def("learnSkill", [this](const std::string& skill) -> bool {
            auto skills = getVariable<std::vector<std::string>>("skills");
            if (skills) {
                auto skillList = skills->get();

                // Check if skill already known
                for (const auto& existingSkill : skillList) {
                    if (existingSkill == skill) {
                        std::cout << "  [" << getName()
                                  << "] Already knows skill: " << skill
                                  << std::endl;
                        return false;
                    }
                }

                skillList.push_back(skill);
                setValue("skills", skillList);
                std::cout << "  [" << getName()
                          << "] Learned new skill: " << skill << std::endl;
                return true;
            }
            return false;
        });

        def("getSkills", [this]() -> std::vector<std::string> {
            auto skills = getVariable<std::vector<std::string>>("skills");
            return skills ? skills->get() : std::vector<std::string>{};
        });

        def("getCharacterInfo", [this]() -> std::string {
            auto name = getVariable<std::string>("character_name");
            auto level = getVariable<int>("level");
            auto exp = getVariable<double>("experience");
            auto alive = getVariable<bool>("is_alive");

            return "Character: " + (name ? name->get() : "Unknown") +
                   ", Level: " + std::to_string(level ? level->get() : 1) +
                   ", Experience: " + std::to_string(exp ? exp->get() : 0.0) +
                   ", Status: " + (alive && alive->get() ? "Alive" : "Dead");
        });

        def("performAction",
            [this](const std::string& action, double intensity) -> std::string {
                std::cout << "  [" << getName()
                          << "] Performing action: " << action
                          << " with intensity: " << intensity << std::endl;

                // Simulate action results
                if (action == "attack") {
                    double damage = intensity * 10.0;
                    return "Dealt " + std::to_string(damage) + " damage";
                } else if (action == "heal") {
                    double healing = intensity * 5.0;
                    return "Healed " + std::to_string(healing) + " health";
                } else if (action == "cast_spell") {
                    double manaCost = intensity * 15.0;
                    return "Cast spell consuming " + std::to_string(manaCost) +
                           " mana";
                } else {
                    return "Unknown action: " + action;
                }
            });
    }
};

/**
 * @brief Unified scripting manager that abstracts different engines
 */
class UnifiedScriptingManager {
public:
    UnifiedScriptingManager() = default;

    void registerEngine(const std::string& name,
                        std::unique_ptr<IScriptEngine> engine) {
        engines_[name] = std::move(engine);
        std::cout << "Registered scripting engine: " << name << std::endl;
    }

    IScriptEngine* getEngine(const std::string& name) {
        auto it = engines_.find(name);
        return (it != engines_.end()) ? it->second.get() : nullptr;
    }

    std::vector<std::string> getAvailableEngines() const {
        std::vector<std::string> names;
        for (const auto& pair : engines_) {
            names.push_back(pair.first);
        }
        return names;
    }

    void registerUniversalFunction(const std::string& name,
                                   ScriptFunction function) {
        universalFunctions_[name] = function;

        // Register with all engines
        for (auto& pair : engines_) {
            pair.second->registerFunction(name, function);
        }

        std::cout << "Registered universal function: " << name << std::endl;
    }

    ScriptResult executeOnEngine(const std::string& engineName,
                                 const std::string& script,
                                 const std::string& context = "") {
        auto engine = getEngine(engineName);
        if (!engine) {
            ScriptResult result;
            result.success = false;
            result.errorMessage = "Engine not found: " + engineName;
            return result;
        }

        return engine->executeScript(script, context);
    }

    void executeOnAllEngines(const std::string& script,
                             const std::string& context = "") {
        for (const auto& pair : engines_) {
            std::cout << "\nExecuting on " << pair.first
                      << " engine:" << std::endl;
            auto result = pair.second->executeScript(script, context);
            if (result.success) {
                std::cout << "  Result: " << result.returnValue.get<std::string>() << std::endl;
            } else {
                std::cout << "  Error: " << result.errorMessage << std::endl;
            }
        }
    }

    void printEngineStatistics() {
        std::cout << "\n=== Engine Statistics ===" << std::endl;
        for (const auto& pair : engines_) {
            const auto& stats = pair.second->getStatistics();
            std::cout << "\n" << pair.first << " Engine:" << std::endl;
            std::cout << "  Scripts executed: " << stats.scriptsExecuted
                      << std::endl;
            std::cout << "  Scripts executed: " << stats.scriptsExecuted
                      << std::endl;
            std::cout << "  Errors encountered: " << stats.errorsEncountered << std::endl;
            std::cout << "  Total time: " << stats.totalExecutionTime.count()
                      << " ms" << std::endl;
            std::cout << "  Average time: "
                      << (stats.totalExecutionTime.count() / std::max(1ULL, stats.scriptsExecuted)) << " μs"
                      << std::endl;
        }
    }

private:
    std::map<std::string, std::unique_ptr<IScriptEngine>> engines_;
    std::map<std::string, ScriptFunction> universalFunctions_;
};

void setupUnifiedScriptingManager(
    UnifiedScriptingManager& manager,
    std::shared_ptr<UniversalScriptableComponent> component) {
    std::cout << "\n=== Setting up Unified Scripting Manager ===" << std::endl;

    // Initialize available scripting engines
    ScriptEngineConfig config;
    config.enableSandbox = false;

#if ATOM_ENABLE_LUA
    std::cout << "\n1. Initializing Lua engine..." << std::endl;
    auto luaEngine = std::make_unique<LuaEngine>();
    if (luaEngine->initialize(config)) {
        manager.registerEngine("lua", std::move(luaEngine));
        std::cout << "   Lua engine registered successfully" << std::endl;
    } else {
        std::cout << "   Failed to initialize Lua engine" << std::endl;
    }
#endif

#if ATOM_ENABLE_PYTHON
    std::cout << "\n2. Initializing Python engine..." << std::endl;
    auto pythonEngine = std::make_unique<PythonEngine>();
    if (pythonEngine->initialize(config)) {
        manager.registerEngine("python", std::move(pythonEngine));
        std::cout << "   Python engine registered successfully" << std::endl;
    } else {
        std::cout << "   Failed to initialize Python engine" << std::endl;
    }
#endif

    std::cout << "\n3. Registering universal functions..." << std::endl;

    // Register component functions that work across all engines
    manager.registerUniversalFunction(
        "level_up",
        [component](const std::vector<ScriptValue>& args) -> ScriptValue {
            auto result = component->runCommand("levelUp", {});
            return ScriptValue(static_cast<int64_t>(std::stoi(std::any_cast<std::string>(result))));
        });

    manager.registerUniversalFunction(
        "gain_exp",
        [component](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.size() >= 1 && args[0].holds<double>()) {
                std::vector<std::any> expArgs = {std::any(std::to_string(args[0].get<double>()))};
                auto result = component->runCommand("gainExperience", expArgs);
                return ScriptValue(std::stod(std::any_cast<std::string>(result)));
            }
            return ScriptValue(0.0);
        });

    manager.registerUniversalFunction(
        "learn_skill",
        [component](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.size() >= 1 && args[0].holds<std::string>()) {
                std::vector<std::any> skillArgs = {std::any(args[0].get<std::string>())};
                auto result = component->runCommand("learnSkill", skillArgs);
                return ScriptValue(std::any_cast<std::string>(result) == "1");
            }
            return ScriptValue(false);
        });

    manager.registerUniversalFunction(
        "get_info",
        [component](const std::vector<ScriptValue>& args) -> ScriptValue {
            auto result = component->runCommand("getCharacterInfo", {});
            return ScriptValue(std::any_cast<std::string>(result));
        });

    manager.registerUniversalFunction(
        "perform_action",
        [component](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.size() >= 2 && args[0].holds<std::string>() &&
                args[1].holds<double>()) {
                std::vector<std::any> actionArgs = {
                    std::any(args[0].get<std::string>()),
                    std::any(std::to_string(args[1].get<double>()))
                };
                auto result = component->runCommand("performAction", actionArgs);
                return ScriptValue(std::any_cast<std::string>(result));
            }
            return ScriptValue("Invalid arguments");
        });

    auto availableEngines = manager.getAvailableEngines();
    std::cout << "Available engines: ";
    for (const auto& engine : availableEngines) {
        std::cout << engine << " ";
    }
    std::cout << std::endl;
}

void demonstrateUnifiedScripting(UnifiedScriptingManager& manager) {
    std::cout << "\n=== Unified Scripting Demo ===" << std::endl;

    std::cout << "\n4. Testing universal functions across engines..."
              << std::endl;

    // Test scripts that should work on all engines
    std::vector<std::pair<std::string, std::string>> testScripts = {
        {"Basic Info", "get_info()"},
        {"Level Up", "level_up()"},
        {"Gain Experience", "gain_exp(75.5)"},
        {"Learn Skill", "learn_skill('fireball')"},
        {"Perform Action", "perform_action('attack', 2.5)"}};

    for (const auto& test : testScripts) {
        std::cout << "\n--- Testing: " << test.first << " ---" << std::endl;
        manager.executeOnAllEngines(test.second, test.first);
    }
}

void demonstrateLanguageSpecificFeatures(UnifiedScriptingManager& manager) {
    std::cout << "\n=== Language-Specific Features Demo ===" << std::endl;

    std::cout << "\n5. Testing language-specific features..." << std::endl;

#if ATOM_ENABLE_LUA
    std::cout << "\n--- Lua-specific features ---" << std::endl;
    auto luaResult = manager.executeOnEngine("lua", R"(
        -- Lua table manipulation
        local character_data = {
            name = "Lua Hero",
            stats = {strength = 10, agility = 8, intelligence = 12}
        }

        -- Calculate total stats
        local total = 0
        for stat, value in pairs(character_data.stats) do
            total = total + value
        end

        return "Total stats: " .. total
    )",
                                             "lua_tables");

    if (luaResult.success) {
        std::cout << "Lua result: " << luaResult.returnValue.get<std::string>() << std::endl;
    } else {
        std::cout << "Lua error: " << luaResult.errorMessage << std::endl;
    }
#endif

#if ATOM_ENABLE_PYTHON
    std::cout << "\n--- Python-specific features ---" << std::endl;
    auto pythonResult = manager.executeOnEngine("python", R"(
# Python list comprehension and dictionary
character_data = {
    'name': 'Python Hero',
    'stats': {'strength': 10, 'agility': 8, 'intelligence': 12}
}

# Calculate total stats using sum and values
total_stats = sum(character_data['stats'].values())

# Create skill list using list comprehension
skills = [f"skill_{i}" for i in range(1, 6)]

f"Total stats: {total_stats}, Skills: {len(skills)}"
    )",
                                                "python_comprehension");

    if (pythonResult.success) {
        std::cout << "Python result: " << pythonResult.returnValue.get<std::string>() << std::endl;
    } else {
        std::cout << "Python error: " << pythonResult.errorMessage << std::endl;
    }
#endif
}

void demonstrateScriptValueConversion(UnifiedScriptingManager& manager) {
    std::cout << "\n=== Script Value Conversion Demo ===" << std::endl;

    std::cout << "\n6. Testing cross-language value conversion..." << std::endl;

    // Test different data types
    auto engines = manager.getAvailableEngines();

    for (const auto& engineName : engines) {
        auto engine = manager.getEngine(engineName);
        if (!engine)
            continue;

        std::cout << "\n--- Testing " << engineName << " value conversion ---"
                  << std::endl;

        // Set various types of global values
        engine->setGlobal("test_int", ScriptValue(static_cast<int64_t>(42)));
        engine->setGlobal("test_double", ScriptValue(3.14159));
        engine->setGlobal("test_string", ScriptValue("Hello World"));
        engine->setGlobal("test_bool", ScriptValue(true));

        // Test retrieving and using these values
        std::string testScript;
        if (engineName == "lua") {
            testScript = R"(
                return string.format("Int: %d, Double: %.2f, String: %s, Bool: %s",
                    test_int, test_double, test_string, tostring(test_bool))
            )";
        } else if (engineName == "python") {
            testScript = R"(
f"Int: {test_int}, Double: {test_double:.2f}, String: {test_string}, Bool: {test_bool}"
            )";
        }

        if (!testScript.empty()) {
            auto result = engine->executeScript(testScript, "value_conversion");
            if (result.success) {
                std::cout << "   Result: " << result.returnValue.get<std::string>() << std::endl;
            } else {
                std::cout << "   Error: " << result.errorMessage << std::endl;
            }
        }
    }
}

void demonstratePerformanceComparison(UnifiedScriptingManager& manager) {
    std::cout << "\n=== Performance Comparison Demo ===" << std::endl;

    std::cout << "\n7. Comparing engine performance..." << std::endl;

    // Test script that performs some computation
    const std::string computationScript = R"(
        local sum = 0
        for i = 1, 1000 do
            sum = sum + i * i
        end
        return sum
    )";

    const std::string pythonComputationScript = R"(
sum(i * i for i in range(1, 1001))
    )";

    const int iterations = 100;

    auto engines = manager.getAvailableEngines();
    for (const auto& engineName : engines) {
        auto engine = manager.getEngine(engineName);
        if (!engine)
            continue;

        std::cout << "\n--- Testing " << engineName << " performance ---"
                  << std::endl;

        // Reset statistics
        engine->resetStatistics();

        // Choose appropriate script for the engine
        std::string script = (engineName == "python") ? pythonComputationScript
                                                      : computationScript;

        // Run multiple iterations
        for (int i = 0; i < iterations; ++i) {
            engine->executeScript(script,
                                  "performance_test_" + std::to_string(i));
        }

        // Print statistics
        const auto& stats = engine->getStatistics();
        std::cout << "   Executed " << stats.scriptsExecuted << " scripts"
                  << std::endl;
        std::cout << "   Error rate: "
                  << (stats.errorsEncountered * 100.0 /
                      std::max(1ULL, stats.scriptsExecuted))
                  << "%" << std::endl;
        std::cout << "   Total time: " << stats.totalExecutionTime.count()
                  << " μs" << std::endl;
        std::cout << "   Average time: " << (stats.totalExecutionTime.count() / std::max(1ULL, stats.scriptsExecuted))
                  << " μs" << std::endl;
    }
}

int main() {
    std::cout << "=== Atom Component Unified Scripting Examples ==="
              << std::endl;

    try {
        // Create component and scripting manager
        auto& registry = ::Registry::instance();
        auto component = registry.createComponent<UniversalScriptableComponent>(
            "UniversalHero");

        UnifiedScriptingManager manager;

        setupUnifiedScriptingManager(manager, component);

        if (manager.getAvailableEngines().empty()) {
            std::cout << "\nNo scripting engines available. Please compile "
                         "with ATOM_ENABLE_LUA=1 and/or ATOM_ENABLE_PYTHON=1"
                      << std::endl;
            return 0;
        }

        demonstrateUnifiedScripting(manager);
        demonstrateLanguageSpecificFeatures(manager);
        demonstrateScriptValueConversion(manager);
        demonstratePerformanceComparison(manager);

        manager.printEngineStatistics();

        std::cout << "\n=== All Unified Scripting Examples Completed "
                     "Successfully! ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in unified scripting examples: " << e.what()
                  << std::endl;
        return 1;
    }

    return 0;
}
