#include <gtest/gtest.h>
#include "atom/meta/proxy.hpp"

#include <chrono>
#include <functional>
#include <string>
#include <thread>

namespace atom::meta::test {

// Helper functions for testing
int add(int a, int b) { return a + b; }
std::string concatenate(const std::string& a, const std::string& b) {
    return a + b;
}
void incrementCounter(int& counter) { counter++; }
double multiply(double a, double b) { return a * b; }
int throwingFunction(int val) {
    if (val < 0)
        throw std::runtime_error("Negative value not allowed");
    return val * 2;
}
int noexceptFunction(int val) noexcept { return val * 2; }
std::vector<int> vectorFunction(const std::vector<int>& vec) {
    std::vector<int> result = vec;
    for (auto& item : result)
        item *= 2;
    return result;
}

// Test fixture for FunctionInfo
class FunctionInfoTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test FunctionInfo basic operations
TEST_F(FunctionInfoTest, BasicOperations) {
    FunctionInfo info("test_func", "int");

    EXPECT_EQ(info.getName(), "test_func");
    EXPECT_EQ(info.getReturnType(), "int");

    info.addArgumentType("int");
    info.addArgumentType("double");

    auto argTypes = info.getArgumentTypes();
    EXPECT_EQ(argTypes.size(), 2);
    EXPECT_EQ(argTypes[0], "int");
    EXPECT_EQ(argTypes[1], "double");

    info.setParameterName(0, "a");
    info.setParameterName(1, "b");

    auto paramNames = info.getParameterNames();
    EXPECT_EQ(paramNames.size(), 2);
    EXPECT_EQ(paramNames[0], "a");
    EXPECT_EQ(paramNames[1], "b");

    info.setNoexcept(true);
    EXPECT_TRUE(info.isNoexcept());

    info.setHash("12345");
    EXPECT_EQ(info.getHash(), "12345");
}

// Test FunctionInfo JSON serialization
TEST_F(FunctionInfoTest, JsonSerialization) {
    FunctionInfo info("test_func", "int");
    info.addArgumentType("int");
    info.addArgumentType("double");
    info.setParameterName(0, "a");
    info.setParameterName(1, "b");
    info.setNoexcept(true);
    info.setHash("12345");

    auto json = info.toJson();
    EXPECT_EQ(json["name"], "test_func");
    EXPECT_EQ(json["return_type"], "int");
    EXPECT_EQ(json["argument_types"][0], "int");
    EXPECT_EQ(json["argument_types"][1], "double");
    EXPECT_EQ(json["parameter_names"][0], "a");
    EXPECT_EQ(json["parameter_names"][1], "b");
    EXPECT_EQ(json["hash"], "12345");
    EXPECT_TRUE(json["noexcept"].get<bool>());

    // Test deserialization
    auto deserializedInfo = FunctionInfo::fromJson(json);
    EXPECT_EQ(deserializedInfo.getName(), "test_func");
    EXPECT_EQ(deserializedInfo.getReturnType(), "int");
    EXPECT_EQ(deserializedInfo.getArgumentTypes()[0], "int");
    EXPECT_EQ(deserializedInfo.getArgumentTypes()[1], "double");
    EXPECT_EQ(deserializedInfo.getParameterNames()[0], "a");
    EXPECT_EQ(deserializedInfo.getParameterNames()[1], "b");
    EXPECT_EQ(deserializedInfo.getHash(), "12345");
    EXPECT_TRUE(deserializedInfo.isNoexcept());
}

// Test any cast helper functions
class AnyCastHelperTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(AnyCastHelperTest, BasicTypeCasts) {
    // Test value types
    std::any intVal = 42;
    EXPECT_EQ(anyCastVal<int>(intVal), 42);
    EXPECT_THROW(anyCastVal<double>(intVal), ProxyTypeError);

    // Test reference types
    int x = 42;
    std::any intRef = std::ref(x);
    EXPECT_EQ(anyCastRef<int&>(intRef), 42);

