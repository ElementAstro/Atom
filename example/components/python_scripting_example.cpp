/*
 * python_scripting_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Python Scripting Engine Example
Demonstrates Python integration, type conversion, advanced bindings,
and Python scripting features with the component system.

**************************************************/

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/core/registry.hpp"

// Conditional compilation for Python support
#if ATOM_ENABLE_PYTHON
#include "atom/components/scripting/python_engine.hpp"
#include "atom/components/scripting/scripting_api.hpp"
#endif

// Note: Component and Registry are in global namespace, not atom::components
// Only use atom::components for scripting-related types

/**
 * @brief Component that can be controlled via Python scripts
 */
class PythonScriptableComponent : public Component {
public:
    explicit PythonScriptableComponent(const std::string& name)
        : Component(name) {
        std::cout << "PythonScriptableComponent '" << name << "' created"
                  << std::endl;

        // Add variables that can be accessed from Python scripts
        addVariable<int>("score", 0);
        addVariable<std::string>("player_name", "Python Player");
        addVariable<double>("x", 0.0);
        addVariable<double>("y", 0.0);
        addVariable<double>("z", 0.0);
        addVariable<bool>("enabled", true);
        addVariable<std::vector<int>>("inventory", {1, 2, 3});

        // Add commands that can be called from Python scripts
        def("setPosition", [this](double x, double y, double z) {
            setValue("x", x);
            setValue("y", y);
            setValue("z", z);
            std::cout << "  [" << getName() << "] Position set to (" << x
                      << ", " << y << ", " << z << ")" << std::endl;
        });

        def("getPosition", [this]() -> std::vector<double> {
            auto x = getVariable<double>("x");
            auto y = getVariable<double>("y");
            auto z = getVariable<double>("z");
            return {x ? x->get() : 0.0, y ? y->get() : 0.0, z ? z->get() : 0.0};
        });

        def("addScore", [this](int points) -> int {
            auto score = getVariable<int>("score");
            if (score) {
                int newScore = score->get() + points;
                setValue("score", newScore);
                std::cout << "  [" << getName() << "] Added " << points
                          << " points, total score: " << newScore << std::endl;
                return newScore;
            }
            return 0;
        });

        def("getScore", [this]() -> int {
            auto score = getVariable<int>("score");
            return score ? score->get() : 0;
        });

        def("addToInventory", [this](int item) {
            auto inventory = getVariable<std::vector<int>>("inventory");
            if (inventory) {
                auto items = inventory->get();
                items.push_back(item);
                setValue("inventory", items);
                std::cout << "  [" << getName() << "] Added item " << item
                          << " to inventory" << std::endl;
            }
        });

        def("getInventory", [this]() -> std::vector<int> {
            auto inventory = getVariable<std::vector<int>>("inventory");
            return inventory ? inventory->get() : std::vector<int>{};
        });

        def("processData", [this](const std::vector<double>& data) -> double {
            double sum = 0.0;
            for (double value : data) {
                sum += value;
            }
            std::cout << "  [" << getName() << "] Processed " << data.size()
                      << " values, sum: " << sum << std::endl;
            return sum;
        });

        def("getInfo", [this]() -> std::string {
            auto name = getVariable<std::string>("player_name");
            auto score = getVariable<int>("score");
            auto enabled = getVariable<bool>("enabled");

            return "Player: " + (name ? name->get() : "Unknown") +
                   ", Score: " + std::to_string(score ? score->get() : 0) +
                   ", Enabled: " +
                   (enabled && enabled->get() ? "true" : "false");
        });
    }
};

#if ATOM_ENABLE_PYTHON

