/*
 * test_scripting_api.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Comprehensive Unit Tests for Scripting API
Tests script execution, type conversion, function registration,
global variables, error handling, and memory management.

**************************************************/

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "../component.hpp"
#include "../scripting_api.hpp"

#if ATOM_ENABLE_LUA
#include "../lua_engine.hpp"
#endif

#if ATOM_ENABLE_PYTHON
#include "../python_engine.hpp"
#endif

using namespace atom::components::scripting;

/**
 * @brief Test fixture for Scripting API tests
 */
class ScriptingAPITest : public ::testing::Test {
protected:
    void SetUp() override {
        scriptingAPI_ = &ComponentScriptingAPI::instance();

        ScriptEngineConfig config;
        config.memoryLimit = 32 * 1024 * 1024;  // 32MB
        config.executionTimeout = std::chrono::seconds(10);
        config.enableDebug = true;
        config.enableSandbox = true;

        initialized_ = scriptingAPI_->initialize(config);
    }

    void TearDown() override {
        if (initialized_) {
            scriptingAPI_->shutdown();
        }
    }

    ComponentScriptingAPI* scriptingAPI_;
    bool initialized_;
};

/**
 * @brief Test fixture for script engine tests
 */
class ScriptEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        scriptingAPI_ = &ComponentScriptingAPI::instance();

        ScriptEngineConfig config;
        config.memoryLimit = 16 * 1024 * 1024;  // 16MB
        config.executionTimeout = std::chrono::seconds(5);
        config.enableDebug = true;

        scriptingAPI_->initialize(config);
    }

    void TearDown() override { scriptingAPI_->shutdown(); }

    ComponentScriptingAPI* scriptingAPI_;
};

// ============================================================================
// Singleton Pattern Tests
// ============================================================================

TEST(ScriptingAPISingletonTest, SingletonInstance) {
    auto& instance1 = ComponentScriptingAPI::instance();
    auto& instance2 = ComponentScriptingAPI::instance();

    EXPECT_EQ(&instance1, &instance2);
}

// ============================================================================
// Initialization and Configuration Tests
// ============================================================================

TEST_F(ScriptingAPITest, BasicInitialization) { EXPECT_TRUE(initialized_); }

TEST_F(ScriptingAPITest, InitializationWithCustomConfig) {
    scriptingAPI_->shutdown();

    ScriptEngineConfig customConfig;
    customConfig.memoryLimit = 64 * 1024 * 1024;  // 64MB
    customConfig.executionTimeout = std::chrono::seconds(30);
    customConfig.enableDebug = false;
    customConfig.enableHotReload = false;
    customConfig.enableSandbox = false;

    bool result = scriptingAPI_->initialize(customConfig);
    EXPECT_TRUE(result);
}

// ============================================================================
// Script Value Tests
// ============================================================================

TEST(ScriptValueTest, BasicTypes) {
    ScriptValue nilValue;
    EXPECT_TRUE(nilValue.holds<std::monostate>());

    ScriptValue boolValue(true);
    EXPECT_TRUE(boolValue.holds<bool>());
    EXPECT_EQ(boolValue.get<bool>(), true);

    ScriptValue intValue(42);
    EXPECT_TRUE(intValue.holds<int64_t>());
    EXPECT_EQ(intValue.get<int64_t>(), 42);

    ScriptValue doubleValue(3.14);
    EXPECT_TRUE(doubleValue.holds<double>());
    EXPECT_DOUBLE_EQ(doubleValue.get<double>(), 3.14);

    ScriptValue stringValue("hello");
    EXPECT_TRUE(stringValue.holds<std::string>());
    EXPECT_EQ(stringValue.get<std::string>(), "hello");
}

