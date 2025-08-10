// filepath: atom/meta/test_facade_proxy.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <any>
#include <functional>
#include <future>
#include <sstream>  // 添加这个，用于std::ostringstream
#include <string>
#include <thread>  // 需要保留，用于ThreadSafety测试
#include <vector>

#include "atom/meta/facade_proxy.hpp"

using namespace atom::meta;
using ::testing::HasSubstr;

// Test fixture for EnhancedProxyFunction tests
class EnhancedProxyFunctionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Define test functions
        addFunc = [](int a, int b) { return a + b; };
        multiplyFunc = [](int a, int b) { return a * b; };
        greetFunc = [](const std::string& name) {
            return "Hello, " + name + "!";
        };

        // Functions with different return types - 修复未使用参数警告
        noReturnFunc = [](const std::string& msg) {
            /* 避免未使用警告 */ (void)msg;
            /* void function */
        };
        complexFunc = [](int a, double b, const std::string& c) {
            return "Result: " + std::to_string(a) + ", " + std::to_string(b) +
                   ", " + c;
        };
    }

    std::function<int(int, int)> addFunc;
    std::function<int(int, int)> multiplyFunc;
    std::function<std::string(const std::string&)> greetFunc;
    std::function<void(const std::string&)> noReturnFunc;
    std::function<std::string(int, double, const std::string&)> complexFunc;
};

// Test basic function creation and metadata retrieval
TEST_F(EnhancedProxyFunctionTest, BasicFunctionCreation) {
    // 修复：使用复制值而不是引用
    auto addProxy =
        makeEnhancedProxy(std::function<int(int, int)>(addFunc), "add");

    // Test function metadata
    EXPECT_EQ(addProxy.getName(), "add");
    EXPECT_EQ(addProxy.getReturnType(), "int");

    auto paramTypes = addProxy.getParameterTypes();
    ASSERT_EQ(paramTypes.size(), 2);
    EXPECT_EQ(paramTypes[0], "int");
    EXPECT_EQ(paramTypes[1], "int");

    // Test function info
    FunctionInfo info = addProxy.getFunctionInfo();
    EXPECT_EQ(info.getName(), "add");
    EXPECT_EQ(info.getReturnType(), "int");
}

// Test function invocation
TEST_F(EnhancedProxyFunctionTest, FunctionInvocation) {
    auto addProxy =
        makeEnhancedProxy(std::function<int(int, int)>(addFunc), "add");
    auto multiplyProxy = makeEnhancedProxy(
        std::function<int(int, int)>(multiplyFunc), "multiply");
    auto greetProxy = makeEnhancedProxy(
        std::function<std::string(const std::string&)>(greetFunc), "greet");

    // Test int function
    std::vector<std::any> addArgs = {5, 7};
    std::any addResult = addProxy(addArgs);
    EXPECT_EQ(std::any_cast<int>(addResult), 12);

    // Test another int function
    std::vector<std::any> multiplyArgs = {5, 7};
    std::any multiplyResult = multiplyProxy(multiplyArgs);
    EXPECT_EQ(std::any_cast<int>(multiplyResult), 35);

    // Test string function
    std::vector<std::any> greetArgs = {std::string("World")};
    std::any greetResult = greetProxy(greetArgs);
    EXPECT_EQ(std::any_cast<std::string>(greetResult), "Hello, World!");
}

// Test FunctionParams integration - 修复FunctionParams问题
TEST_F(EnhancedProxyFunctionTest, FunctionParamsIntegration) {
    auto complexProxy = makeEnhancedProxy(
        std::function<std::string(int, double, const std::string&)>(
            complexFunc),
        "complex");

    // 使用vector<any>替代FunctionParams
    std::vector<std::any> params;
    params.push_back(42);
    params.push_back(3.14);
    params.push_back(std::string("test"));

    std::any result = complexProxy(params);
    std::string resultStr = std::any_cast<std::string>(result);

    EXPECT_THAT(resultStr, HasSubstr("42"));
    EXPECT_THAT(resultStr, HasSubstr("3.14"));
    EXPECT_THAT(resultStr, HasSubstr("test"));
}