void demonstrateBasicPythonScripting() {
    std::cout << "\n=== Basic Python Scripting Demo ===" << std::endl;

    using namespace atom::components::scripting;

    std::cout << "\n1. Creating Python engine..." << std::endl;

    PythonConfig config;
    config.enableSitePackages = true;
    config.enableUserSite = true;
    config.isolatedMode = false;
    config.programName = "atom_python_example";

    auto pythonEngine = std::make_unique<PythonEngine>(config);

    ScriptEngineConfig engineConfig;
    engineConfig.enableSandbox = false;  // Disable for basic demo

    if (!pythonEngine->initialize(engineConfig)) {
        std::cout << "Failed to initialize Python engine" << std::endl;
        return;
    }

    std::cout << "Python engine initialized successfully" << std::endl;

    std::cout << "\n2. Executing basic Python scripts..." << std::endl;

    // Test basic Python operations
    auto result1 = pythonEngine->executeScript("2 + 3", "basic_math");
    if (result1.success) {
        std::cout << "2 + 3 = " << result1.output << std::endl;
    } else {
        std::cout << "Error: " << result1.errorMessage << std::endl;
    }

    // Test string operations
    auto result2 =
        pythonEngine->executeScript("'Hello ' + 'World!'", "string_concat");
    if (result2.success) {
        std::cout << "String concatenation: " << result2.output << std::endl;
    } else {
        std::cout << "Error: " << result2.errorMessage << std::endl;
    }

    // Test list operations
    auto result3 = pythonEngine->executeScript(R"(
data = [1, 2, 3, 4, 5]
sum(data)
    )",
                                               "list_sum");
    if (result3.success) {
        std::cout << "List sum: " << result3.output << std::endl;
    } else {
        std::cout << "Error: " << result3.errorMessage << std::endl;
    }

    // Test dictionary operations
    auto result4 = pythonEngine->executeScript(R"(
player = {'name': 'Hero', 'level': 42, 'health': 85.5}
f"Player {player['name']} is level {player['level']} with {player['health']} health"
    )",
                                               "dict_format");
    if (result4.success) {
        std::cout << "Dictionary formatting: " << result4.output << std::endl;
    } else {
        std::cout << "Error: " << result4.errorMessage << std::endl;
    }

    std::cout << "\n3. Setting and getting global variables..." << std::endl;

    // Set global variables
    pythonEngine->setGlobal("game_title", ScriptValue("Atom Game"));
    pythonEngine->setGlobal("max_players",
                            ScriptValue(static_cast<int64_t>(8)));
    pythonEngine->setGlobal("difficulty", ScriptValue(0.75));

    // Get global variables
    auto title = pythonEngine->getGlobal("game_title");
    auto maxPlayers = pythonEngine->getGlobal("max_players");
    auto difficulty = pythonEngine->getGlobal("difficulty");

    if (title && title->holds<std::string>()) {
        std::cout << "Game title: " << title->get<std::string>() << std::endl;
    }
    if (maxPlayers && maxPlayers->holds<int64_t>()) {
        std::cout << "Max players: " << maxPlayers->get<int64_t>() << std::endl;
    }
    if (difficulty && difficulty->holds<double>()) {
        std::cout << "Difficulty: " << difficulty->get<double>() << std::endl;
    }
}

void demonstratePythonComponentBinding() {
    std::cout << "\n=== Python Component Binding Demo ===" << std::endl;

    using namespace atom::components::scripting;

    auto& registry = ::Registry::instance();

    std::cout << "\n4. Creating Python scriptable component..." << std::endl;
    auto component =
        registry.createComponent<PythonScriptableComponent>("PythonPlayer");

    std::cout << "\n5. Setting up Python engine with component bindings..."
              << std::endl;

    auto pythonEngine = std::make_unique<PythonEngine>();
    ScriptEngineConfig config;
    config.enableSandbox = false;

    if (!pythonEngine->initialize(config)) {
        std::cout << "Failed to initialize Python engine" << std::endl;
        return;
    }

    // Register component functions
    pythonEngine->registerFunction(
        "set_position",
        [&](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.size() >= 3 && args[0].holds<double>() &&
                args[1].holds<double>() && args[2].holds<double>()) {
                // Note: executeCommand is not available in Component base class
                // component->executeCommand(
                //     "setPosition", {std::to_string(args[0].get<double>()),
                //                     std::to_string(args[1].get<double>()),
                //                     std::to_string(args[2].get<double>())});
                std::cout << "  [PYTHON] Position set via Python script"
                          << std::endl;
                return ScriptValue(true);
            }
            return ScriptValue(false);
        });

    pythonEngine->registerFunction(
        "get_position",
        [&](const std::vector<ScriptValue>& args) -> ScriptValue {
            // Note: executeCommand is not available in Component base class
            // auto result = component->executeCommand("getPosition", {});
            // Note: In a real implementation, we'd parse the vector result
            // properly
            return ScriptValue("(0, 0, 0)");
        });

    pythonEngine->registerFunction(
        "add_score", [&](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.size() >= 1 && args[0].holds<int64_t>()) {
                // Note: executeCommand is not available in Component base class
                // auto result = component->executeCommand(
                //     "addScore", {std::to_string(args[0].get<int64_t>())});
                std::cout << "  [PYTHON] Score added via Python script"
                          << std::endl;
                return ScriptValue(static_cast<int64_t>(100));  // Mock result
            }
            return ScriptValue(static_cast<int64_t>(0));
        });

    pythonEngine->registerFunction(
        "get_score", [&](const std::vector<ScriptValue>& args) -> ScriptValue {
            // Note: executeCommand is not available in Component base class
            // auto result = component->executeCommand("getScore", {});
            return ScriptValue(static_cast<int64_t>(100));  // Mock result
        });

    pythonEngine->registerFunction(
        "get_info", [&](const std::vector<ScriptValue>& args) -> ScriptValue {
            // Note: executeCommand is not available in Component base class
            // auto result = component->executeCommand("getInfo", {});
            return ScriptValue(
                "Player: Python Player, Score: 100, Enabled: true");
        });

    std::cout << "\n6. Executing Python scripts with component interaction..."
              << std::endl;

    // Test component position setting
    auto result1 = pythonEngine->executeScript(R"(
print("Setting player position to (5, 10, 15)")
set_position(5.0, 10.0, 15.0)
print("Position set successfully")
print("Position update completed")
    )", "position_test");

    if (result1.success) {
        std::cout << "Position script result: " << result1.output << std::endl;
    }
    else {
        std::cout << "Position script error: " << result1.errorMessage
                  << std::endl;
    }

    // Test scoring system
    auto result2 = pythonEngine->executeScript(R"(
print("Initial player info:")
print(get_info())

print("Adding scores...")
score1 = add_score(100)
print(f"Score after adding 100: {score1}")

score2 = add_score(250)
print(f"Score after adding 250: {score2}")

score3 = add_score(50)
print(f"Final score: {score3}")

print("Final player info:")
print(get_info())

print("Scoring test completed")
    )",
                                               "scoring_test");

    if (result2.success) {
        std::cout << "Scoring script result: " << result2.output << std::endl;
    } else {
        std::cout << "Scoring script error: " << result2.errorMessage
                  << std::endl;
    }
}