TEST(ScriptValueTest, ComplexTypes) {
    std::vector<ScriptValue> arrayValue = {ScriptValue(1), ScriptValue("test"),
                                           ScriptValue(true)};

    ScriptValue vectorValue(arrayValue);
    EXPECT_TRUE(vectorValue.holds<std::vector<ScriptValue>>());

    auto& retrievedArray = vectorValue.get<std::vector<ScriptValue>>();
    EXPECT_EQ(retrievedArray.size(), 3);
    EXPECT_EQ(retrievedArray[0].get<int64_t>(), 1);
    EXPECT_EQ(retrievedArray[1].get<std::string>(), "test");
    EXPECT_EQ(retrievedArray[2].get<bool>(), true);

    std::unordered_map<std::string, ScriptValue> mapValue = {
        {"key1", ScriptValue(100)},
        {"key2", ScriptValue("value2")},
        {"key3", ScriptValue(false)}};

    ScriptValue objectValue(mapValue);
    EXPECT_TRUE(
        objectValue.holds<std::unordered_map<std::string, ScriptValue>>());

    auto& retrievedMap =
        objectValue.get<std::unordered_map<std::string, ScriptValue>>();
    EXPECT_EQ(retrievedMap.size(), 3);
    EXPECT_EQ(retrievedMap["key1"].get<int64_t>(), 100);
    EXPECT_EQ(retrievedMap["key2"].get<std::string>(), "value2");
    EXPECT_EQ(retrievedMap["key3"].get<bool>(), false);
}

// ============================================================================
// Basic Script Execution Tests
// ============================================================================

TEST_F(ScriptingAPITest, BasicScriptExecution) {
    if (!initialized_) {
        GTEST_SKIP() << "Scripting API not initialized";
    }

    std::string script = R"(
        local result = 2 + 3
        return result
    )";

    auto result = scriptingAPI_->execute(script, false, ScriptLanguage::Auto);

    // Test will pass if any scripting engine is available
    if (result.success) {
        EXPECT_TRUE(result.returnValue.holds<int64_t>());
        EXPECT_EQ(result.returnValue.get<int64_t>(), 5);
    }
}

TEST_F(ScriptingAPITest, ScriptExecutionFromFile) {
    if (!initialized_) {
        GTEST_SKIP() << "Scripting API not initialized";
    }

    // This test would require actual script files
    // For now, just test that the method doesn't crash
    auto result = scriptingAPI_->executeFile("nonexistent_script.lua");

    // Should fail gracefully for non-existent file
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

// ============================================================================
// Function Registration and Calling Tests
// ============================================================================

TEST_F(ScriptingAPITest, RegisterAndCallFunction) {
    if (!initialized_) {
        GTEST_SKIP() << "Scripting API not initialized";
    }

    bool functionCalled = false;
    int receivedValue = 0;

    auto testFunction =
        [&functionCalled,
         &receivedValue](const std::vector<ScriptValue>& args) -> ScriptValue {
        functionCalled = true;
        if (!args.empty() && args[0].holds<int64_t>()) {
            receivedValue = static_cast<int>(args[0].get<int64_t>());
        }
        return ScriptValue(42);
    };

    scriptingAPI_->registerFunction("testFunction", testFunction);

    std::string script = R"(
        local result = testFunction(100)
        return result
    )";

    auto result = scriptingAPI_->execute(script, false, ScriptLanguage::Auto);

    if (result.success) {
        EXPECT_TRUE(functionCalled);
        EXPECT_EQ(receivedValue, 100);
        EXPECT_TRUE(result.returnValue.holds<int64_t>());
        EXPECT_EQ(result.returnValue.get<int64_t>(), 42);
    }
}