    // Test const reference types
    const std::string str = "hello";
    std::any strRef = std::cref(str);
    EXPECT_EQ(anyCastConstRef<std::string>(strRef), "hello");
}

TEST_F(AnyCastHelperTest, TypeConversion) {
    // Test integer to double conversion
    std::any intVal = 42;
    std::any doubleVal = 3.14;
    std::any floatVal = 2.71f;

    // Integer conversions
    int intResult = anyCastHelper<int>(intVal);
    EXPECT_EQ(intResult, 42);

    // Double to int conversion should work with tryConvertType
    int convertedInt = anyCastHelper<int>(doubleVal);
    EXPECT_EQ(convertedInt, 3);

    // Float to double conversion
    double convertedDouble = anyCastHelper<double>(floatVal);
    EXPECT_DOUBLE_EQ(convertedDouble, 2.71);

    // String conversion tests
    std::any charPtrVal = "hello";
    std::any strViewVal = std::string_view("world");

    std::string strFromCharPtr = anyCastHelper<std::string>(charPtrVal);
    EXPECT_EQ(strFromCharPtr, "hello");

    std::string strFromStrView = anyCastHelper<std::string>(strViewVal);
    EXPECT_EQ(strFromStrView, "world");
}

// Test fixture for ProxyFunction
class ProxyFunctionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test basic ProxyFunction operations
TEST_F(ProxyFunctionTest, BasicFunctionCall) {
    ProxyFunction proxy(add);

    // Test function info collection
    FunctionInfo info = proxy.getFunctionInfo();
    EXPECT_EQ(info.getName(), "anonymous_function");  // Default name
    EXPECT_EQ(info.getReturnType(), "int");
    EXPECT_FALSE(info.isNoexcept());

    auto argTypes = info.getArgumentTypes();
    EXPECT_EQ(argTypes.size(), 2);
    EXPECT_TRUE(argTypes[0].find("int") != std::string::npos);
    EXPECT_TRUE(argTypes[1].find("int") != std::string::npos);

    // Test function call with vector of any
    std::vector<std::any> args = {5, 3};
    std::any result = proxy(args);
    EXPECT_EQ(std::any_cast<int>(result), 8);

    // Test function call with FunctionParams
    FunctionParams params;
    params.emplace_back("a", 10);
    params.emplace_back("b", 20);
    result = proxy(params);
    EXPECT_EQ(std::any_cast<int>(result), 30);

    // Test setting function name
    proxy.setName("add_function");
    info = proxy.getFunctionInfo();
    EXPECT_EQ(info.getName(), "add_function");

    // Test setting parameter names
    proxy.setParameterName(0, "first");
    proxy.setParameterName(1, "second");
    info = proxy.getFunctionInfo();
    EXPECT_EQ(info.getParameterNames().size(), 2);
    EXPECT_EQ(info.getParameterNames()[0], "first");
    EXPECT_EQ(info.getParameterNames()[1], "second");
}

// Test ProxyFunction with different parameter types
TEST_F(ProxyFunctionTest, DifferentParameterTypes) {
    // Test with string concatenation function
    ProxyFunction strProxy(concatenate);
    FunctionInfo strInfo = strProxy.getFunctionInfo();
    EXPECT_EQ(strInfo.getReturnType(), "std::string");

    std::vector<std::any> strArgs = {std::string("Hello, "),
                                     std::string("World!")};
    std::any strResult = strProxy(strArgs);
    EXPECT_EQ(std::any_cast<std::string>(strResult), "Hello, World!");

    // Test with void return function
    int counter = 0;
    ProxyFunction voidProxy(
        [&counter](int increment) { counter += increment; });
    FunctionInfo voidInfo = voidProxy.getFunctionInfo();
    EXPECT_TRUE(voidInfo.getReturnType().find("void") != std::string::npos);

    std::vector<std::any> voidArgs = {3};
    voidProxy(voidArgs);
    EXPECT_EQ(counter, 3);

    // Test with function returning vector
    ProxyFunction vecProxy(vectorFunction);
    std::vector<int> inputVec = {1, 2, 3};
    std::vector<std::any> vecArgs = {inputVec};
    std::any vecResult = vecProxy(vecArgs);
    auto outputVec = std::any_cast<std::vector<int>>(vecResult);
    EXPECT_EQ(outputVec.size(), 3);
    EXPECT_EQ(outputVec[0], 2);
    EXPECT_EQ(outputVec[1], 4);
    EXPECT_EQ(outputVec[2], 6);
}

// Test ProxyFunction with type conversion
TEST_F(ProxyFunctionTest, TypeConversion) {
    ProxyFunction proxy(add);

    // Test with double -> int conversion
    std::vector<std::any> args = {5.5, 3.2};
    std::any result = proxy(args);
    EXPECT_EQ(std::any_cast<int>(result), 8);  // 5 + 3

    // Test with mixed types
    args = {10, 3.7};
    result = proxy(args);
    EXPECT_EQ(std::any_cast<int>(result), 13);  // 10 + 3

    // Test with string conversion
    ProxyFunction strProxy(concatenate);
    std::vector<std::any> strArgs = {std::string("Hello, "), "World!"};
    std::any strResult = strProxy(strArgs);
    EXPECT_EQ(std::any_cast<std::string>(strResult), "Hello, World!");

    // Test with char* and string_view
    strArgs = {"Hello, ", std::string_view("Universe!")};
    strResult = strProxy(strArgs);
    EXPECT_EQ(std::any_cast<std::string>(strResult), "Hello, Universe!");
}

// Test ProxyFunction error handling
TEST_F(ProxyFunctionTest, ErrorHandling) {
    ProxyFunction proxy(add);

    // Test incorrect argument count
    std::vector<std::any> args = {5};
    EXPECT_THROW(proxy(args), ProxyArgumentError);

    // Test incorrect argument types
    args = {5, std::string("not_a_number")};
    EXPECT_THROW(proxy(args), ProxyTypeError);

    // Test throwing function
    ProxyFunction throwingProxy(throwingFunction);
    args = {-5};
    EXPECT_THROW(throwingProxy(args), std::runtime_error);
}

// Test ProxyFunction with noexcept functions
TEST_F(ProxyFunctionTest, NoexceptFunction) {
    ProxyFunction proxy(noexceptFunction);
    FunctionInfo info = proxy.getFunctionInfo();
    EXPECT_TRUE(info.isNoexcept());

    std::vector<std::any> args = {5};
    std::any result = proxy(args);
    EXPECT_EQ(std::any_cast<int>(result), 10);
}

// Test fixture for AsyncProxyFunction
class AsyncProxyFunctionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test basic AsyncProxyFunction operations
TEST_F(AsyncProxyFunctionTest, BasicAsyncFunctionCall) {
    AsyncProxyFunction asyncProxy(add);

    // Test function info collection
    FunctionInfo info = asyncProxy.getFunctionInfo();
    EXPECT_EQ(info.getName(), "anonymous_function");
    EXPECT_EQ(info.getReturnType(), "int");

    // Test async function call with vector of any
    std::vector<std::any> args = {5, 3};
    std::future<std::any> futureResult = asyncProxy(args);
    std::any result = futureResult.get();
    EXPECT_EQ(std::any_cast<int>(result), 8);

    // Test async function call with FunctionParams
    FunctionParams params;
    params.emplace_back("a", 10);
    params.emplace_back("b", 20);
    futureResult = asyncProxy(params);
    result = futureResult.get();
    EXPECT_EQ(std::any_cast<int>(result), 30);

    // Test async function with delay
    AsyncProxyFunction delayProxy([](int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return 42;
    });

    auto start = std::chrono::steady_clock::now();
    futureResult = delayProxy(std::vector<std::any>{50});
    result = futureResult.get();
    auto end = std::chrono::steady_clock::now();

    EXPECT_EQ(std::any_cast<int>(result), 42);
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(duration.count(), 50);
}

// Test AsyncProxyFunction error handling
TEST_F(AsyncProxyFunctionTest, AsyncErrorHandling) {
    AsyncProxyFunction asyncProxy(throwingFunction);

    // Test with negative value that will throw
    std::vector<std::any> args = {-5};
    std::future<std::any> futureResult = asyncProxy(args);

    EXPECT_THROW(futureResult.get(), std::runtime_error);

    // Test incorrect argument count
    std::vector<std::any> wrongArgs = {1, 2};
    futureResult = asyncProxy(wrongArgs);
    EXPECT_THROW(futureResult.get(), ProxyArgumentError);
}

// Test fixture for ComposedProxy
class ComposedProxyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    // Helper functions for composition
    static int doubleValue(int x) { return x * 2; }
    static int addFive(int x) { return x + 5; }
};

