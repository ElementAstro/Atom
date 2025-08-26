#include "atom/components/scripting_api.hpp"

#include <gtest/gtest.h>

#if ATOM_ENABLE_PYTHON
#include "atom/components/python_engine.hpp"

using namespace atom::components::scripting;

// Test fixture for PythonEngine tests
class PythonEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        ScriptEngineConfig config;
        config.language = ScriptLanguage::Python;
        config.memoryLimit = 1024 * 1024; // 1MB
        config.executionTimeout = std::chrono::seconds(10);
        config.enableDebug = true;
        
        engine_ = std::make_unique<PythonEngine>();
        bool initResult = engine_->initialize(config);
        ASSERT_TRUE(initResult) << "Failed to initialize Python engine";
    }

    void TearDown() override {
        if (engine_) {
            engine_->shutdown();
        }
    }

    std::unique_ptr<PythonEngine> engine_;
};

// ============================================================================
// PythonEngine Basic Tests
// ============================================================================

TEST_F(PythonEngineTest, GetLanguage) {
    EXPECT_EQ(engine_->getLanguage(), ScriptLanguage::Python);
}

TEST_F(PythonEngineTest, ExecuteSimplePythonScript) {
    std::string script = "result = 2 + 3";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_TRUE(result.success);
    
    // Get the result variable
    auto resultValue = engine_->getGlobal("result");
    if (resultValue.has_value()) {
        EXPECT_EQ(resultValue->get<int64_t>(), 5);
    }
}

TEST_F(PythonEngineTest, ExecutePythonFunction) {
    std::string script = R"(
def add(a, b):
    return a + b
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

TEST_F(PythonEngineTest, PythonListHandling) {
    std::string script = R"(
result = [1, 2, 3, 4, 5]
)";
    
    auto result = engine_->executeScript(script);
    ASSERT_TRUE(result.success);
    
    auto listValue = engine_->getGlobal("result");
    if (listValue.has_value()) {
        // Should return an array
        using ArrayType = std::vector<ScriptValue>;
        EXPECT_TRUE(listValue->holds<ArrayType>());
    }
}

TEST_F(PythonEngineTest, PythonDictHandling) {
    std::string script = R"(
result = {'x': 10, 'y': 20, 'z': 30}
)";
    
    auto result = engine_->executeScript(script);
    ASSERT_TRUE(result.success);
    
    auto dictValue = engine_->getGlobal("result");
    if (dictValue.has_value()) {
        // Should return a dictionary/object
        using ObjectType = std::unordered_map<std::string, ScriptValue>;
        EXPECT_TRUE(dictValue->holds<ObjectType>());
    }
}

TEST_F(PythonEngineTest, PythonStringOperations) {
    std::string script = R"(
result = "Hello, " + "World!"
)";
    
    auto result = engine_->executeScript(script);
    ASSERT_TRUE(result.success);
    
    auto stringValue = engine_->getGlobal("result");
    if (stringValue.has_value()) {
        EXPECT_EQ(stringValue->get<std::string>(), "Hello, World!");
    }
}

TEST_F(PythonEngineTest, PythonMathOperations) {
    std::string script = R"(
import math
result = math.sqrt(16) + math.pi
)";
    
    auto result = engine_->executeScript(script);
    ASSERT_TRUE(result.success);
    
    auto mathValue = engine_->getGlobal("result");
    if (mathValue.has_value()) {
        double expected = 4.0 + 3.14159265359; // Approximate pi
        EXPECT_NEAR(mathValue->get<double>(), expected, 0.001);
    }
}

TEST_F(PythonEngineTest, PythonGlobalVariables) {
    // Set a global variable
    ScriptValue value(42);
    engine_->setGlobal("test_global", value);
    
    // Use it in a script
    std::string script = "result = test_global * 2";
    auto result = engine_->executeScript(script);
    ASSERT_TRUE(result.success);
    
    auto resultValue = engine_->getGlobal("result");
    if (resultValue.has_value()) {
        EXPECT_EQ(resultValue->get<int64_t>(), 84);
    }
    
    // Get the global variable back
    auto globalValue = engine_->getGlobal("test_global");
    EXPECT_TRUE(globalValue.has_value());
    if (globalValue.has_value()) {
        EXPECT_EQ(globalValue->get<int64_t>(), 42);
    }
}

TEST_F(PythonEngineTest, PythonErrorHandling) {
    std::string invalidScript = "this is not valid python syntax !!!";
    
    auto result = engine_->executeScript(invalidScript);
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(PythonEngineTest, PythonRuntimeError) {
    std::string script = R"(
def divide(a, b):
    if b == 0:
        raise ValueError("Division by zero")
    return a / b

result = divide(10, 0)
)";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
    EXPECT_NE(result.errorMessage.find("Division by zero"), std::string::npos);
}

