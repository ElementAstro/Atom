// filepath: /home/max/Atom-1/atom/utils/test_to_string.cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <array>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

#include "to_string.hpp"

using namespace atom::utils;
using ::testing::HasSubstr;
using ::testing::StartsWith;
using ::testing::EndsWith;

// Helper classes for testing
class StreamableClass {
public:
    int value;
    explicit StreamableClass(int val) : value(val) {}
    
    friend std::ostream& operator<<(std::ostream& os, const StreamableClass& obj) {
        os << "StreamableClass(" << obj.value << ")";
        return os;
    }
};

class NonStreamableClass {
public:
    int value;
    explicit NonStreamableClass(int val) : value(val) {}
    // No stream operator
};

enum class TestEnum { One = 1, Two = 2, Three = 3 };

// The test fixture
class ToStringTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }
    
    void TearDown() override {
        // Clean up test environment
    }
};

// Test string type conversions
TEST_F(ToStringTest, StringTypes) {
    // std::string
    std::string str = "hello";
    EXPECT_EQ(toString(str), "hello");
    
    // const char*
    const char* cstr = "hello";
    EXPECT_EQ(toString(cstr), "hello");
    
    // char*
    char mutable_str[] = "hello";
    EXPECT_EQ(toString(mutable_str), "hello");
    
    // std::string_view
    std::string_view str_view = "hello";
    EXPECT_EQ(toString(str_view), "hello");
    
    // Null C-string pointer
    const char* null_str = nullptr;
    EXPECT_EQ(toString(null_str), "null");
    
    // Empty string
    EXPECT_EQ(toString(""), "");
}

// Test character conversion
TEST_F(ToStringTest, CharType) {
    EXPECT_EQ(toString('A'), "A");
    EXPECT_EQ(toString(' '), " ");
    EXPECT_EQ(toString('\n'), "\n");
}

// Test enum type conversion
TEST_F(ToStringTest, EnumType) {
    EXPECT_EQ(toString(TestEnum::One), "1");
    EXPECT_EQ(toString(TestEnum::Two), "2");
    EXPECT_EQ(toString(TestEnum::Three), "3");
}

// Test pointer type conversion
TEST_F(ToStringTest, PointerType) {
    int value = 42;
    int* ptr = &value;
    std::string result = toString(ptr);
    
    EXPECT_THAT(result, StartsWith("Pointer("));
    EXPECT_THAT(result, HasSubstr("42"));
    
    // Null pointer
    int* null_ptr = nullptr;
    EXPECT_EQ(toString(null_ptr), "nullptr");
    
    // Pointer to complex type
    std::string str = "test";
    std::string* str_ptr = &str;
    result = toString(str_ptr);
    EXPECT_THAT(result, HasSubstr("test"));
}

// Test smart pointer type conversion
TEST_F(ToStringTest, SmartPointerType) {
    auto shared_ptr = std::make_shared<int>(42);
    std::string result = toString(shared_ptr);
    
    EXPECT_THAT(result, StartsWith("SmartPointer("));
    EXPECT_THAT(result, HasSubstr("42"));
    
    // Null smart pointer
    std::shared_ptr<int> null_shared_ptr;
    EXPECT_EQ(toString(null_shared_ptr), "nullptr");
    
    // Unique pointer
    auto unique_ptr = std::make_unique<int>(123);
    result = toString(unique_ptr);
    EXPECT_THAT(result, StartsWith("SmartPointer("));
    EXPECT_THAT(result, HasSubstr("123"));
}

// Test container type conversion (vector)
TEST_F(ToStringTest, VectorContainer) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::string result = toString(vec);
    
    EXPECT_EQ(result, "[1, 2, 3, 4, 5]");
    
    // Empty vector
    std::vector<int> empty_vec;
    EXPECT_EQ(toString(empty_vec), "[]");
    
    // Vector with custom separator
    EXPECT_EQ(toString(vec, " | "), "[1 | 2 | 3 | 4 | 5]");
    
    // Vector of strings
    std::vector<std::string> str_vec = {"hello", "world"};
    EXPECT_EQ(toString(str_vec), "[hello, world]");
    
    // Nested vector
    std::vector<std::vector<int>> nested_vec = {{1, 2}, {3, 4}};
    EXPECT_EQ(toString(nested_vec), "[[1, 2], [3, 4]]");
}