// Test basic ComposedProxy operations
TEST_F(ComposedProxyTest, BasicComposition) {
    // Create a composed proxy: double and then add 5
    auto proxy = composeProxy(doubleValue, addFive);

    // Test function info
    FunctionInfo info = proxy.getFunctionInfo();
    EXPECT_TRUE(
        info.getName().find("composed_anonymous_function_anonymous_function") !=
        std::string::npos);
    EXPECT_EQ(info.getReturnType(), "int");
    EXPECT_EQ(info.getArgumentTypes().size(), 1);

    // Test with vector of any
    std::vector<std::any> args = {10};
    std::any result = proxy(args);

    // (10 * 2) + 5 = 25
    EXPECT_EQ(std::any_cast<int>(result), 25);

    // Test with FunctionParams
    FunctionParams params;
    params.emplace_back("x", 7);
    result = proxy(params);

    // (7 * 2) + 5 = 19
    EXPECT_EQ(std::any_cast<int>(result), 19);
}

// Test complex composition
TEST_F(ComposedProxyTest, ComplexComposition) {
    // Create a more complex chain: doubleValue -> addFive -> stringConvert
    auto doubleProxy = makeProxy(doubleValue);
    auto addFiveProxy = makeProxy(addFive);
    auto stringConvertProxy =
        makeProxy([](int x) { return "Result: " + std::to_string(x); });

    // First compose doubleValue and addFive
    auto intermediateProxy = composeProxy(doubleValue, addFive);

    // Then compose with stringConvert
    auto finalProxy = composeProxy(
        [&intermediateProxy](int x) {
            return std::any_cast<int>(
                intermediateProxy(std::vector<std::any>{x}));
        },
        [&stringConvertProxy](int x) {
            return std::any_cast<std::string>(
                stringConvertProxy(std::vector<std::any>{x}));
        });

    // Test the final composition
    std::vector<std::any> args = {10};
    std::any result = finalProxy(args);

    // (10 * 2) + 5 = 25 -> "Result: 25"
    EXPECT_EQ(std::any_cast<std::string>(result), "Result: 25");
}