TEST_F(PythonEngineTest, PythonFileExecution) {
    // Test file execution (should fail for nonexistent file)
    auto result = engine_->executeFile("nonexistent_file.py");
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(PythonEngineTest, PythonClassDefinition) {
    std::string script = R"(
class TestClass:
    def __init__(self, value):
        self.value = value
    
    def get_value(self):
        return self.value
    
    def set_value(self, value):
        self.value = value

obj = TestClass(42)
result = obj.get_value()
)";
    
    auto result = engine_->executeScript(script);
    ASSERT_TRUE(result.success);
    
    auto resultValue = engine_->getGlobal("result");
    if (resultValue.has_value()) {
        EXPECT_EQ(resultValue->get<int64_t>(), 42);
    }
}

TEST_F(PythonEngineTest, PythonListComprehension) {
    std::string script = R"(
result = [x * x for x in range(5)]
)";
    
    auto result = engine_->executeScript(script);
    ASSERT_TRUE(result.success);
    
    auto listValue = engine_->getGlobal("result");
    if (listValue.has_value()) {
        using ArrayType = std::vector<ScriptValue>;
        EXPECT_TRUE(listValue->holds<ArrayType>());
        
        const auto& array = listValue->get<ArrayType>();
        EXPECT_EQ(array.size(), 5);
        // Should contain [0, 1, 4, 9, 16]
        if (array.size() >= 5) {
            EXPECT_EQ(array[0].get<int64_t>(), 0);
            EXPECT_EQ(array[1].get<int64_t>(), 1);
            EXPECT_EQ(array[2].get<int64_t>(), 4);
            EXPECT_EQ(array[3].get<int64_t>(), 9);
            EXPECT_EQ(array[4].get<int64_t>(), 16);
        }
    }
}

// ============================================================================
// PythonEngine Advanced Tests
// ============================================================================

TEST_F(PythonEngineTest, PythonMemoryUsage) {
    std::string script = R"(
result = []
for i in range(1000):
    result.append(f"string_{i}")
)";
    
    auto result = engine_->executeScript(script);
    
    // Should either succeed or fail gracefully due to memory limits
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}

TEST_F(PythonEngineTest, PythonRecursion) {
    std::string script = R"(
def factorial(n):
    if n <= 1:
        return 1
    else:
        return n * factorial(n - 1)

result = factorial(10)
)";
    
    auto result = engine_->executeScript(script);
    ASSERT_TRUE(result.success);
    
    auto factorialValue = engine_->getGlobal("result");
    if (factorialValue.has_value()) {
        EXPECT_EQ(factorialValue->get<int64_t>(), 3628800); // 10!
    }
}

TEST_F(PythonEngineTest, PythonClosures) {
    std::string script = R"(
def create_counter():
    count = 0
    def counter():
        nonlocal count
        count += 1
        return count
    return counter

counter = create_counter()
result = counter() + counter() + counter()
)";
    
    auto result = engine_->executeScript(script);
    ASSERT_TRUE(result.success);
    
    auto closureValue = engine_->getGlobal("result");
    if (closureValue.has_value()) {
        EXPECT_EQ(closureValue->get<int64_t>(), 6); // 1 + 2 + 3
    }
}

TEST_F(PythonEngineTest, PythonGenerators) {
    std::string script = R"(
def fibonacci():
    a, b = 0, 1
    while True:
        yield a
        a, b = b, a + b

fib = fibonacci()
result = [next(fib) for _ in range(5)]
)";
    
    auto result = engine_->executeScript(script);
    ASSERT_TRUE(result.success);
    
    auto fibValue = engine_->getGlobal("result");
    if (fibValue.has_value()) {
        using ArrayType = std::vector<ScriptValue>;
        EXPECT_TRUE(fibValue->holds<ArrayType>());
        
        const auto& array = fibValue->get<ArrayType>();
        EXPECT_EQ(array.size(), 5);
        // Should contain [0, 1, 1, 2, 3]
        if (array.size() >= 5) {
            EXPECT_EQ(array[0].get<int64_t>(), 0);
            EXPECT_EQ(array[1].get<int64_t>(), 1);
            EXPECT_EQ(array[2].get<int64_t>(), 1);
            EXPECT_EQ(array[3].get<int64_t>(), 2);
            EXPECT_EQ(array[4].get<int64_t>(), 3);
        }
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(PythonEngineTest, PythonSyntaxError) {
    std::string script = "def incomplete(";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(PythonEngineTest, PythonAttributeError) {
    std::string script = R"(
obj = None
result = obj.field
)";
    
    auto result = engine_->executeScript(script);
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(PythonEngineTest, PythonInfiniteLoop) {
    std::string script = R"(
count = 0
while True:
    count += 1
    if count > 1000000:
        break
result = count
)";
    
    auto result = engine_->executeScript(script);
    
    // Should either complete or timeout
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}

#else

// Placeholder test when Python is not enabled
TEST(PythonEngineTest, PythonNotEnabled) {
    GTEST_SKIP() << "Python engine is not enabled in this build";
}

#endif // ATOM_ENABLE_PYTHON
