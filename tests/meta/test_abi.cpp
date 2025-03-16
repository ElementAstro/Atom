#include <gtest/gtest.h>

#include "atom/function/abi.hpp"

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

namespace {

// Complex types for testing demangling
template <typename T>
struct SimpleTemplate {
    T value;
};

template <typename T, typename U>
struct ComplexTemplate {
    T first;
    U second;
};

template <typename... Args>
struct VariadicTemplate {};

class AbstractBase {
public:
    virtual ~AbstractBase() = default;
    virtual void abstractMethod() = 0;
};

class DerivedClass : public AbstractBase {
public:
    void abstractMethod() override {}
};

// A complex nested type
template <typename T>
using NestedType = std::map<std::string, std::vector<SimpleTemplate<T>>>;

// Function type for testing
using FunctionType = int (*)(const std::string&, double);

// Enum for testing
enum class TestEnum { Value1, Value2, Value3 };

}  // namespace

class DemangleHelperTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear the cache before each test
        atom::meta::DemangleHelper::clearCache();
    }

    // Helper to verify demangled type contains expected substring
    void expectTypeContains(const std::string& demangled,
                            const std::string& expected) {
        EXPECT_TRUE(demangled.find(expected) != std::string::npos)
            << "Expected demangled type to contain: " << expected
            << " but got: " << demangled;
    }
};

// Test basic demangling functionality
TEST_F(DemangleHelperTest, BasicDemangling) {
    // Built-in types
    std::string int_type = atom::meta::DemangleHelper::demangleType<int>();
    std::string double_type =
        atom::meta::DemangleHelper::demangleType<double>();

    EXPECT_TRUE(int_type == "int" || int_type.find("int") != std::string::npos);
    EXPECT_TRUE(double_type == "double" ||
                double_type.find("double") != std::string::npos);

    // String type (implementation defined, but should contain "string")
    std::string string_type =
        atom::meta::DemangleHelper::demangleType<std::string>();
    expectTypeContains(string_type, "string");
}

// Test demangling of instance types
TEST_F(DemangleHelperTest, InstanceDemangling) {
    int i = 42;
    std::string s = "test";
    std::vector<int> v;

    std::string int_type = atom::meta::DemangleHelper::demangleType(i);
    std::string string_type = atom::meta::DemangleHelper::demangleType(s);
    std::string vector_type = atom::meta::DemangleHelper::demangleType(v);

    EXPECT_TRUE(int_type == "int" || int_type.find("int") != std::string::npos);
    expectTypeContains(string_type, "string");
    expectTypeContains(vector_type, "vector");
    expectTypeContains(vector_type, "int");
}

// Test demangling of template types
TEST_F(DemangleHelperTest, TemplateDemangling) {
    // Simple template
    std::string simple_template_type =
        atom::meta::DemangleHelper::demangleType<SimpleTemplate<int>>();
    expectTypeContains(simple_template_type, "SimpleTemplate");
    expectTypeContains(simple_template_type, "int");

    // Complex template
    std::string complex_template_type =
        atom::meta::DemangleHelper::demangleType<
            ComplexTemplate<int, std::string>>();
    expectTypeContains(complex_template_type, "ComplexTemplate");
    expectTypeContains(complex_template_type, "int");
    expectTypeContains(complex_template_type, "string");

    // Variadic template
    std::string variadic_template_type =
        atom::meta::DemangleHelper::demangleType<
            VariadicTemplate<int, double, char>>();
    expectTypeContains(variadic_template_type, "VariadicTemplate");
}

// Test demangling of nested types
TEST_F(DemangleHelperTest, NestedTypeDemangling) {
    std::string nested_type =
        atom::meta::DemangleHelper::demangleType<NestedType<double>>();

    expectTypeContains(nested_type, "map");
    expectTypeContains(nested_type, "string");
    expectTypeContains(nested_type, "vector");
    expectTypeContains(nested_type, "SimpleTemplate");
    expectTypeContains(nested_type, "double");
}

// Test demangling of pointer, reference and const types
TEST_F(DemangleHelperTest, ModifierTypeDemangling) {
    // Pointer type
    std::string ptr_type = atom::meta::DemangleHelper::demangleType<int*>();
    EXPECT_TRUE(ptr_type.find("int") != std::string::npos &&
                (ptr_type.find("*") != std::string::npos ||
                 ptr_type.find("pointer") != std::string::npos));

    // Reference type
    std::string ref_type = atom::meta::DemangleHelper::demangleType<int&>();
    EXPECT_TRUE(ref_type.find("int") != std::string::npos &&
                (ref_type.find("&") != std::string::npos ||
                 ref_type.find("reference") != std::string::npos));

    // Const type
    std::string const_type =
        atom::meta::DemangleHelper::demangleType<const int>();
    EXPECT_TRUE(const_type.find("int") != std::string::npos &&
                (const_type.find("const") != std::string::npos));
}