// Test fixture for Member Function Proxies
class MemberFunctionProxyTest : public ::testing::Test {
protected:
    class TestClass {
    public:
        int addToMember(int x) { return x + member_; }
        std::string getName() const { return name_; }
        void setMember(int val) { member_ = val; }

        int member_{10};
        std::string name_{"TestClass"};
    };

    void SetUp() override {}
    void TearDown() override {}
};

// Test ProxyFunction with member functions
TEST_F(MemberFunctionProxyTest, BasicMemberFunction) {
    TestClass instance;
    ProxyFunction memberProxy(&TestClass::addToMember);

    // Test function info
    FunctionInfo info = memberProxy.getFunctionInfo();
    EXPECT_EQ(info.getReturnType(), "int");
    EXPECT_EQ(info.getArgumentTypes().size(), 1);

    // Test member function call with instance as first arg
    std::vector<std::any> args = {std::ref(instance), 5};
    std::any result = memberProxy(args);

    // 5 + 10 = 15
    EXPECT_EQ(std::any_cast<int>(result), 15);

    // Test with FunctionParams
    FunctionParams params;
    params.emplace_back("obj", std::ref(instance));
    params.emplace_back("x", 7);
    result = memberProxy(params);

    // 7 + 10 = 17
    EXPECT_EQ(std::any_cast<int>(result), 17);

    // Test with modification of instance
    ProxyFunction setterProxy(&TestClass::setMember);
    std::vector<std::any> setterArgs = {std::ref(instance), 20};
    setterProxy(setterArgs);

    // Now member_ should be 20
    EXPECT_EQ(instance.member_, 20);

    // Test again with new member value
    args = {std::ref(instance), 5};
    result = memberProxy(args);

    // 5 + 20 = 25
    EXPECT_EQ(std::any_cast<int>(result), 25);
}

// Test AsyncProxyFunction with member functions
TEST_F(MemberFunctionProxyTest, AsyncMemberFunction) {
    TestClass instance;
    AsyncProxyFunction asyncMemberProxy(&TestClass::addToMember);

    // Test async member function call
    std::vector<std::any> args = {std::ref(instance), 5};
    std::future<std::any> futureResult = asyncMemberProxy(args);
    std::any result = futureResult.get();

    // 5 + 10 = 15
    EXPECT_EQ(std::any_cast<int>(result), 15);

    // Test with const member function
    AsyncProxyFunction constMemberProxy(&TestClass::getName);
    std::vector<std::any> constArgs = {std::ref(instance)};
    futureResult = constMemberProxy(constArgs);
    result = futureResult.get();

    EXPECT_EQ(std::any_cast<std::string>(result), "TestClass");
}

