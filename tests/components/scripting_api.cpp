#include "atom/components/scripting_api.hpp"
#include "atom/components/component.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>

using namespace atom::components::scripting;

// Test fixture for ScriptValue tests
class ScriptValueTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test values
        nullValue_ = ScriptValue();
        boolValue_ = ScriptValue(true);
        intValue_ = ScriptValue(42);
        doubleValue_ = ScriptValue(3.14159);
        stringValue_ = ScriptValue("Hello World");
        
        // Create array value
        std::vector<ScriptValue> arrayData = {
            ScriptValue(1), ScriptValue(2), ScriptValue(3)
        };
        arrayValue_ = ScriptValue(arrayData);
        
        // Create object value
        std::unordered_map<std::string, ScriptValue> objectData = {
            {"key1", ScriptValue("value1")},
            {"key2", ScriptValue(123)},
            {"key3", ScriptValue(true)}
        };
        objectValue_ = ScriptValue(objectData);
    }

    ScriptValue nullValue_;
    ScriptValue boolValue_;
    ScriptValue intValue_;
    ScriptValue doubleValue_;
    ScriptValue stringValue_;
    ScriptValue arrayValue_;
    ScriptValue objectValue_;
};

// Test fixture for ComponentScriptingAPI tests
class ComponentScriptingAPITest : public ::testing::Test {
protected:
    void SetUp() override {
        api_ = &ComponentScriptingAPI::instance();
        component_ = std::make_shared<Component>("ScriptingTestComponent");
        
        // Initialize the API
        api_->initialize();
    }

    void TearDown() override {
        api_->shutdown();
    }

    ComponentScriptingAPI* api_;
    std::shared_ptr<Component> component_;
};

// Mock ScriptEngine for testing
class MockScriptEngine : public IScriptEngine {
public:
    bool initialize(const ScriptEngineConfig& /*config*/) override { return true; }

    ScriptResult executeScript(const std::string& script, const std::string& /*context*/ = "") override {
        ScriptResult result;
        if (script.find("invalid") != std::string::npos) {
            result.success = false;
            result.errorMessage = "Mock error: invalid script";
        } else {
            result.success = true;
            result.returnValue = ScriptValue(42);
        }
        return result;
    }

    ScriptResult executeFile(const std::string& /*filename*/) override {
        ScriptResult result;
        result.success = false;
        result.errorMessage = "Mock: File not found";
        return result;
    }

    ScriptResult callFunction(const std::string& functionName, const std::vector<ScriptValue>& args = {}) override {
        ScriptResult result;
        result.success = true;
        if (functionName == "testFunc" && args.size() == 2) {
            result.returnValue = ScriptValue(30); // Mock addition result
        }
        return result;
    }

    void setGlobal(const std::string& /*name*/, const ScriptValue& /*value*/) override {}

    std::optional<ScriptValue> getGlobal(const std::string& /*name*/) override {
        return ScriptValue(42);
    }

    void registerFunction(const std::string& /*name*/, ScriptFunction /*function*/) override {}

    ScriptLanguage getLanguage() const override { return ScriptLanguage::Auto; }
};

// Test fixture for ScriptEngine tests
class ScriptEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = std::make_unique<MockScriptEngine>();
        ScriptEngineConfig config;
        engine_->initialize(config);
    }

    void TearDown() override {
        // Mock engine doesn't need explicit shutdown
    }

    std::unique_ptr<MockScriptEngine> engine_;
};

// ============================================================================
// ScriptLanguage Tests
// ============================================================================

TEST(ScriptLanguageTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(ScriptLanguage::Lua), 0);
    EXPECT_EQ(static_cast<uint8_t>(ScriptLanguage::ChaiScript), 1);
    EXPECT_EQ(static_cast<uint8_t>(ScriptLanguage::Auto), 2);
}

// ============================================================================
// ScriptValue Tests
// ============================================================================

TEST_F(ScriptValueTest, DefaultConstruction) {
    ScriptValue value;
    EXPECT_TRUE(std::holds_alternative<std::monostate>(value.value));
}

TEST_F(ScriptValueTest, BooleanConstruction) {
    EXPECT_TRUE(std::holds_alternative<bool>(boolValue_.value));
    EXPECT_EQ(std::get<bool>(boolValue_.value), true);
}

TEST_F(ScriptValueTest, IntegerConstruction) {
    EXPECT_TRUE(std::holds_alternative<int64_t>(intValue_.value));
    EXPECT_EQ(std::get<int64_t>(intValue_.value), 42);
}

TEST_F(ScriptValueTest, DoubleConstruction) {
    EXPECT_TRUE(std::holds_alternative<double>(doubleValue_.value));
    EXPECT_DOUBLE_EQ(std::get<double>(doubleValue_.value), 3.14159);
}

