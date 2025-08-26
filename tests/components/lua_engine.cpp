#include "atom/components/scripting_api.hpp"

#include <gtest/gtest.h>

#if ATOM_ENABLE_LUA
#include "atom/components/lua_engine.hpp"

using namespace atom::components::scripting;

// Test fixture for LuaEngine tests
class LuaEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        ScriptEngineConfig config;
        config.language = ScriptLanguage::Lua;
        config.memoryLimit = 1024 * 1024; // 1MB
        config.executionTimeout = std::chrono::seconds(10);
        config.enableDebug = true;
        
        engine_ = std::make_unique<LuaEngine>();
        bool initResult = engine_->initialize(config);
        ASSERT_TRUE(initResult) << "Failed to initialize Lua engine";
    }

    void TearDown() override {
        if (engine_) {
            engine_->shutdown();
        }
    }

    std::unique_ptr<LuaEngine> engine_;
};

// ============================================================================
// LuaEngine Basic Tests
// ============================================================================

TEST_F(LuaEngineTest, GetLanguage) {
    EXPECT_EQ(engine_->getLanguage(), ScriptLanguage::Lua);
}

TEST_F(LuaEngineTest, ExecuteSimpleLuaScript) {
    std::string script = "return 2 + 3";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 5);
    }
}

TEST_F(LuaEngineTest, ExecuteLuaFunction) {
    std::string script = R"(
        function add(a, b)
            return a + b
        end
    )";
    
    auto defineResult = engine_->executeScript(script);
    ASSERT_TRUE(defineResult.success);
    
    std::vector<ScriptValue> args = {ScriptValue(10), ScriptValue(20)};
    auto callResult = engine_->callFunction("add", args);
    
    EXPECT_TRUE(callResult.success);
    if (callResult.success) {
        EXPECT_EQ(callResult.returnValue.get<int64_t>(), 30);
    }
}

TEST_F(LuaEngineTest, LuaTableHandling) {
    std::string script = R"(
        local t = {x = 10, y = 20, z = 30}
        return t
    )";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_TRUE(result.success);
    if (result.success) {
        // Should return a table/object
        using ObjectType = std::unordered_map<std::string, ScriptValue>;
        EXPECT_TRUE(result.returnValue.holds<ObjectType>());
    }
}

TEST_F(LuaEngineTest, LuaArrayHandling) {
    std::string script = R"(
        local arr = {1, 2, 3, 4, 5}
        return arr
    )";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_TRUE(result.success);
    if (result.success) {
        // Should return an array
        using ArrayType = std::vector<ScriptValue>;
        EXPECT_TRUE(result.returnValue.holds<ArrayType>());
    }
}

TEST_F(LuaEngineTest, LuaStringOperations) {
    std::string script = R"(
        local str = "Hello, "
        str = str .. "World!"
        return str
    )";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<std::string>(), "Hello, World!");
    }
}

TEST_F(LuaEngineTest, LuaMathOperations) {
    std::string script = R"(
        return math.sqrt(16) + math.pi
    )";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_TRUE(result.success);
    if (result.success) {
        double expected = 4.0 + 3.14159265359; // Approximate pi
        EXPECT_NEAR(result.returnValue.get<double>(), expected, 0.001);
    }
}

TEST_F(LuaEngineTest, LuaGlobalVariables) {
    // Set a global variable
    ScriptValue value(42);
    engine_->setGlobal("testGlobal", value);
    
    // Use it in a script
    std::string script = "return testGlobal * 2";
    auto result = engine_->executeScript(script);
    
    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 84);
    }
    
    // Get the global variable back
    auto globalValue = engine_->getGlobal("testGlobal");
    EXPECT_TRUE(globalValue.has_value());
    if (globalValue.has_value()) {
        EXPECT_EQ(globalValue->get<int64_t>(), 42);
    }
}

TEST_F(LuaEngineTest, LuaErrorHandling) {
    std::string invalidScript = "this is not valid lua syntax !!!";
    
    auto result = engine_->executeScript(invalidScript);
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(LuaEngineTest, LuaRuntimeError) {
    std::string script = R"(
        local function divide(a, b)
            if b == 0 then
                error("Division by zero")
            end
            return a / b
        end
        return divide(10, 0)
    )";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
    EXPECT_NE(result.errorMessage.find("Division by zero"), std::string::npos);
}

TEST_F(LuaEngineTest, LuaFileExecution) {
    // Test file execution (should fail for nonexistent file)
    auto result = engine_->executeFile("nonexistent_file.lua");
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(LuaEngineTest, LuaCoroutines) {
    std::string script = R"(
        local co = coroutine.create(function()
            coroutine.yield(1)
            coroutine.yield(2)
            return 3
        end)
        
        local success, value = coroutine.resume(co)
        return value
    )";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 1);
    }
}

TEST_F(LuaEngineTest, LuaMetatables) {
    std::string script = R"(
        local mt = {
            __add = function(a, b)
                return {value = a.value + b.value}
            end
        }
        
        local obj1 = {value = 10}
        local obj2 = {value = 20}
        
        setmetatable(obj1, mt)
        setmetatable(obj2, mt)
        
        local result = obj1 + obj2
        return result.value
    )";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 30);
    }
}

// ============================================================================
// LuaEngine Advanced Tests
// ============================================================================

TEST_F(LuaEngineTest, LuaMemoryUsage) {
    std::string script = R"(
        local t = {}
        for i = 1, 1000 do
            t[i] = "string_" .. i
        end
        return #t
    )";
    
    auto result = engine_->executeScript(script);
    
    // Should either succeed or fail gracefully due to memory limits
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}

TEST_F(LuaEngineTest, LuaRecursion) {
    std::string script = R"(
        local function factorial(n)
            if n <= 1 then
                return 1
            else
                return n * factorial(n - 1)
            end
        end
        return factorial(10)
    )";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 3628800); // 10!
    }
}

TEST_F(LuaEngineTest, LuaClosures) {
    std::string script = R"(
        local function createCounter()
            local count = 0
            return function()
                count = count + 1
                return count
            end
        end
        
        local counter = createCounter()
        return counter() + counter() + counter()
    )";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 6); // 1 + 2 + 3
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(LuaEngineTest, LuaSyntaxError) {
    std::string script = "function incomplete(";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(LuaEngineTest, LuaNilAccess) {
    std::string script = R"(
        local obj = nil
        return obj.field
    )";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(LuaEngineTest, LuaInfiniteLoop) {
    std::string script = R"(
        local count = 0
        while true do
            count = count + 1
            if count > 1000000 then
                break
            end
        end
        return count
    )";
    
    auto result = engine_->executeScript(script);
    
    // Should either complete or timeout
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}

#else

// Placeholder test when Lua is not enabled
TEST(LuaEngineTest, LuaNotEnabled) {
    GTEST_SKIP() << "Lua engine is not enabled in this build";
}

#endif // ATOM_ENABLE_LUA