// Test member function error handling
TEST_F(MemberFunctionProxyTest, MemberFunctionErrorHandling) {
    TestClass instance;
    ProxyFunction memberProxy(&TestClass::addToMember);

    // Test missing instance
    std::vector<std::any> args = {5};
    EXPECT_THROW(memberProxy(args), ProxyArgumentError);

    // Test wrong instance type
    std::string wrongInstance = "not_an_instance";
    args = {std::ref(wrongInstance), 5};
    EXPECT_THROW(memberProxy(args), ProxyTypeError);

    // Test incorrect argument count
    args = {std::ref(instance), 5, 10};
    EXPECT_THROW(memberProxy(args), ProxyArgumentError);
}

// Test with complex parameter types
class ComplexParameterTest : public ::testing::Test {
protected:
    struct ComplexStruct {
        int id;
        std::string name;
        std::vector<double> values;

        bool operator==(const ComplexStruct& other) const {
            return id == other.id && name == other.name &&
                   values == other.values;
        }
    };

    // Function that uses a complex parameter
    static ComplexStruct processComplex(const ComplexStruct& input) {
        ComplexStruct result = input;
        result.id *= 2;
        result.name = "Processed: " + input.name;
        for (auto& val : result.values) {
            val *= 1.5;
        }
        return result;
    }

    void SetUp() override {}
    void TearDown() override {}
};

// This test demonstrates a limitation - complex types require additional
// serialization/deserialization support that isn't implemented in the current
// system
TEST_F(ComplexParameterTest, DISABLED_ComplexParameterHandling) {
    // This test is disabled because the current proxy system
    // doesn't support custom types without additional serialization helpers

    ProxyFunction complexProxy(processComplex);

    ComplexStruct input;
    input.id = 42;
    input.name = "Test";
    input.values = {1.0, 2.0, 3.0};

    std::vector<std::any> args = {input};
    std::any result = complexProxy(args);

    ComplexStruct expected;
    expected.id = 84;
    expected.name = "Processed: Test";
    expected.values = {1.5, 3.0, 4.5};

    auto output = std::any_cast<ComplexStruct>(result);
    EXPECT_EQ(output, expected);
}

// Test parallel invocation for thread safety
class ThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    static int slowAdd(int a, int b) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return a + b;
    }
};

TEST_F(ThreadSafetyTest, ParallelInvocation) {
    ProxyFunction proxy(slowAdd);

    // Create multiple threads calling the same proxy
    std::vector<std::thread> threads;
    std::vector<std::future<int>> results;

    for (int i = 0; i < 10; i++) {
        std::promise<int> promise;
        results.push_back(promise.get_future());

        threads.emplace_back(
            [&proxy, i, promise = std::move(promise)]() mutable {
                try {
                    std::vector<std::any> args = {i, i * 2};
                    std::any result = proxy(args);
                    promise.set_value(std::any_cast<int>(result));
                } catch (const std::exception& e) {
                    promise.set_exception(std::current_exception());
                }
            });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Check results
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(results[i].get(), i + i * 2);
    }
}

// Test with factory functions
TEST(FactoryFunctionTest, ProxyFactoryFunctions) {
    // Test makeProxy
    auto proxy = makeProxy(add);
    std::vector<std::any> args = {5, 3};
    std::any result = proxy(args);
    EXPECT_EQ(std::any_cast<int>(result), 8);

    // Test makeAsyncProxy
    auto asyncProxy = makeAsyncProxy(add);
    std::future<std::any> futureResult = asyncProxy(args);
    result = futureResult.get();
    EXPECT_EQ(std::any_cast<int>(result), 8);

    // Test composeProxy
    auto composedProxy = composeProxy(add, [](int x) { return x * 2; });
    result = composedProxy(args);
    EXPECT_EQ(std::any_cast<int>(result), 16);  // (5+3)*2
}

}  // namespace atom::meta::test

// Main function to run the tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}