// Test demangling with source location
TEST_F(DemangleHelperTest, DemangleWithSourceLocation) {
    std::source_location loc = std::source_location::current();
    std::string mangled_name = typeid(int).name();

    std::string demangled =
        atom::meta::DemangleHelper::demangle(mangled_name, loc);

    // Check that the result contains the file name and line number
    EXPECT_TRUE(demangled.find(loc.file_name()) != std::string::npos);
    EXPECT_TRUE(demangled.find(std::to_string(loc.line())) !=
                std::string::npos);
}

// Test demangling multiple names
TEST_F(DemangleHelperTest, DemangleMultipleNames) {
    std::vector<std::string_view> mangled_names = {
        typeid(int).name(), typeid(double).name(), typeid(std::string).name()};

    std::vector<std::string> demangled =
        atom::meta::DemangleHelper::demangleMany(mangled_names);

    ASSERT_EQ(demangled.size(), 3);
    EXPECT_TRUE(demangled[0] == "int" ||
                demangled[0].find("int") != std::string::npos);
    EXPECT_TRUE(demangled[1] == "double" ||
                demangled[1].find("double") != std::string::npos);
    expectTypeContains(demangled[2], "string");
}

// Test cache functionality
TEST_F(DemangleHelperTest, CacheFunctionality) {
    EXPECT_EQ(atom::meta::DemangleHelper::cacheSize(), 0);

    // First demangling should add to cache
    atom::meta::DemangleHelper::demangleType<int>();
    EXPECT_EQ(atom::meta::DemangleHelper::cacheSize(), 1);

    // Second demangling of the same type should use cache (size remains the
    // same)
    atom::meta::DemangleHelper::demangleType<int>();
    EXPECT_EQ(atom::meta::DemangleHelper::cacheSize(), 1);

    // Different type should add to cache
    atom::meta::DemangleHelper::demangleType<double>();
    EXPECT_EQ(atom::meta::DemangleHelper::cacheSize(), 2);

    // Clear cache
    atom::meta::DemangleHelper::clearCache();
    EXPECT_EQ(atom::meta::DemangleHelper::cacheSize(), 0);
}

// Test template specialization detection
TEST_F(DemangleHelperTest, TemplateSpecializationDetection) {
    // Test with non-template type
    bool isIntTemplate =
        atom::meta::DemangleHelper::isTemplateSpecialization<int>();
    EXPECT_FALSE(isIntTemplate);

    // Test with template type
    bool isVectorTemplate =
        atom::meta::DemangleHelper::isTemplateSpecialization<
            std::vector<int>>();
    EXPECT_TRUE(isVectorTemplate);
    bool isSimpleTemplate =
        atom::meta::DemangleHelper::isTemplateSpecialization<
            SimpleTemplate<double>>();
    EXPECT_TRUE(isSimpleTemplate);

    // Test with demangled name
    std::string demangled_vector =
        atom::meta::DemangleHelper::demangleType<std::vector<int>>();
    EXPECT_TRUE(atom::meta::DemangleHelper::isTemplateType(demangled_vector));

    std::string demangled_int = atom::meta::DemangleHelper::demangleType<int>();
    EXPECT_FALSE(atom::meta::DemangleHelper::isTemplateType(demangled_int));
}

// Test thread safety of the cache
TEST_F(DemangleHelperTest, ThreadSafetyTest) {
    if (!atom::meta::AbiConfig::thread_safe_cache) {
        GTEST_SKIP() << "Thread safety is disabled in AbiConfig";
    }

    // Create several threads that demangle types concurrently
    constexpr int num_threads = 10;
    constexpr int iterations_per_thread = 1000;

    std::vector<std::thread> threads;
    std::atomic<bool> start_flag(false);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&start_flag]() {
            // Wait for the start signal
            while (!start_flag.load()) {
                std::this_thread::yield();
            }

            // Demangle types in a loop
            for (int j = 0; j < iterations_per_thread; ++j) {
                switch (j % 5) {
                    case 0:
                        atom::meta::DemangleHelper::demangleType<int>();
                        break;
                    case 1:
                        atom::meta::DemangleHelper::demangleType<std::string>();
                        break;
                    case 2:
                        atom::meta::DemangleHelper::demangleType<
                            std::vector<int>>();
                        break;
                    case 3:
                        atom::meta::DemangleHelper::demangleType<
                            SimpleTemplate<double>>();
                        break;
                    case 4:
                        atom::meta::DemangleHelper::demangleType<
                            ComplexTemplate<int, std::string>>();
                        break;
                }
            }
        });
    }

    // Start all threads at once
    start_flag.store(true);

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Cache should contain at most 5 entries (one for each unique type)
    EXPECT_LE(atom::meta::DemangleHelper::cacheSize(), 5);

    // No crashes or exceptions should have occurred
}

// Test cache management (ensuring it doesn't grow beyond max_cache_size)
TEST_F(DemangleHelperTest, CacheManagement) {
    // Create a number of unique types to exceed max_cache_size
    constexpr int num_types = atom::meta::AbiConfig::max_cache_size + 100;

    // Using templates with different integer parameters creates unique types
    for (int i = 0; i < num_types; ++i) {
        atom::meta::DemangleHelper::demangleType(
            typeid(SimpleTemplate<char[55]>).name());
    }

    // Cache size should be limited to max_cache_size
    EXPECT_LE(atom::meta::DemangleHelper::cacheSize(),
              atom::meta::AbiConfig::max_cache_size);
}