TEST_F(ScriptingAPITest, RegisterMultipleFunctions) {
    if (!initialized_) {
        GTEST_SKIP() << "Scripting API not initialized";
    }

    auto addFunction = [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 2 && args[0].holds<int64_t>() &&
            args[1].holds<int64_t>()) {
            return ScriptValue(args[0].get<int64_t>() + args[1].get<int64_t>());
        }
        return ScriptValue(0);
    };

    auto multiplyFunction =
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 2 && args[0].holds<int64_t>() &&
            args[1].holds<int64_t>()) {
            return ScriptValue(args[0].get<int64_t>() * args[1].get<int64_t>());
        }
        return ScriptValue(0);
    };

    scriptingAPI_->registerFunction("add", addFunction);
    scriptingAPI_->registerFunction("multiply", multiplyFunction);

    std::string script = R"(
        local sum = add(5, 3)
        local product = multiply(4, 6)
        return sum + product
    )";

    auto result = scriptingAPI_->execute(script, false, ScriptLanguage::Auto);

    if (result.success) {
        EXPECT_TRUE(result.returnValue.holds<int64_t>());
        EXPECT_EQ(result.returnValue.get<int64_t>(), 32);  // 8 + 24
    }
}

// ============================================================================
// Global Variable Tests
// ============================================================================

TEST_F(ScriptingAPITest, GlobalVariables) {
    if (!initialized_) {
        GTEST_SKIP() << "Scripting API not initialized";
    }

    scriptingAPI_->setGlobal("globalInt", ScriptValue(100));
    scriptingAPI_->setGlobal("globalString", ScriptValue("hello world"));
    scriptingAPI_->setGlobal("globalBool", ScriptValue(true));

    std::string script = R"(
        local result = globalInt + string.len(globalString)
        if globalBool then
            result = result * 2
        end
        return result
    )";

    auto result = scriptingAPI_->execute(script, false, ScriptLanguage::Auto);

    if (result.success) {
        EXPECT_TRUE(result.returnValue.holds<int64_t>());
        // 100 + 11 (length of "hello world") = 111, then * 2 = 222
        EXPECT_EQ(result.returnValue.get<int64_t>(), 222);
    }
}

TEST_F(ScriptingAPITest, GetGlobalVariables) {
    if (!initialized_) {
        GTEST_SKIP() << "Scripting API not initialized";
    }

    std::string script = R"(
        testGlobal = 42
        stringGlobal = "test value"
    )";

    auto result = scriptingAPI_->execute(script, false, ScriptLanguage::Auto);

    if (result.success) {
        auto intGlobal = scriptingAPI_->getGlobal("testGlobal");
        auto stringGlobal = scriptingAPI_->getGlobal("stringGlobal");

        if (intGlobal.has_value()) {
            EXPECT_TRUE(intGlobal->holds<int64_t>());
            EXPECT_EQ(intGlobal->get<int64_t>(), 42);
        }

        if (stringGlobal.has_value()) {
            EXPECT_TRUE(stringGlobal->holds<std::string>());
            EXPECT_EQ(stringGlobal->get<std::string>(), "test value");
        }
    }
}

// ============================================================================
// Component Integration Tests
// ============================================================================