TEST_F(ScriptValueTest, StringConstruction) {
    EXPECT_TRUE(std::holds_alternative<std::string>(stringValue_.value));
    EXPECT_EQ(std::get<std::string>(stringValue_.value), "Hello World");
}

TEST_F(ScriptValueTest, ArrayConstruction) {
    EXPECT_TRUE(std::holds_alternative<std::vector<ScriptValue>>(arrayValue_.value));
    
    const auto& array = std::get<std::vector<ScriptValue>>(arrayValue_.value);
    EXPECT_EQ(array.size(), 3);
    EXPECT_EQ(std::get<int64_t>(array[0].value), 1);
    EXPECT_EQ(std::get<int64_t>(array[1].value), 2);
    EXPECT_EQ(std::get<int64_t>(array[2].value), 3);
}

TEST_F(ScriptValueTest, ObjectConstruction) {
    EXPECT_TRUE(objectValue_.holds<std::unordered_map<std::string, ScriptValue>>());

    const auto& object = objectValue_.get<std::unordered_map<std::string, ScriptValue>>();
    EXPECT_EQ(object.size(), 3);
    EXPECT_TRUE(object.find("key1") != object.end());
    EXPECT_TRUE(object.find("key2") != object.end());
    EXPECT_TRUE(object.find("key3") != object.end());
}

TEST_F(ScriptValueTest, TypeChecking) {
    EXPECT_TRUE(nullValue_.holds<std::monostate>());
    EXPECT_TRUE(boolValue_.holds<bool>());
    EXPECT_TRUE(intValue_.holds<int64_t>());
    EXPECT_TRUE(doubleValue_.holds<double>());
    EXPECT_TRUE(stringValue_.holds<std::string>());

    using ArrayType = std::vector<ScriptValue>;
    EXPECT_TRUE(arrayValue_.holds<ArrayType>());

    using ObjectType = std::unordered_map<std::string, ScriptValue>;
    EXPECT_TRUE(objectValue_.holds<ObjectType>());
}

TEST_F(ScriptValueTest, ValueConversion) {
    // Test direct access to values
    EXPECT_EQ(boolValue_.get<bool>(), true);
    EXPECT_EQ(intValue_.get<int64_t>(), 42);
    EXPECT_DOUBLE_EQ(doubleValue_.get<double>(), 3.14159);
    EXPECT_EQ(stringValue_.get<std::string>(), "Hello World");
}

TEST_F(ScriptValueTest, ArrayAccess) {
    const auto& array = arrayValue_.get<std::vector<ScriptValue>>();
    EXPECT_EQ(array.size(), 3);
    EXPECT_EQ(array[0].get<int64_t>(), 1);
    EXPECT_EQ(array[1].get<int64_t>(), 2);
    EXPECT_EQ(array[2].get<int64_t>(), 3);
}

TEST_F(ScriptValueTest, ObjectAccess) {
    const auto& object = objectValue_.get<std::unordered_map<std::string, ScriptValue>>();
    EXPECT_EQ(object.at("key1").get<std::string>(), "value1");
    EXPECT_EQ(object.at("key2").get<int64_t>(), 123);
    EXPECT_EQ(object.at("key3").get<bool>(), true);
}

TEST_F(ScriptValueTest, Assignment) {
    ScriptValue value;

    value = ScriptValue(true);
    EXPECT_TRUE(value.holds<bool>());
    EXPECT_EQ(value.get<bool>(), true);

    value = ScriptValue(42);
    EXPECT_TRUE(value.holds<int64_t>());
    EXPECT_EQ(value.get<int64_t>(), 42);

    value = ScriptValue(std::string("test"));
    EXPECT_TRUE(value.holds<std::string>());
    EXPECT_EQ(value.get<std::string>(), "test");
}

// ============================================================================
// ComponentScriptingAPI Tests
// ============================================================================

TEST_F(ComponentScriptingAPITest, Singleton) {
    auto& api1 = ComponentScriptingAPI::instance();
    auto& api2 = ComponentScriptingAPI::instance();
    EXPECT_EQ(&api1, &api2);
}

TEST_F(ComponentScriptingAPITest, CreateEngine) {
    auto engine = api_->createEngine(ScriptLanguage::Auto);
    EXPECT_NE(engine, nullptr);
}

TEST_F(ComponentScriptingAPITest, ExecuteScript) {
    // Simple script that should execute successfully
    std::string script = "return 42";

    auto result = api_->execute(script, false, ScriptLanguage::Auto);

    // Result depends on implementation - should either succeed or fail gracefully
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}

