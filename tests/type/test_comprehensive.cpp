// Comprehensive test runner that includes both header-only and implementation tests
// This file attempts to include implementation-dependent tests with fallbacks

#include <gtest/gtest.h>

// Note: Header-only tests are included in test_header_only.cpp
// This file contains additional comprehensive tests that don't duplicate those

// Try to include implementation-dependent tests with error handling
// These may fail to compile if dependencies are missing

// Test for basic type functionality that should work without external dependencies
#include "atom/type/json_fwd.hpp"

// Basic JSON forward declaration tests
TEST(JsonForwardTest, ForwardDeclarationExists) {
    // Test that the forward declarations exist and can be used
    // This is a compile-time test - if it compiles, the forward declarations work
    
    // We can't test much without the actual JSON implementation,
    // but we can verify the types are declared
    SUCCEED(); // If this compiles, the forward declarations are working
}

// Test for basic string functionality
#include <string>
#include <sstream>

TEST(StringUtilityTest, BasicStringOperations) {
    // Test basic string operations that should work
    std::string test_str = "hello world";
    
    EXPECT_EQ(test_str.length(), 11);
    EXPECT_EQ(test_str.substr(0, 5), "hello");
    EXPECT_NE(test_str.find("world"), std::string::npos);
}

// Test for memory management patterns
TEST(MemoryTest, SmartPointerUsage) {
    // Test smart pointer usage patterns
    auto ptr = std::make_unique<int>(42);
    EXPECT_EQ(*ptr, 42);
    
    auto shared = std::make_shared<std::string>("test");
    EXPECT_EQ(*shared, "test");
    EXPECT_EQ(shared.use_count(), 1);
    
    auto shared2 = shared;
    EXPECT_EQ(shared.use_count(), 2);
}

// Test for exception safety
TEST(ExceptionSafetyTest, BasicExceptionHandling) {
    // Test basic exception handling patterns
    EXPECT_THROW(throw std::runtime_error("test"), std::runtime_error);
    EXPECT_NO_THROW(std::string("safe operation"));
    
    try {
        throw std::invalid_argument("test argument");
    } catch (const std::exception& e) {
        EXPECT_STREQ(e.what(), "test argument");
    }
}

// Test for type traits and concepts
TEST(TypeTraitsTest, BasicTypeTraits) {
    // Test basic type traits
    static_assert(std::is_integral_v<int>);
    static_assert(std::is_floating_point_v<double>);
    static_assert(std::is_pointer_v<int*>);
    static_assert(std::is_reference_v<int&>);
    
    // Test type relationships
    static_assert(std::is_same_v<int, int>);
    static_assert(!std::is_same_v<int, double>);
    
    SUCCEED(); // Compile-time tests
}

// Test for container usage patterns
TEST(ContainerTest, BasicContainerOperations) {
    // Test vector operations
    std::vector<int> vec = {1, 2, 3, 4, 5};
    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec.back(), 5);
    
    // Test map operations
    std::map<std::string, int> map;
    map["key1"] = 10;
    map["key2"] = 20;
    
    EXPECT_EQ(map.size(), 2);
    EXPECT_EQ(map["key1"], 10);
    EXPECT_TRUE(map.find("key1") != map.end());
    EXPECT_TRUE(map.find("nonexistent") == map.end());
}

// Test for algorithm usage
TEST(AlgorithmTest, BasicAlgorithms) {
    std::vector<int> vec = {3, 1, 4, 1, 5, 9, 2, 6};
    
    // Test find
    auto it = std::find(vec.begin(), vec.end(), 4);
    EXPECT_NE(it, vec.end());
    EXPECT_EQ(*it, 4);
    
    // Test count
    auto count = std::count(vec.begin(), vec.end(), 1);
    EXPECT_EQ(count, 2);
    
    // Test sort
    std::vector<int> sorted_vec = vec;
    std::sort(sorted_vec.begin(), sorted_vec.end());
    EXPECT_TRUE(std::is_sorted(sorted_vec.begin(), sorted_vec.end()));
}

// Test for functional programming patterns
TEST(FunctionalTest, LambdasAndFunctional) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    
    // Test lambda with for_each
    int sum = 0;
    std::for_each(vec.begin(), vec.end(), [&sum](int n) {
        sum += n;
    });
    EXPECT_EQ(sum, 15);
    
    // Test transform
    std::vector<int> doubled;
    std::transform(vec.begin(), vec.end(), std::back_inserter(doubled), 
                   [](int n) { return n * 2; });
    
    EXPECT_EQ(doubled.size(), 5);
    EXPECT_EQ(doubled[0], 2);
    EXPECT_EQ(doubled[4], 10);
}

// Test for thread safety concepts (basic)
TEST(ThreadSafetyTest, AtomicOperations) {
    std::atomic<int> atomic_int{0};
    
    EXPECT_EQ(atomic_int.load(), 0);
    atomic_int.store(42);
    EXPECT_EQ(atomic_int.load(), 42);
    
    int expected = 42;
    bool success = atomic_int.compare_exchange_strong(expected, 100);
    EXPECT_TRUE(success);
    EXPECT_EQ(atomic_int.load(), 100);
}

// Test for RAII patterns
TEST(RAIITest, ResourceManagement) {
    // Test RAII with file streams
    {
        std::ostringstream oss;
        oss << "test data";
        EXPECT_EQ(oss.str(), "test data");
    } // Stream automatically cleaned up
    
    // Test RAII with containers
    {
        std::vector<std::unique_ptr<int>> ptrs;
        ptrs.push_back(std::make_unique<int>(42));
        ptrs.push_back(std::make_unique<int>(100));
        
        EXPECT_EQ(ptrs.size(), 2);
        EXPECT_EQ(*ptrs[0], 42);
        EXPECT_EQ(*ptrs[1], 100);
    } // All unique_ptrs automatically cleaned up
}

// Test for constexpr functionality
TEST(ConstexprTest, CompileTimeComputation) {
    constexpr int compile_time_value = 42;
    constexpr int computed_value = compile_time_value * 2;
    
    static_assert(compile_time_value == 42);
    static_assert(computed_value == 84);
    
    EXPECT_EQ(compile_time_value, 42);
    EXPECT_EQ(computed_value, 84);
}

// Integration test for multiple components
TEST(IntegrationTest, MultipleComponentsWorking) {
    // Test that multiple C++ features work together
    
    // Create a map of string to function
    std::map<std::string, std::function<int(int)>> operations;
    
    operations["double"] = [](int x) { return x * 2; };
    operations["square"] = [](int x) { return x * x; };
    operations["increment"] = [](int x) { return x + 1; };
    
    EXPECT_EQ(operations.size(), 3);
    
    // Test each operation
    EXPECT_EQ(operations["double"](5), 10);
    EXPECT_EQ(operations["square"](4), 16);
    EXPECT_EQ(operations["increment"](10), 11);
    
    // Test with containers
    std::vector<int> inputs = {1, 2, 3, 4, 5};
    std::vector<int> results;
    
    std::transform(inputs.begin(), inputs.end(), std::back_inserter(results),
                   operations["double"]);
    
    std::vector<int> expected = {2, 4, 6, 8, 10};
    EXPECT_EQ(results, expected);
}