// Test container type conversion (list)
TEST_F(ToStringTest, ListContainer) {
    std::list<int> list = {1, 2, 3, 4, 5};
    EXPECT_EQ(toString(list), "[1, 2, 3, 4, 5]");
    
    // Empty list
    std::list<int> empty_list;
    EXPECT_EQ(toString(empty_list), "[]");
}

// Test container type conversion (set)
TEST_F(ToStringTest, SetContainer) {
    std::set<int> set = {5, 3, 1, 4, 2};
    // Set will be ordered
    EXPECT_EQ(toString(set), "[1, 2, 3, 4, 5]");
    
    // Empty set
    std::set<int> empty_set;
    EXPECT_EQ(toString(empty_set), "[]");
}

// Test map type conversion
TEST_F(ToStringTest, MapType) {
    std::map<int, std::string> map = {{1, "one"}, {2, "two"}, {3, "three"}};
    std::string result = toString(map);
    
    EXPECT_EQ(result, "{1: one, 2: two, 3: three}");
    
    // Empty map
    std::map<int, std::string> empty_map;
    EXPECT_EQ(toString(empty_map), "{}");
    
    // Map with custom separator
    EXPECT_EQ(toString(map, " | "), "{1: one | 2: two | 3: three}");
    
    // Map with string keys
    std::map<std::string, int> str_map = {{"one", 1}, {"two", 2}, {"three", 3}};
    EXPECT_EQ(toString(str_map), "{one: 1, three: 3, two: 2}");
    
    // Nested map
    std::map<int, std::map<int, std::string>> nested_map = {
        {1, {{1, "one-one"}, {2, "one-two"}}},
        {2, {{1, "two-one"}, {2, "two-two"}}}
    };
    EXPECT_EQ(toString(nested_map), "{1: {1: one-one, 2: one-two}, 2: {1: two-one, 2: two-two}}");
    
    // Unordered map (order is not guaranteed, so just check length and specific elements)
    std::unordered_map<int, std::string> umap = {{1, "one"}, {2, "two"}, {3, "three"}};
    result = toString(umap);
    EXPECT_THAT(result, StartsWith("{"));
    EXPECT_THAT(result, EndsWith("}"));
    EXPECT_THAT(result, HasSubstr("1: one"));
    EXPECT_THAT(result, HasSubstr("2: two"));
    EXPECT_THAT(result, HasSubstr("3: three"));
}

// Test array type conversion
TEST_F(ToStringTest, ArrayType) {
    std::array<int, 5> arr = {1, 2, 3, 4, 5};
    EXPECT_EQ(toString(arr), "[1, 2, 3, 4, 5]");
    
    // Empty array
    std::array<int, 0> empty_arr = {};
    EXPECT_EQ(toString(empty_arr), "[]");
}

// Test tuple type conversion
TEST_F(ToStringTest, TupleType) {
    auto tuple = std::make_tuple(1, "hello", 3.14);
    EXPECT_EQ(toString(tuple), "(1, hello, 3.140000)");
    
    // Empty tuple
    auto empty_tuple = std::make_tuple();
    EXPECT_EQ(toString(empty_tuple), "()");
    
    // Single element tuple
    auto single_tuple = std::make_tuple(42);
    EXPECT_EQ(toString(single_tuple), "(42)");
    
    // Tuple with custom separator
    EXPECT_EQ(toString(tuple, " - "), "(1 - hello - 3.140000)");
    
    // Nested tuple
    auto nested_tuple = std::make_tuple(std::make_tuple(1, 2), std::make_tuple("a", "b"));
    EXPECT_EQ(toString(nested_tuple), "((1, 2), (a, b))");
}