// Test void function handling
TEST_F(EnhancedProxyFunctionTest, VoidFunctionHandling) {
    auto noReturnProxy = makeEnhancedProxy(
        std::function<void(const std::string&)>(noReturnFunc), "noReturn");

    // Get function metadata
    EXPECT_EQ(noReturnProxy.getName(), "noReturn");
    EXPECT_EQ(noReturnProxy.getReturnType(), "void");

    auto paramTypes = noReturnProxy.getParameterTypes();
    ASSERT_EQ(paramTypes.size(), 1);
    EXPECT_THAT(paramTypes[0], HasSubstr("string"));

    // Test invocation of void function
    std::vector<std::any> args = {std::string("void test")};
    std::any result = noReturnProxy(args);

    // Result should be empty for void functions
    EXPECT_THROW(std::any_cast<std::string>(result), std::bad_any_cast);
}

// Test asynchronous function invocation
TEST_F(EnhancedProxyFunctionTest, AsyncFunctionInvocation) {
    auto addProxy =
        makeEnhancedProxy(std::function<int(int, int)>(addFunc), "add");

    // Call the function asynchronously
    std::vector<std::any> args = {10, 20};
    std::future<std::any> futureResult = addProxy.asyncCall(args);

    // Wait for and verify the result
    std::any result = futureResult.get();
    EXPECT_EQ(std::any_cast<int>(result), 30);
}

// Test async with vector params instead of FunctionParams
TEST_F(EnhancedProxyFunctionTest, AsyncWithVectorParams) {
    auto complexProxy = makeEnhancedProxy(
        std::function<std::string(int, double, const std::string&)>(
            complexFunc),
        "complex");

    // Create vector of params
    std::vector<std::any> params;
    params.push_back(100);
    params.push_back(2.718);
    params.push_back(std::string("async"));

    // Call asynchronously
    std::future<std::any> futureResult = complexProxy.asyncCall(params);

    // Verify result
    std::any result = futureResult.get();
    std::string resultStr = std::any_cast<std::string>(result);

    EXPECT_THAT(resultStr, HasSubstr("100"));
    EXPECT_THAT(resultStr, HasSubstr("2.718"));
    EXPECT_THAT(resultStr, HasSubstr("async"));
}

// Test parameter binding
TEST_F(EnhancedProxyFunctionTest, ParameterBinding) {
    auto addProxy =
        makeEnhancedProxy(std::function<int(int, int)>(addFunc), "add");

    // Bind the first parameter to 100
    auto boundAddProxy = addProxy.bind(100);

    // Call with just the second parameter
    std::vector<std::any> args = {5};
    std::any result = boundAddProxy(args);

    EXPECT_EQ(std::any_cast<int>(result), 105);

    // Check the name of the bound function
    EXPECT_THAT(boundAddProxy.getName(), HasSubstr("bound_add"));
}

// Test function composition
TEST_F(EnhancedProxyFunctionTest, FunctionComposition) {
    auto addProxy =
        makeEnhancedProxy(std::function<int(int, int)>(addFunc), "add");
    auto multiplyProxy = makeEnhancedProxy(
        std::function<int(int, int)>(multiplyFunc), "multiply");

    // Compose multiply(add(a,b), c)
    auto composedProxy = multiplyProxy.compose(addProxy);

    // Call the composed function with three parameters
    std::vector<std::any> args = {5, 7, 2};  // (5+7)*2 = 24
    std::any result = composedProxy(args);

    EXPECT_EQ(std::any_cast<int>(result), 24);

    // Check the name of the composed function
    EXPECT_THAT(composedProxy.getName(), HasSubstr("composed_multiply_add"));
}

// Test serialization
TEST_F(EnhancedProxyFunctionTest, FunctionSerialization) {
    auto complexProxy = makeEnhancedProxy(
        std::function<std::string(int, double, const std::string&)>(
            complexFunc),
        "complexFunction");

    // Set parameter names for better serialization
    complexProxy.setParameterName(0, "intParam");
    complexProxy.setParameterName(1, "doubleParam");
    complexProxy.setParameterName(2, "stringParam");

    // Get the serialized function info
    std::string serialized = complexProxy.serialize();

    // Check that the serialized data contains expected information
    EXPECT_THAT(serialized, HasSubstr("complexFunction"));
    EXPECT_THAT(serialized, HasSubstr("string"));
    EXPECT_THAT(serialized, HasSubstr("intParam"));
    EXPECT_THAT(serialized, HasSubstr("doubleParam"));
    EXPECT_THAT(serialized, HasSubstr("stringParam"));
}