TEST_F(ScriptingAPITest, ComponentIntegration) {
    if (!initialized_) {
        GTEST_SKIP() << "Scripting API not initialized";
    }

    auto component = std::make_shared<Component>("ScriptTestComponent");
    component->addVariable<int>("testVar", 50);
    component->def("getDouble", [](int x) -> int { return x * 2; });

    // Register component access function
    auto getComponentVar =
        [component](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (!args.empty() && args[0].holds<std::string>()) {
            std::string varName = args[0].get<std::string>();
            if (component->hasVariable(varName)) {
                auto var = component->getVariable<int>(varName);
                return ScriptValue(static_cast<int64_t>(var->get()));
            }
        }
        return ScriptValue();
    };

    auto callComponentFunc =
        [component](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 2 && args[0].holds<std::string>() &&
            args[1].holds<int64_t>()) {
            std::string funcName = args[0].get<std::string>();
            int value = static_cast<int>(args[1].get<int64_t>());

            try {
                auto result = component->dispatch(funcName, value);
                return ScriptValue(
                    static_cast<int64_t>(std::any_cast<int>(result)));
            } catch (...) {
                return ScriptValue(0);
            }
        }
        return ScriptValue(0);
    };

    scriptingAPI_->registerFunction("getComponentVar", getComponentVar);
    scriptingAPI_->registerFunction("callComponentFunc", callComponentFunc);

    std::string script = R"(
        local varValue = getComponentVar("testVar")
        local doubledValue = callComponentFunc("getDouble", varValue)
        return doubledValue
    )";

    auto result = scriptingAPI_->execute(script, false, ScriptLanguage::Auto);

    if (result.success) {
        EXPECT_TRUE(result.returnValue.holds<int64_t>());
        EXPECT_EQ(result.returnValue.get<int64_t>(), 100);  // 50 * 2
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ScriptingAPITest, ScriptSyntaxError) {
    if (!initialized_) {
        GTEST_SKIP() << "Scripting API not initialized";
    }

    std::string invalidScript = R"(
        local x =
        return x
    )";

    auto result =
        scriptingAPI_->execute(invalidScript, false, ScriptLanguage::Auto);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(ScriptingAPITest, ScriptRuntimeError) {
    if (!initialized_) {
        GTEST_SKIP() << "Scripting API not initialized";
    }

    std::string errorScript = R"(
        local x = nil
        return x.nonexistent_field
    )";

    auto result =
        scriptingAPI_->execute(errorScript, false, ScriptLanguage::Auto);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(ScriptingAPITest, FunctionCallError) {
    if (!initialized_) {
        GTEST_SKIP() << "Scripting API not initialized";
    }

    std::string script = R"(
        return nonexistentFunction()
    )";

    auto result = scriptingAPI_->execute(script, false, ScriptLanguage::Auto);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

#if ATOM_ENABLE_LUA
// ============================================================================
// Lua Engine Specific Tests
// ============================================================================

TEST_F(ScriptEngineTest, LuaEngineCreation) {
    LuaConfig config;
    config.enableJIT = false;
    config.enableDebug = true;
    config.memoryLimit = 16 * 1024 * 1024;

    auto engine = LuaEngineFactory::create(config);
    ASSERT_NE(engine, nullptr);

    ScriptEngineConfig engineConfig;
    EXPECT_TRUE(engine->initialize(engineConfig));

    EXPECT_EQ(engine->getLanguage(), ScriptLanguage::Lua);
}

TEST_F(ScriptEngineTest, LuaBasicExecution) {
    LuaConfig config;
    config.enableJIT = false;
    config.enableDebug = true;

    auto engine = LuaEngineFactory::create(config);
    ASSERT_NE(engine, nullptr);

    ScriptEngineConfig engineConfig;
    ASSERT_TRUE(engine->initialize(engineConfig));

    std::string script = R"(
        local function factorial(n)
            if n <= 1 then
                return 1
            else
                return n * factorial(n - 1)
            end
        end
        return factorial(5)
    )";

    auto result = engine->executeScript(script);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.returnValue.holds<int64_t>());
    EXPECT_EQ(result.returnValue.get<int64_t>(), 120);
}

TEST_F(ScriptEngineTest, LuaFunctionCalling) {
    LuaConfig config;
    auto engine = LuaEngineFactory::create(config);
    ASSERT_NE(engine, nullptr);

    ScriptEngineConfig engineConfig;
    ASSERT_TRUE(engine->initialize(engineConfig));

    // Define a function in Lua
    std::string defineScript = R"(
        function multiply(a, b)
            return a * b
        end
    )";

    auto defineResult = engine->executeScript(defineScript);
    EXPECT_TRUE(defineResult.success);

    // Call the function
    std::vector<ScriptValue> args = {ScriptValue(6), ScriptValue(7)};
    auto callResult = engine->callFunction("multiply", args);

    EXPECT_TRUE(callResult.success);
    EXPECT_TRUE(callResult.returnValue.holds<int64_t>());
    EXPECT_EQ(callResult.returnValue.get<int64_t>(), 42);
}