// Test optional type conversion
TEST_F(ToStringTest, OptionalType) {
    std::optional<int> opt = 42;
    EXPECT_EQ(toString(opt), "Optional(42)");
    
    // Empty optional
    std::optional<int> empty_opt;
    EXPECT_EQ(toString(empty_opt), "nullopt");
    
    // Optional with complex type
    std::optional<std::vector<int>> opt_vec = std::vector<int>{1, 2, 3};
    EXPECT_EQ(toString(opt_vec), "Optional([1, 2, 3])");
}

// Test variant type conversion
TEST_F(ToStringTest, VariantType) {
    std::variant<int, std::string, double> var = 42;
    EXPECT_EQ(toString(var), "42");
    
    var = "hello";
    EXPECT_EQ(toString(var), "hello");
    
    var = 3.14;
    EXPECT_EQ(toString(var), "3.140000");
    
    // Variant with complex type
    std::variant<int, std::vector<int>> var2 = std::vector<int>{1, 2, 3};
    EXPECT_EQ(toString(var2), "[1, 2, 3]");
}

// Test general types conversion (using std::to_string)
TEST_F(ToStringTest, GeneralTypesStdToString) {
    // Integer
    EXPECT_EQ(toString(42), "42");
    EXPECT_EQ(toString(-42), "-42");
    
    // Float/Double
    EXPECT_EQ(toString(3.14f), "3.140000");
    EXPECT_EQ(toString(-3.14), "-3.140000");
    
    // Boolean
    EXPECT_EQ(toString(true), "1");
    EXPECT_EQ(toString(false), "0");
}

// Test general types conversion (using stream operator)
TEST_F(ToStringTest, GeneralTypesStreamable) {
    StreamableClass obj(42);
    EXPECT_EQ(toString(obj), "StreamableClass(42)");
}

// Test error handling
TEST_F(ToStringTest, ErrorHandling) {
    // Error in container elements
    std::vector<std::shared_ptr<int>> vec = {
        std::make_shared<int>(1),
        nullptr,
        std::make_shared<int>(3)
    };
    
    std::string result = toString(vec);
    EXPECT_THAT(result, HasSubstr("[SmartPointer"));
    EXPECT_THAT(result, HasSubstr("nullptr"));
    
    // Exception in conversion should be caught and reported
    try {
        // This would cause a static_assert failure in actual code
        //toString(NonStreamableClass(42));
        
        // Instead, simulate a conversion error
        throw ToStringException("Test exception");
    } catch (const ToStringException& e) {
        std::string error = e.what();
        EXPECT_THAT(error, StartsWith("ToString conversion error"));
    }
}

// Test toStringArray function
TEST_F(ToStringTest, ToStringArray) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    
    // Default separator (space)
    EXPECT_EQ(toStringArray(vec), "1 2 3 4 5");
    
    // Custom separator
    EXPECT_EQ(toStringArray(vec, ", "), "1, 2, 3, 4, 5");
    
    // Empty array
    std::vector<int> empty_vec;
    EXPECT_EQ(toStringArray(empty_vec), "");
    
    // Array with complex types
    std::vector<std::vector<int>> nested_vec = {{1, 2}, {3, 4}};
    EXPECT_EQ(toStringArray(nested_vec), "[1, 2] [3, 4]");
}

// Test toStringRange function
TEST_F(ToStringTest, ToStringRange) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    
    // Default separator
    EXPECT_EQ(toStringRange(vec.begin(), vec.end()), "[1, 2, 3, 4, 5]");
    
    // Custom separator
    EXPECT_EQ(toStringRange(vec.begin(), vec.end(), " | "), "[1 | 2 | 3 | 4 | 5]");
    
    // Empty range
    EXPECT_EQ(toStringRange(vec.begin(), vec.begin()), "[]");
    
    // Partial range
    EXPECT_EQ(toStringRange(vec.begin() + 1, vec.begin() + 4), "[2, 3, 4]");
}