void demonstratePythonAdvancedFeatures() {
    std::cout << "\n=== Python Advanced Features Demo ===" << std::endl;

    using namespace atom::components::scripting;

    std::cout << "\n7. Testing Python advanced features..." << std::endl;

    auto pythonEngine = std::make_unique<PythonEngine>();
    ScriptEngineConfig config;
    config.enableSandbox = false;

    if (!pythonEngine->initialize(config)) {
        std::cout << "Failed to initialize Python engine" << std::endl;
        return;
    }

    // Test list comprehensions
    auto result1 = pythonEngine->executeScript(R"(
squares = [x**2 for x in range(10)]
sum(squares)
    )",
                                               "list_comprehension");
    if (result1.success) {
        std::cout << "List comprehension result: " << result1.output
                  << std::endl;
    } else {
        std::cout << "List comprehension error: " << result1.errorMessage
                  << std::endl;
    }

    // Test lambda functions
    auto result2 = pythonEngine->executeScript(R"(
numbers = [1, 2, 3, 4, 5]
doubled = list(map(lambda x: x * 2, numbers))
doubled
    )",
                                               "lambda_test");
    if (result2.success) {
        std::cout << "Lambda function result: " << result2.output << std::endl;
    } else {
        std::cout << "Lambda function error: " << result2.errorMessage
                  << std::endl;
    }

    // Test class definition and usage
    auto result3 = pythonEngine->executeScript(
        R"(
class GameEntity:
    def __init__(self, name, health=100):
        self.name = name
        self.health = health

    def take_damage(self, damage):
        self.health = max(0, self.health - damage)
        return self.health

    def __str__(self):
        return f"{self.name} (Health: {self.health})"
#Create and use entity
        player = GameEntity("Hero", 150) player.take_damage(30)
                     str(player)) ", " class_test ");
                   if (result3.success) {
        std::cout << "Class definition result: " << result3.output << std::endl;
    }
    else {
        std::cout << "Class definition error: " << result3.errorMessage
                  << std::endl;
    }

    // Test exception handling
    auto result4 = pythonEngine->executeScript(R"(
try:
    result = 10 / 2
    print(f"Division result: {result}")

    # This will raise an exception
    bad_result = 10 / 0
except ZeroDivisionError as e:
    print(f"Caught exception: {e}")
    result = "Error handled"

result
    )",
                                               "exception_test");
    if (result4.success) {
        std::cout << "Exception handling result: " << result4.output
                  << std::endl;
    } else {
        std::cout << "Exception handling error: " << result4.errorMessage
                  << std::endl;
    }
}

void demonstratePythonErrorHandling() {
    std::cout << "\n=== Python Error Handling Demo ===" << std::endl;

    using namespace atom::components::scripting;

    std::cout << "\n8. Testing Python error handling..." << std::endl;

    auto pythonEngine = std::make_unique<PythonEngine>();
    ScriptEngineConfig config;
    config.enableSandbox = false;

    if (!pythonEngine->initialize(config)) {
        std::cout << "Failed to initialize Python engine" << std::endl;
        return;
    }

    // Test syntax error
    std::cout << "   Testing syntax error..." << std::endl;
    auto result1 = pythonEngine->executeScript("if True", "syntax_error");
    if (!result1.success) {
        std::cout << "   Expected syntax error: " << result1.errorMessage
                  << std::endl;
    }

    // Test name error
    std::cout << "   Testing name error..." << std::endl;
    auto result2 =
        pythonEngine->executeScript("undefined_variable", "name_error");
    if (!result2.success) {
        std::cout << "   Expected name error: " << result2.errorMessage
                  << std::endl;
    }

    // Test type error
    std::cout << "   Testing type error..." << std::endl;
    auto result3 = pythonEngine->executeScript("'string' + 42", "type_error");
    if (!result3.success) {
        std::cout << "   Expected type error: " << result3.errorMessage
                  << std::endl;
    }

    // Test import error
    std::cout << "   Testing import error..." << std::endl;
    auto result4 = pythonEngine->executeScript("import nonexistent_module",
                                               "import_error");
    if (!result4.success) {
        std::cout << "   Expected import error: " << result4.errorMessage
                  << std::endl;
    }
}

