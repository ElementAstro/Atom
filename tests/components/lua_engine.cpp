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
        config.memoryLimit = 1024 * 1024;  // 1MB
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
        double expected = 4.0 + 3.14159265359;  // Approximate pi
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
        EXPECT_EQ(result.returnValue.get<int64_t>(), 3628800);  // 10!
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
        EXPECT_EQ(result.returnValue.get<int64_t>(), 6);  // 1 + 2 + 3
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

// ============================================================================
// Additional LuaEngine Tests
// ============================================================================

TEST_F(LuaEngineTest, LuaBooleanOperations) {
    std::string script = R"(
        local a = true
        local b = false
        return a and not b
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_TRUE(result.returnValue.get<bool>());
    }
}

TEST_F(LuaEngineTest, LuaMultipleReturnValues) {
    std::string script = R"(
        function multiReturn()
            return 1, 2, 3
        end
        local a, b, c = multiReturn()
        return a + b + c
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 6);
    }
}

TEST_F(LuaEngineTest, LuaStringPatternMatching) {
    std::string script = R"(
        local str = "Hello, World!"
        local match = string.match(str, "World")
        return match
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<std::string>(), "World");
    }
}

TEST_F(LuaEngineTest, LuaTableIteration) {
    std::string script = R"(
        local t = {a = 1, b = 2, c = 3}
        local sum = 0
        for k, v in pairs(t) do
            sum = sum + v
        end
        return sum
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 6);
    }
}

TEST_F(LuaEngineTest, LuaNestedTables) {
    std::string script = R"(
        local t = {
            inner = {
                value = 42
            }
        }
        return t.inner.value
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 42);
    }
}

TEST_F(LuaEngineTest, LuaLocalVariableScope) {
    std::string script = R"(
        local x = 10
        do
            local x = 20
        end
        return x
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 10);
    }
}

TEST_F(LuaEngineTest, LuaModuloOperation) {
    std::string script = R"(
        return 17 % 5
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 2);
    }
}

TEST_F(LuaEngineTest, LuaPowerOperation) {
    std::string script = R"(
        return 2 ^ 10
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<double>(), 1024.0);
    }
}

TEST_F(LuaEngineTest, LuaStringLength) {
    std::string script = R"(
        local str = "Hello"
        return #str
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 5);
    }
}

TEST_F(LuaEngineTest, LuaTableLength) {
    std::string script = R"(
        local t = {1, 2, 3, 4, 5}
        return #t
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 5);
    }
}

TEST_F(LuaEngineTest, LuaConditionalExpression) {
    std::string script = R"(
        local x = 10
        local result = x > 5 and "greater" or "lesser"
        return result
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<std::string>(), "greater");
    }
}

TEST_F(LuaEngineTest, LuaTypeFunction) {
    std::string script = R"(
        return type(42)
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<std::string>(), "number");
    }
}

TEST_F(LuaEngineTest, LuaToNumberConversion) {
    std::string script = R"(
        return tonumber("42") + 8
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 50);
    }
}

TEST_F(LuaEngineTest, LuaToStringConversion) {
    std::string script = R"(
        return tostring(42) .. " is the answer"
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<std::string>(), "42 is the answer");
    }
}

TEST_F(LuaEngineTest, LuaTableInsert) {
    std::string script = R"(
        local t = {1, 2, 3}
        table.insert(t, 4)
        return #t
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 4);
    }
}

TEST_F(LuaEngineTest, LuaTableRemove) {
    std::string script = R"(
        local t = {1, 2, 3, 4}
        table.remove(t, 2)
        return #t
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 3);
    }
}

TEST_F(LuaEngineTest, LuaTableSort) {
    std::string script = R"(
        local t = {3, 1, 4, 1, 5, 9, 2, 6}
        table.sort(t)
        return t[1]
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 1);
    }
}

TEST_F(LuaEngineTest, LuaStringFormat) {
    std::string script = R"(
        return string.format("Value: %d, Name: %s", 42, "test")
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<std::string>(),
                  "Value: 42, Name: test");
    }
}

TEST_F(LuaEngineTest, LuaPcallErrorHandling) {
    std::string script = R"(
        local success, err = pcall(function()
            error("intentional error")
        end)
        return success
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_FALSE(result.returnValue.get<bool>());
    }
}

TEST_F(LuaEngineTest, LuaAssert) {
    std::string script = R"(
        local function safeAssert()
            local success, err = pcall(function()
                assert(false, "assertion failed")
            end)
            return success
        end
        return safeAssert()
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_FALSE(result.returnValue.get<bool>());
    }
}

TEST_F(LuaEngineTest, LuaIpairs) {
    std::string script = R"(
        local t = {10, 20, 30}
        local sum = 0
        for i, v in ipairs(t) do
            sum = sum + v
        end
        return sum
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 60);
    }
}

TEST_F(LuaEngineTest, LuaSelect) {
    std::string script = R"(
        local function varargs(...)
            return select("#", ...)
        end
        return varargs(1, 2, 3, 4, 5)
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 5);
    }
}

TEST_F(LuaEngineTest, LuaUnpack) {
    std::string script = R"(
        local t = {1, 2, 3}
        local a, b, c = table.unpack(t)
        return a + b + c
    )";

    auto result = engine_->executeScript(script);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 6);
    }
}

#else

// Placeholder test when Lua is not enabled
TEST(LuaEngineTest, LuaNotEnabled) {
    GTEST_SKIP() << "Lua engine is not enabled in this build";
}

#endif  // ATOM_ENABLE_LUA