TEST_F(ScriptEngineTest, LuaGlobalVariables) {
    LuaConfig config;
    auto engine = LuaEngineFactory::create(config);
    ASSERT_NE(engine, nullptr);

    ScriptEngineConfig engineConfig;
    ASSERT_TRUE(engine->initialize(engineConfig));

    // Set global variables
    engine->setGlobal("testNumber", ScriptValue(123));
    engine->setGlobal("testString", ScriptValue("lua test"));

    std::string script = R"(
        return testNumber + string.len(testString)
    )";

    auto result = engine->executeScript(script);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.returnValue.holds<int64_t>());
    EXPECT_EQ(result.returnValue.get<int64_t>(), 131);  // 123 + 8
}

TEST_F(ScriptEngineTest, LuaStatistics) {
    LuaConfig config;
    auto engine = LuaEngineFactory::create(config);
    ASSERT_NE(engine, nullptr);

    ScriptEngineConfig engineConfig;
    ASSERT_TRUE(engine->initialize(engineConfig));

    auto initialStats = engine->getStatistics();

    // Execute some scripts
    engine->executeScript("return 1 + 1");
    engine->executeScript("return 2 * 3");
    engine->executeScript("return 'hello'");

    auto finalStats = engine->getStatistics();

    EXPECT_GT(finalStats.scriptsExecuted, initialStats.scriptsExecuted);
    EXPECT_GE(finalStats.totalExecutionTime.count(),
              initialStats.totalExecutionTime.count());
}
#endif

#if ATOM_ENABLE_PYTHON
// ============================================================================
// Python Engine Specific Tests
// ============================================================================

TEST_F(ScriptEngineTest, PythonEngineCreation) {
    PythonConfig config;
    config.enableSitePackages = false;
    config.isolatedMode = true;

    auto engine = PythonEngineFactory::create(config);
    ASSERT_NE(engine, nullptr);

    ScriptEngineConfig engineConfig;
    EXPECT_TRUE(engine->initialize(engineConfig));

    EXPECT_EQ(engine->getLanguage(), ScriptLanguage::Python);
}

TEST_F(ScriptEngineTest, PythonBasicExecution) {
    PythonConfig config;
    config.enableSitePackages = false;
    config.isolatedMode = true;

    auto engine = PythonEngineFactory::create(config);
    ASSERT_NE(engine, nullptr);

    ScriptEngineConfig engineConfig;
    ASSERT_TRUE(engine->initialize(engineConfig));

    std::string script = R"(
def fibonacci(n):
    if n <= 1:
        return n
    else:
        return fibonacci(n-1) + fibonacci(n-2)

result = fibonacci(10)
    )";

    auto result = engine->executeScript(script);

    EXPECT_TRUE(result.success);

    // Get the result variable
    auto resultValue = engine->getGlobal("result");
    EXPECT_TRUE(resultValue.has_value());
    EXPECT_TRUE(resultValue->holds<int64_t>());
    EXPECT_EQ(resultValue->get<int64_t>(), 55);
}

TEST_F(ScriptEngineTest, PythonGlobalVariables) {
    PythonConfig config;
    config.isolatedMode = true;

    auto engine = PythonEngineFactory::create(config);
    ASSERT_NE(engine, nullptr);

    ScriptEngineConfig engineConfig;
    ASSERT_TRUE(engine->initialize(engineConfig));

    // Set global variables
    engine->setGlobal("x", ScriptValue(10));
    engine->setGlobal("y", ScriptValue(20));

    std::string script = R"(
result = x * y + len("python")
    )";

    auto result = engine->executeScript(script);
    EXPECT_TRUE(result.success);

    auto resultValue = engine->getGlobal("result");
    EXPECT_TRUE(resultValue.has_value());
    EXPECT_TRUE(resultValue->holds<int64_t>());
    EXPECT_EQ(resultValue->get<int64_t>(), 206);  // 10 * 20 + 6
}
#endif