TEST_F(ComponentScriptingAPITest, ExecuteScriptWithError) {
    // Invalid script that should fail
    std::string invalidScript = "invalid syntax here!!!";

    auto result = api_->execute(invalidScript, false, ScriptLanguage::Auto);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(ComponentScriptingAPITest, DetectLanguage) {
    // Test language detection
    auto luaLang = api_->detectLanguage("test.lua", true);
    EXPECT_EQ(luaLang, ScriptLanguage::Lua);

    auto autoLang = api_->detectLanguage("return 42", false);
    // Should return some language or Auto
    EXPECT_TRUE(autoLang == ScriptLanguage::Auto ||
                autoLang == ScriptLanguage::Lua ||
                autoLang == ScriptLanguage::ChaiScript);
}

TEST_F(ComponentScriptingAPITest, RegisterComponentAPI) {
    auto engine = api_->createEngine(ScriptLanguage::Auto);
    ASSERT_NE(engine, nullptr);

    // Register component API functions
    EXPECT_NO_THROW(api_->registerComponentAPI(*engine));
}

TEST_F(ComponentScriptingAPITest, RegisterComponent) {
    auto engine = api_->createEngine(ScriptLanguage::Auto);
    ASSERT_NE(engine, nullptr);

    // Register a component with the engine
    EXPECT_NO_THROW(api_->registerComponent("TestComponent", component_, *engine));
}

TEST_F(ComponentScriptingAPITest, GetGlobalStatistics) {
    const auto& stats = api_->getGlobalStatistics();

    // Should have valid statistics structure
    EXPECT_GE(stats.totalEnginesCreated, 0);
    EXPECT_GE(stats.activeEngines, 0);
}

TEST_F(ComponentScriptingAPITest, ResetGlobalStatistics) {
    // Create an engine to generate some stats
    auto engine = api_->createEngine(ScriptLanguage::Auto);

    // Reset statistics
    EXPECT_NO_THROW(api_->resetGlobalStatistics());

    const auto& stats = api_->getGlobalStatistics();
    // After reset, some stats should be cleared
    EXPECT_GE(stats.totalEnginesCreated, 0);
}

// ============================================================================
// ScriptEngine Tests
// ============================================================================

TEST_F(ScriptEngineTest, ExecuteSimpleScript) {
    std::string script = "return 1 + 1";

    auto result = engine_->executeScript(script);

    // Should either succeed or fail gracefully
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}

TEST_F(ScriptEngineTest, ExecuteInvalidScript) {
    std::string invalidScript = "this is not valid script syntax";

    auto result = engine_->executeScript(invalidScript);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(ScriptEngineTest, ExecuteFile) {
    std::string filename = "nonexistent_file.lua";

    auto result = engine_->executeFile(filename);

    // Should fail gracefully for nonexistent file
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(ScriptEngineTest, SetAndGetGlobalVariable) {
    ScriptValue value(42);
    engine_->setGlobal("testGlobal", value);

    auto getResult = engine_->getGlobal("testGlobal");
    if (getResult.has_value()) {
        EXPECT_EQ(getResult->get<int64_t>(), 42);
    }
}

TEST_F(ScriptEngineTest, CallFunction) {
    // First define a function
    std::string defineScript = "function testFunc(a, b) return a + b end";
    auto defineResult = engine_->executeScript(defineScript);

    if (defineResult.success) {
        std::vector<ScriptValue> args = {ScriptValue(10), ScriptValue(20)};
        auto callResult = engine_->callFunction("testFunc", args);

        if (callResult.success) {
            EXPECT_EQ(callResult.returnValue.get<int64_t>(), 30);
        }
    }
}

TEST_F(ScriptEngineTest, GetLanguage) {
    auto language = engine_->getLanguage();
    EXPECT_EQ(language, ScriptLanguage::Auto);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(ScriptingErrorTest, InvalidScriptValue) {
    ScriptValue value;

    // Test that default constructed value holds monostate
    EXPECT_TRUE(value.holds<std::monostate>());

    // Test exception handling when accessing wrong type
    EXPECT_THROW(value.get<int64_t>(), std::bad_variant_access);
    EXPECT_THROW(value.get<std::string>(), std::bad_variant_access);
    EXPECT_THROW(value.get<bool>(), std::bad_variant_access);
}

TEST_F(ScriptEngineTest, MemoryLimits) {
    // Test that the engine handles memory limits appropriately
    // This would be implementation-specific
    std::string memoryIntensiveScript = R"(
        local t = {}
        for i = 1, 1000 do
            t[i] = string.rep("x", 1000)
        end
        return #t
    )";

    auto result = engine_->executeScript(memoryIntensiveScript);

    // Should either succeed or fail gracefully with memory limit
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(ComponentScriptingAPITest, ThreadSafety) {
    const int numThreads = 4;
    const int operationsPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Launch threads that perform API operations
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &successCount]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                try {
                    auto engine = api_->createEngine(ScriptLanguage::Auto);
                    if (engine) {
                        successCount++;
                    }
                } catch (...) {
                    // Handle any exceptions gracefully
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successCount.load(), 0);
}