// Test joinCommandLine function
TEST_F(ToStringTest, JoinCommandLine) {
    // Basic join
    EXPECT_EQ(joinCommandLine("program", "-f", "file.txt"), "program -f file.txt");
    
    // Join with mixed types
    EXPECT_EQ(joinCommandLine("program", 42, 3.14, true), "program 42 3.140000 1");
    
    // Join with no arguments
    EXPECT_EQ(joinCommandLine(), "");
    
    // Join with single argument
    EXPECT_EQ(joinCommandLine("program"), "program");
}

// Test with deque container
TEST_F(ToStringTest, DequeContainer) {
    std::deque<int> deq = {1, 2, 3, 4, 5};
    EXPECT_EQ(toString(deq), "[1, 2, 3, 4, 5]");
    
    // Empty deque
    std::deque<int> empty_deq;
    EXPECT_EQ(toString(empty_deq), "[]");
}

// Test with custom delimiters
TEST_F(ToStringTest, CustomDelimiters) {
    std::vector<int> vec = {1, 2, 3};
    EXPECT_EQ(toString(vec, " -> "), "[1 -> 2 -> 3]");
    
    std::map<int, std::string> map = {{1, "one"}, {2, "two"}};
    EXPECT_EQ(toString(map, " => "), "{1: one => 2: two}");
    
    auto tuple = std::make_tuple(1, "hello", 3.14);
    EXPECT_EQ(toString(tuple, "; "), "(1; hello; 3.140000)");
}

// Test nested complex structures
TEST_F(ToStringTest, NestedComplexStructures) {
    // Map of vectors
    std::map<int, std::vector<int>> map_of_vecs = {
        {1, {1, 2, 3}},
        {2, {4, 5, 6}}
    };
    EXPECT_EQ(toString(map_of_vecs), "{1: [1, 2, 3], 2: [4, 5, 6]}");
    
    // Vector of optionals
    std::vector<std::optional<int>> vec_of_opts = {
        std::optional<int>{1},
        std::optional<int>{},
        std::optional<int>{3}
    };
    EXPECT_EQ(toString(vec_of_opts), "[Optional(1), nullopt, Optional(3)]");
    
    // Optional of vector
    std::optional<std::vector<int>> opt_vec = std::vector<int>{1, 2, 3};
    EXPECT_EQ(toString(opt_vec), "Optional([1, 2, 3])");
    
    // Variant of container
    std::variant<int, std::vector<int>> var_vec = std::vector<int>{1, 2, 3};
    EXPECT_EQ(toString(var_vec), "[1, 2, 3]");
    
    // Tuple with complex elements
    auto complex_tuple = std::make_tuple(
        std::vector<int>{1, 2, 3},
        std::map<int, std::string>{{1, "one"}, {2, "two"}},
        std::optional<int>{42}
    );
    EXPECT_EQ(toString(complex_tuple), "([1, 2, 3], {1: one, 2: two}, Optional(42))");
}

// Test with pointers to containers
TEST_F(ToStringTest, PointersToContainers) {
    auto vec_ptr = std::make_shared<std::vector<int>>(std::vector<int>{1, 2, 3});
    std::string result = toString(vec_ptr);
    
    EXPECT_THAT(result, StartsWith("SmartPointer("));
    EXPECT_THAT(result, HasSubstr("[1, 2, 3]"));
    
    // Raw pointer to container
    std::vector<int> vec = {1, 2, 3};
    std::vector<int>* raw_ptr = &vec;
    result = toString(raw_ptr);
    
    EXPECT_THAT(result, StartsWith("Pointer("));
    EXPECT_THAT(result, HasSubstr("[1, 2, 3]"));
}

// Test with error cases in containers
TEST_F(ToStringTest, ErrorInContainers) {
    // Create a vector with some elements that would cause errors if converted directly
    std::vector<std::shared_ptr<int>> vec = {
        std::make_shared<int>(1),
        nullptr,
        std::make_shared<int>(3)
    };
    
    std::string result = toString(vec);
    EXPECT_THAT(result, HasSubstr("[SmartPointer"));
    EXPECT_THAT(result, HasSubstr("nullptr"));
    EXPECT_THAT(result, HasSubstr("1"));
    EXPECT_THAT(result, HasSubstr("3"));
}