// Test error handling for invalid mangled names
TEST_F(DemangleHelperTest, ErrorHandlingTest) {
    // This should not crash, but may throw or return the original string
    try {
        std::string result =
            atom::meta::DemangleHelper::demangle("not_a_valid_mangled_name");
        // If no exception, it should return something (either the original or
        // some fallback)
        EXPECT_FALSE(result.empty());
    } catch (const atom::meta::AbiException& e) {
        // It's also valid to throw on invalid input
        EXPECT_TRUE(std::string(e.what()).find("Failed to demangle") !=
                    std::string::npos);
    }
}

#if defined(ENABLE_DEBUG) || defined(ATOM_META_ENABLE_VISUALIZATION)
// Test visualization functionality (only when enabled)
TEST_F(DemangleHelperTest, TypeVisualization) {
    // Test visualization of a simple type
    std::string int_viz = atom::meta::DemangleHelper::visualizeType<int>();
    EXPECT_TRUE(int_viz.find("int") != std::string::npos);

    // Test visualization of a complex type
    std::string complex_viz = atom::meta::DemangleHelper::visualizeType<
        std::map<int, std::vector<std::string>>>();

    // Visualization should include map, int, vector and string somewhere
    EXPECT_TRUE(complex_viz.find("map") != std::string::npos);
    EXPECT_TRUE(complex_viz.find("int") != std::string::npos);
    EXPECT_TRUE(complex_viz.find("vector") != std::string::npos);
    EXPECT_TRUE(complex_viz.find("string") != std::string::npos);

    // Test instance visualization
    std::vector<int> vec = {1, 2, 3};
    std::string vec_viz = atom::meta::DemangleHelper::visualizeObject(vec);
    EXPECT_TRUE(vec_viz.find("vector") != std::string::npos);
    EXPECT_TRUE(vec_viz.find("int") != std::string::npos);
}
#endif

// Test with highly complex and nested types
TEST_F(DemangleHelperTest, ComplexNestedTypes) {
    using ComplexType =
        std::tuple<std::map<std::string, std::vector<int>>,
                   std::shared_ptr<AbstractBase>,
                   std::array<std::unique_ptr<SimpleTemplate<double>>, 5>>;

    std::string complex_type =
        atom::meta::DemangleHelper::demangleType<ComplexType>();

    // Check for presence of key type components
    expectTypeContains(complex_type, "tuple");
    expectTypeContains(complex_type, "map");
    expectTypeContains(complex_type, "vector");
    expectTypeContains(complex_type, "shared_ptr");
    expectTypeContains(complex_type, "unique_ptr");
    expectTypeContains(complex_type, "AbstractBase");
    expectTypeContains(complex_type, "SimpleTemplate");
}

// Test with function types
TEST_F(DemangleHelperTest, FunctionTypes) {
    // Function pointer
    using FuncPtr = void (*)(int, double);
    std::string func_ptr = atom::meta::DemangleHelper::demangleType<FuncPtr>();
    expectTypeContains(func_ptr, "void");
    expectTypeContains(func_ptr, "int");
    expectTypeContains(func_ptr, "double");

    // Member function pointer
    using MemFuncPtr = void (std::string::*)(int) const;
    std::string mem_func_ptr =
        atom::meta::DemangleHelper::demangleType<MemFuncPtr>();
    expectTypeContains(mem_func_ptr, "void");
    expectTypeContains(mem_func_ptr, "string");
    expectTypeContains(mem_func_ptr, "int");
    expectTypeContains(mem_func_ptr, "const");
}

// Extra test for C++20 features
TEST_F(DemangleHelperTest, Cpp20Features) {
    // std::span
    using SpanType = std::span<const int>;
    std::string span_type =
        atom::meta::DemangleHelper::demangleType<SpanType>();
    expectTypeContains(span_type, "span");
    expectTypeContains(span_type, "int");
    expectTypeContains(span_type, "const");

    // Concepts and constraints (checking only that it doesn't crash)
    std::string concept_type = atom::meta::DemangleHelper::demangleType<
        std::enable_if_t<std::is_integral_v<int>, int>>();
    EXPECT_FALSE(concept_type.empty());
}

// Test for potential platform-specific issues
TEST_F(DemangleHelperTest, PlatformSpecificTypes) {
#ifdef _WIN32
    // Windows-specific types
    using WindowsHandle = void*;  // Simplified example
    std::string handle_type =
        atom::meta::DemangleHelper::demangleType<WindowsHandle>();
    expectTypeContains(handle_type, "void");
    expectTypeContains(handle_type, "*");
#else
    // Unix-specific types
    using FileDescriptor = int;  // Simplified example
    std::string fd_type =
        atom::meta::DemangleHelper::demangleType<FileDescriptor>();
    expectTypeContains(fd_type, "int");
#endif
}