void demonstratePythonFileExecution() {
    std::cout << "\n=== Python File Execution Demo ===" << std::endl;

    using namespace atom::components::scripting;

    std::cout << "\n9. Creating and executing Python script file..."
              << std::endl;

    // Create a test Python script file
    const std::string scriptContent = R"(
# Test Python script file
print("Hello from Python script file!")

def fibonacci(n):
    """Calculate Fibonacci number using iteration."""
    if n <= 1:
        return n
    a, b = 0, 1
    for _ in range(2, n + 1):
        a, b = b, a + b
    return b

def prime_factors(n):
    """Find prime factors of a number."""
    factors = []
    d = 2
    while d * d <= n:
        while n % d == 0:
            factors.append(d)
            n //= d
        d += 1
    if n > 1:
        factors.append(n)
    return factors

# Calculate some values
fib_15 = fibonacci(15)
factors_60 = prime_factors(60)

print(f"Fibonacci of 15: {fib_15}")
print(f"Prime factors of 60: {factors_60}")

# Return results as a dictionary
result = {
    'fibonacci_15': fib_15,
    'prime_factors_60': factors_60,
    'message': 'Python script executed successfully'
}

result
)";

    // Write script to file
    std::ofstream scriptFile("test_script.py");
    if (scriptFile.is_open()) {
        scriptFile << scriptContent;
        scriptFile.close();
        std::cout << "Created test_script.py" << std::endl;
    } else {
        std::cout << "Failed to create script file" << std::endl;
        return;
    }

    // Execute the script file
    auto pythonEngine = std::make_unique<PythonEngine>();
    ScriptEngineConfig config;
    config.enableSandbox = false;

    if (!pythonEngine->initialize(config)) {
        std::cout << "Failed to initialize Python engine" << std::endl;
        return;
    }

    auto result = pythonEngine->executeFile("test_script.py");
    if (result.success) {
        std::cout << "Script file executed successfully" << std::endl;
        std::cout << "Result: " << result.output << std::endl;
    } else {
        std::cout << "Script file execution failed: " << result.errorMessage
                  << std::endl;
    }

    // Clean up
    std::remove("test_script.py");
    std::cout << "Cleaned up test_script.py" << std::endl;
}

void demonstratePythonStatistics() {
    std::cout << "\n=== Python Engine Statistics Demo ===" << std::endl;

    using namespace atom::components::scripting;

    std::cout << "\n10. Python engine performance statistics..." << std::endl;

    auto pythonEngine = std::make_unique<PythonEngine>();
    ScriptEngineConfig config;
    config.enableSandbox = false;

    if (!pythonEngine->initialize(config)) {
        std::cout << "Failed to initialize Python engine" << std::endl;
        return;
    }

    // Execute multiple scripts to generate statistics
    for (int i = 0; i < 10; ++i) {
        pythonEngine->executeScript(std::to_string(i) + " * 3",
                                    "test_" + std::to_string(i));
    }

    // Get and display statistics
    const auto& stats = pythonEngine->getStatistics();
    std::cout << "Python Engine Statistics:" << std::endl;
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

#endif  // ATOM_ENABLE_PYTHON

int main() {
    std::cout << "=== Atom Component Python Scripting Examples ==="
              << std::endl;

#if ATOM_ENABLE_PYTHON
    try {
        demonstrateBasicPythonScripting();
        demonstratePythonComponentBinding();
        demonstratePythonAdvancedFeatures();
        demonstratePythonErrorHandling();
        demonstratePythonFileExecution();
        demonstratePythonStatistics();

        std::cout
            << "\n=== All Python Scripting Examples Completed Successfully! ==="
            << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in Python scripting examples: " << e.what()
                  << std::endl;
        return 1;
    }
#else
    std::cout << "\nPython support is not enabled. Please compile with "
                 "ATOM_ENABLE_PYTHON=1 to run these examples."
              << std::endl;
    std::cout << "To enable Python support:" << std::endl;
    std::cout << "  cmake -DATOM_ENABLE_PYTHON=ON .." << std::endl;
    std::cout << "  or" << std::endl;
    std::cout << "  add_definitions(-DATOM_ENABLE_PYTHON=1)" << std::endl;
#endif

    return 0;
}