// Test with C-arrays
TEST_F(ToStringTest, CArrays) {
    int arr[] = {1, 2, 3, 4, 5};
    // C-arrays need to be wrapped in a container view like span
    std::span<int> arr_span(arr, 5);
    EXPECT_EQ(toString(arr_span), "[1, 2, 3, 4, 5]");
}

// Test with recursive structures
TEST_F(ToStringTest, RecursiveStructures) {
    // Create a linked structure (will not cause infinite recursion due to pointer handling)
    struct Node {
        int value;
        std::shared_ptr<Node> next;
    };
    
    auto node1 = std::make_shared<Node>();
    auto node2 = std::make_shared<Node>();
    auto node3 = std::make_shared<Node>();
    
    node1->value = 1;
    node1->next = node2;
    
    node2->value = 2;
    node2->next = node3;
    
    node3->value = 3;
    node3->next = nullptr;
    
    std::string result = toString(node1);
    EXPECT_THAT(result, HasSubstr("SmartPointer"));
    EXPECT_THAT(result, HasSubstr("1"));
    EXPECT_THAT(result, HasSubstr("2"));
    EXPECT_THAT(result, HasSubstr("3"));
    EXPECT_THAT(result, HasSubstr("nullptr"));
}

// Performance test for large structures
TEST_F(ToStringTest, LargeStructurePerformance) {
    // Generate a large vector
    std::vector<int> large_vec(10000);
    for (int i = 0; i < 10000; i++) {
        large_vec[i] = i;
    }
    
    // Measure time to convert to string
    auto start = std::chrono::high_resolution_clock::now();
    std::string result = toString(large_vec);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // Verify correct conversion
    EXPECT_THAT(result, StartsWith("[0, 1, 2"));
    EXPECT_THAT(result, EndsWith("9998, 9999]"));
    
    // Just log performance - no strict assertion as it will vary by system
    std::cout << "Converted vector of 10000 elements in " << duration_ms << "ms" << std::endl;
}

// Test with standard library container adaptors
TEST_F(ToStringTest, ContainerAdaptors) {
    // stack, queue, and priority_queue don't satisfy the Container concept directly
    // but their underlying containers do when accessed properly
    
    // For testing purposes, we can use a custom adaptor wrapper
    std::vector<int> vec = {1, 2, 3, 4, 5};
    
    // Test with a custom wrapper that simulates container adaptors
    struct AdaptorWrapper {
        std::vector<int>& container;
        
        auto begin() const { return container.begin(); }
        auto end() const { return container.end(); }
    };
    
    AdaptorWrapper wrapper{vec};
    EXPECT_EQ(toString(wrapper), "[1, 2, 3, 4, 5]");
}

// Real-world complex example
TEST_F(ToStringTest, RealWorldExample) {
    // Create a complex data structure that might be used in real-world applications
    std::map<std::string, std::variant<
        int,
        std::string,
        std::vector<int>,
        std::map<std::string, std::optional<double>>
    >> complex_data = {
        {"int_value", 42},
        {"string_value", std::string("hello world")},
        {"vector_value", std::vector<int>{1, 2, 3}},
        {"map_value", std::map<std::string, std::optional<double>>{
            {"present", 3.14},
            {"absent", std::nullopt}
        }}
    };
    
    std::string result = toString(complex_data);
    
    // Check for key components in the result
    EXPECT_THAT(result, HasSubstr("int_value: 42"));
    EXPECT_THAT(result, HasSubstr("string_value: hello world"));
    EXPECT_THAT(result, HasSubstr("vector_value: [1, 2, 3]"));
    EXPECT_THAT(result, HasSubstr("map_value: {"));
    EXPECT_THAT(result, HasSubstr("present: Optional(3.140000)"));
    EXPECT_THAT(result, HasSubstr("absent: nullopt"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}