// Test output stream functionality
TEST_F(EnhancedProxyFunctionTest, OutputStreamOperator) {
    auto greetProxy = makeEnhancedProxy(
        std::function<std::string(const std::string&)>(greetFunc), "greet");
    greetProxy.setParameterName(0, "name");

    std::ostringstream oss;
    oss << greetProxy;
    std::string output = oss.str();

    EXPECT_THAT(output, HasSubstr("Function: greet"));
    EXPECT_THAT(output, HasSubstr("Return type: string"));
    EXPECT_THAT(output, HasSubstr("Parameters: string name"));
}

// Test copy/move operations
TEST_F(EnhancedProxyFunctionTest, CopyAndMoveOperations) {
    auto original =
        makeEnhancedProxy(std::function<int(int, int)>(addFunc), "original");

    // Test copy constructor
    auto copied(original);
    EXPECT_EQ(copied.getName(), "original");

    // Test move constructor
    auto moved(std::move(copied));
    EXPECT_EQ(moved.getName(), "original");

    // Test copy assignment
    auto assigned =
        makeEnhancedProxy(std::function<int(int, int)>(multiplyFunc), "temp");
    assigned = original;
    EXPECT_EQ(assigned.getName(), "original");

    // Test move assignment
    auto moveAssigned =
        makeEnhancedProxy(std::function<int(int, int)>(multiplyFunc), "temp");
    moveAssigned = std::move(moved);
    EXPECT_EQ(moveAssigned.getName(), "original");

    // Verify functionality after copying
    std::vector<std::any> args = {3, 4};
    std::any result = assigned(args);
    EXPECT_EQ(std::any_cast<int>(result), 7);
}

// Test thread safety
TEST_F(EnhancedProxyFunctionTest, ThreadSafety) {
    auto addProxy =
        makeEnhancedProxy(std::function<int(int, int)>(addFunc), "add");
    auto multiplyProxy = makeEnhancedProxy(
        std::function<int(int, int)>(multiplyFunc), "multiply");

    const int numThreads = 10;
    std::vector<std::thread> threads;
    std::vector<std::any> results(numThreads);

    // Create threads that use the proxies concurrently
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            std::vector<std::any> args = {i, i + 1};
            if (i % 2 == 0) {
                results[i] = addProxy(args);
            } else {
                results[i] = multiplyProxy(args);
            }
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify results
    for (int i = 0; i < numThreads; ++i) {
        if (i % 2 == 0) {
            EXPECT_EQ(std::any_cast<int>(results[i]),
                      2 * i + 1);  // add: i + (i+1)
        } else {
            EXPECT_EQ(std::any_cast<int>(results[i]),
                      i * (i + 1));  // multiply: i * (i+1)
        }
    }
}

// Test error handling for incorrect argument types
TEST_F(EnhancedProxyFunctionTest, ErrorHandlingIncorrectTypes) {
    auto addProxy =
        makeEnhancedProxy(std::function<int(int, int)>(addFunc), "add");

    // Call with incorrect argument types
    std::vector<std::any> args = {std::string("not a number"), 5};

    // Should throw an exception when trying to convert string to int
    EXPECT_THROW(addProxy(args), std::bad_any_cast);
}

// Test error handling for incorrect number of arguments
TEST_F(EnhancedProxyFunctionTest, ErrorHandlingIncorrectArgCount) {
    auto addProxy =
        makeEnhancedProxy(std::function<int(int, int)>(addFunc), "add");

    // Call with too few arguments
    std::vector<std::any> tooFewArgs = {5};
    EXPECT_THROW(addProxy(tooFewArgs), std::out_of_range);

    // Call with too many arguments
    std::vector<std::any> tooManyArgs = {5, 10, 15};
    EXPECT_THROW(addProxy(tooManyArgs), std::out_of_range);
}

// Test complex scenarios combining multiple features
TEST_F(EnhancedProxyFunctionTest, ComplexScenarios) {
    auto addProxy =
        makeEnhancedProxy(std::function<int(int, int)>(addFunc), "add");
    auto multiplyProxy = makeEnhancedProxy(
        std::function<int(int, int)>(multiplyFunc), "multiply");

    // Bind the first parameter of add to 10
    auto boundAddProxy = addProxy.bind(10);

    // Compose multiply with the bound add
    auto composedProxy = multiplyProxy.compose(boundAddProxy);

    // Call the complex function: multiply(10+b, c)
    std::vector<std::any> args = {5, 3};  // (10+5)*3 = 45
    auto result = composedProxy(args);

    EXPECT_EQ(std::any_cast<int>(result), 45);

    // Verify the composed function works with async too
    auto futureResult = composedProxy.asyncCall(args);
    EXPECT_EQ(std::any_cast<int>(futureResult.get()), 45);
}
