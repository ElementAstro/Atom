/*!
 * \file test_abi.cpp
 * \brief Comprehensive tests for atom::meta::DemangleHelper and ABI utilities
 * \author Max Qian <lightapt.com>
 * \date 2024
 * \copyright Copyright (C) 2023-2024 Max Qian
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/meta/abi.hpp"

#include <atomic>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace atom::meta::test {

// Test helper classes
template <typename T>
struct SimpleTemplate {
    T value;
};

template <typename T, typename U>
struct ComplexTemplate {
    T first;
    U second;
};

class AbstractBase {
public:
    virtual ~AbstractBase() = default;
    virtual int getValue() const = 0;
};

class ConcreteClass : public AbstractBase {
public:
    int getValue() const override { return 42; }
};

// Test fixture
class DemangleHelperTest : public ::testing::Test {
protected:
    void SetUp() override { DemangleHelper::clearCache(); }

    void TearDown() override { DemangleHelper::clearCache(); }

    // Helper to check type name contains expected substring
    void expectTypeContains(const String& typeName,
                            const std::string& expected) {
        std::string typeStr(typeName.begin(), typeName.end());
        EXPECT_NE(typeStr.find(expected), std::string::npos)
            << "Type '" << typeStr << "' should contain '" << expected << "'";
    }
};

//==============================================================================
// Basic Demangling Tests
//==============================================================================

TEST_F(DemangleHelperTest, DemangleBasicTypes) {
    auto intType = DemangleHelper::demangleType<int>();
    auto doubleType = DemangleHelper::demangleType<double>();
    auto charType = DemangleHelper::demangleType<char>();

    expectTypeContains(intType, "int");
    expectTypeContains(doubleType, "double");
    expectTypeContains(charType, "char");
}

TEST_F(DemangleHelperTest, DemanglePointerTypes) {
    auto intPtr = DemangleHelper::demangleType<int*>();
    auto constIntPtr = DemangleHelper::demangleType<const int*>();

    expectTypeContains(intPtr, "int");
    expectTypeContains(constIntPtr, "int");
}

TEST_F(DemangleHelperTest, DemangleReferenceTypes) {
    auto intRef = DemangleHelper::demangleType<int&>();
    auto intRvalueRef = DemangleHelper::demangleType<int&&>();

    expectTypeContains(intRef, "int");
    expectTypeContains(intRvalueRef, "int");
}

TEST_F(DemangleHelperTest, DemangleStdTypes) {
    auto stringType = DemangleHelper::demangleType<std::string>();
    auto vectorType = DemangleHelper::demangleType<std::vector<int>>();
    auto mapType = DemangleHelper::demangleType<std::map<int, std::string>>();

    expectTypeContains(stringType, "string");
    expectTypeContains(vectorType, "vector");
    expectTypeContains(mapType, "map");
}

TEST_F(DemangleHelperTest, DemangleTemplateTypes) {
    auto simpleTemplate = DemangleHelper::demangleType<SimpleTemplate<int>>();
    auto complexTemplate =
        DemangleHelper::demangleType<ComplexTemplate<int, double>>();

    expectTypeContains(simpleTemplate, "SimpleTemplate");
    expectTypeContains(complexTemplate, "ComplexTemplate");
}

TEST_F(DemangleHelperTest, DemangleFromInstance) {
    int intVal = 42;
    std::string strVal = "test";
    std::vector<int> vecVal = {1, 2, 3};

    auto intType = DemangleHelper::demangleType(intVal);
    auto strType = DemangleHelper::demangleType(strVal);
    auto vecType = DemangleHelper::demangleType(vecVal);

    expectTypeContains(intType, "int");
    expectTypeContains(strType, "string");
    expectTypeContains(vecType, "vector");
}

//==============================================================================
// Demangle with Source Location Tests
//==============================================================================

TEST_F(DemangleHelperTest, DemangleWithSourceLocation) {
    auto loc = std::source_location::current();
    auto result = DemangleHelper::demangle(typeid(int).name(), loc);

    // Result should contain the source location info
    std::string resultStr(result.begin(), result.end());
    EXPECT_NE(resultStr.find("("), std::string::npos);
}

TEST_F(DemangleHelperTest, DemangleWithoutSourceLocation) {
    auto result = DemangleHelper::demangle(typeid(int).name());

    // Result should not contain parentheses for location
    expectTypeContains(result, "int");
}

//==============================================================================
// DemangleMany Tests
//==============================================================================

TEST_F(DemangleHelperTest, DemangleManyNames) {
    containers::Vector<std::string_view> names = {
        typeid(int).name(), typeid(double).name(), typeid(std::string).name()};

    auto results = DemangleHelper::demangleMany(names);

    ASSERT_EQ(results.size(), 3);
    expectTypeContains(results[0], "int");
    expectTypeContains(results[1], "double");
    expectTypeContains(results[2], "string");
}

//==============================================================================
// Cache Tests
//==============================================================================

TEST_F(DemangleHelperTest, CacheOperations) {
    // Initially empty
    EXPECT_EQ(DemangleHelper::cacheSize(), 0);

    // Demangle some types
    DemangleHelper::demangleType<int>();
    DemangleHelper::demangleType<double>();
    DemangleHelper::demangleType<std::string>();

    EXPECT_GE(DemangleHelper::cacheSize(), 1);

    // Clear cache
    DemangleHelper::clearCache();
    EXPECT_EQ(DemangleHelper::cacheSize(), 0);
}

TEST_F(DemangleHelperTest, CacheReuse) {
    // First call should populate cache
    auto first = DemangleHelper::demangleType<int>();
    size_t sizeAfterFirst = DemangleHelper::cacheSize();

    // Second call should use cache
    auto second = DemangleHelper::demangleType<int>();
    size_t sizeAfterSecond = DemangleHelper::cacheSize();

    EXPECT_EQ(first, second);
    EXPECT_EQ(sizeAfterFirst, sizeAfterSecond);
}

//==============================================================================
// Template Detection Tests
//==============================================================================

TEST_F(DemangleHelperTest, IsTemplateSpecialization) {
    // This is a compile-time check
    auto isTemplate =
        DemangleHelper::isTemplateSpecialization<std::vector<int>>();
    auto isNotTemplate = DemangleHelper::isTemplateSpecialization<int>();

    EXPECT_TRUE(isTemplate);
    EXPECT_FALSE(isNotTemplate);
}

TEST_F(DemangleHelperTest, IsTemplateType) {
    auto vectorName = DemangleHelper::demangleType<std::vector<int>>();
    auto intName = DemangleHelper::demangleType<int>();

    EXPECT_TRUE(DemangleHelper::isTemplateType(vectorName));
    EXPECT_FALSE(DemangleHelper::isTemplateType(intName));
}

//==============================================================================
// GetBareTypeName Tests
//==============================================================================

TEST_F(DemangleHelperTest, GetBareTypeNameSimple) {
    auto bareName = DemangleHelper::getBareTypeName("int");
    EXPECT_EQ(std::string(bareName.begin(), bareName.end()), "int");
}

TEST_F(DemangleHelperTest, GetBareTypeNameWithConst) {
    auto bareName = DemangleHelper::getBareTypeName("const int");
    EXPECT_EQ(std::string(bareName.begin(), bareName.end()), "int");
}

TEST_F(DemangleHelperTest, GetBareTypeNameWithNamespace) {
    auto bareName = DemangleHelper::getBareTypeName("std::vector");
    EXPECT_EQ(std::string(bareName.begin(), bareName.end()), "vector");
}

TEST_F(DemangleHelperTest, GetBareTypeNameWithTemplate) {
    auto bareName = DemangleHelper::getBareTypeName("std::vector<int>");
    EXPECT_EQ(std::string(bareName.begin(), bareName.end()), "vector");
}

//==============================================================================
// ExtractNamespace Tests
//==============================================================================

TEST_F(DemangleHelperTest, ExtractNamespaceSimple) {
    auto ns = DemangleHelper::extractNamespace("std::string");
    EXPECT_EQ(std::string(ns.begin(), ns.end()), "std");
}

TEST_F(DemangleHelperTest, ExtractNamespaceNested) {
    auto ns = DemangleHelper::extractNamespace("atom::meta::DemangleHelper");
    EXPECT_EQ(std::string(ns.begin(), ns.end()), "atom::meta");
}

TEST_F(DemangleHelperTest, ExtractNamespaceNoNamespace) {
    auto ns = DemangleHelper::extractNamespace("int");
    EXPECT_TRUE(ns.empty());
}

//==============================================================================
// ExtractTemplateArgs Tests
//==============================================================================

TEST_F(DemangleHelperTest, ExtractTemplateArgsSingle) {
    auto args = DemangleHelper::extractTemplateArgs("vector<int>");

    ASSERT_EQ(args.size(), 1);
    EXPECT_EQ(std::string(args[0].begin(), args[0].end()), "int");
}

TEST_F(DemangleHelperTest, ExtractTemplateArgsMultiple) {
    auto args = DemangleHelper::extractTemplateArgs("map<int, string>");

    ASSERT_EQ(args.size(), 2);
    EXPECT_EQ(std::string(args[0].begin(), args[0].end()), "int");
    EXPECT_EQ(std::string(args[1].begin(), args[1].end()), "string");
}

TEST_F(DemangleHelperTest, ExtractTemplateArgsNested) {
    auto args =
        DemangleHelper::extractTemplateArgs("vector<pair<int, double>>");

    ASSERT_EQ(args.size(), 1);
    expectTypeContains(args[0], "pair");
}

TEST_F(DemangleHelperTest, ExtractTemplateArgsNoTemplate) {
    auto args = DemangleHelper::extractTemplateArgs("int");
    EXPECT_TRUE(args.empty());
}

//==============================================================================
// Type Classification Tests
//==============================================================================

TEST_F(DemangleHelperTest, IsPointerType) {
    EXPECT_TRUE(DemangleHelper::isPointerType("int*"));
    EXPECT_TRUE(DemangleHelper::isPointerType("const int *"));
    EXPECT_FALSE(DemangleHelper::isPointerType("int"));
    EXPECT_FALSE(DemangleHelper::isPointerType("int&"));
}

TEST_F(DemangleHelperTest, IsReferenceType) {
    EXPECT_TRUE(DemangleHelper::isReferenceType("int&"));
    EXPECT_TRUE(DemangleHelper::isReferenceType("int &&"));
    EXPECT_FALSE(DemangleHelper::isReferenceType("int"));
    EXPECT_FALSE(DemangleHelper::isReferenceType("int*"));
}

TEST_F(DemangleHelperTest, IsConstType) {
    EXPECT_TRUE(DemangleHelper::isConstType("const int"));
    EXPECT_TRUE(DemangleHelper::isConstType("int const"));
    EXPECT_FALSE(DemangleHelper::isConstType("int"));
}

//==============================================================================
// GetTypeCategory Tests
//==============================================================================

TEST_F(DemangleHelperTest, GetTypeCategoryVoid) {
    auto category = DemangleHelper::getTypeCategory<void>();
    EXPECT_EQ(std::string(category.begin(), category.end()), "void");
}

TEST_F(DemangleHelperTest, GetTypeCategoryIntegral) {
    auto category = DemangleHelper::getTypeCategory<int>();
    EXPECT_EQ(std::string(category.begin(), category.end()), "integral");
}

TEST_F(DemangleHelperTest, GetTypeCategoryFloatingPoint) {
    auto category = DemangleHelper::getTypeCategory<double>();
    EXPECT_EQ(std::string(category.begin(), category.end()), "floating_point");
}

TEST_F(DemangleHelperTest, GetTypeCategoryArray) {
    auto category = DemangleHelper::getTypeCategory<int[10]>();
    EXPECT_EQ(std::string(category.begin(), category.end()), "array");
}

TEST_F(DemangleHelperTest, GetTypeCategoryEnum) {
    enum class TestEnum { A, B };
    auto category = DemangleHelper::getTypeCategory<TestEnum>();
    EXPECT_EQ(std::string(category.begin(), category.end()), "enum");
}

TEST_F(DemangleHelperTest, GetTypeCategoryClass) {
    auto category = DemangleHelper::getTypeCategory<std::string>();
    EXPECT_EQ(std::string(category.begin(), category.end()), "class");
}

TEST_F(DemangleHelperTest, GetTypeCategoryPointer) {
    auto category = DemangleHelper::getTypeCategory<int*>();
    EXPECT_EQ(std::string(category.begin(), category.end()), "pointer");
}

TEST_F(DemangleHelperTest, GetTypeCategoryLvalueReference) {
    auto category = DemangleHelper::getTypeCategory<int&>();
    EXPECT_EQ(std::string(category.begin(), category.end()),
              "lvalue_reference");
}

TEST_F(DemangleHelperTest, GetTypeCategoryRvalueReference) {
    auto category = DemangleHelper::getTypeCategory<int&&>();
    EXPECT_EQ(std::string(category.begin(), category.end()),
              "rvalue_reference");
}

TEST_F(DemangleHelperTest, GetTypeCategoryMemberPointer) {
    struct TestStruct {
        int member;
    };
    auto category = DemangleHelper::getTypeCategory<int TestStruct::*>();
    EXPECT_EQ(std::string(category.begin(), category.end()),
              "member_object_pointer");
}

TEST_F(DemangleHelperTest, GetTypeCategoryMemberFunction) {
    struct TestStruct {
        void func() {}
    };
    auto category = DemangleHelper::getTypeCategory<void (TestStruct::*)()>();
    EXPECT_EQ(std::string(category.begin(), category.end()),
              "member_function_pointer");
}

//==============================================================================
// TryDemangle Tests
//==============================================================================

TEST_F(DemangleHelperTest, TryDemangleSuccess) {
    auto result = DemangleHelper::tryDemangle(typeid(int).name());

    EXPECT_TRUE(result.hasValue());
    EXPECT_TRUE(static_cast<bool>(result));
    EXPECT_EQ(result.error, AbiErrorCode::Success);
}

TEST_F(DemangleHelperTest, TryDemangleInvalidName) {
    // Invalid mangled name should still return something
    auto result = DemangleHelper::tryDemangle("invalid_mangled_name");

    // Either succeeds with original or fails gracefully
    EXPECT_TRUE(result.hasValue() ||
                result.error == AbiErrorCode::DemangleFailed);
}

//==============================================================================
// Thread Safety Tests
//==============================================================================

TEST_F(DemangleHelperTest, ConcurrentDemangling) {
    constexpr int NUM_THREADS = 8;
    constexpr int ITERATIONS = 100;

    std::vector<std::thread> threads;
    std::atomic<bool> startFlag{false};

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&startFlag, ITERATIONS]() {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }

            for (int j = 0; j < ITERATIONS; ++j) {
                switch (j % 5) {
                    case 0:
                        DemangleHelper::demangleType<int>();
                        break;
                    case 1:
                        DemangleHelper::demangleType<std::string>();
                        break;
                    case 2:
                        DemangleHelper::demangleType<std::vector<int>>();
                        break;
                    case 3:
                        DemangleHelper::demangleType<SimpleTemplate<double>>();
                        break;
                    case 4:
                        DemangleHelper::demangleType<
                            ComplexTemplate<int, std::string>>();
                        break;
                }
            }
        });
    }

    startFlag.store(true);

    for (auto& t : threads) {
        t.join();
    }

    // Should complete without crashes
    EXPECT_LE(DemangleHelper::cacheSize(), 5);
}

TEST_F(DemangleHelperTest, ConcurrentCacheAccess) {
    constexpr int NUM_THREADS = 4;

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&successCount, i]() {
            for (int j = 0; j < 100; ++j) {
                if (i % 2 == 0) {
                    // Reader threads
                    auto size = DemangleHelper::cacheSize();
                    (void)size;
                    successCount++;
                } else {
                    // Writer threads
                    DemangleHelper::demangleType<std::vector<double>>();
                    successCount++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successCount.load(), NUM_THREADS * 100);
}

//==============================================================================
// Cache Management Tests
//==============================================================================

TEST_F(DemangleHelperTest, CacheEviction) {
    // Create many unique types to trigger cache eviction
    // This tests the LRU eviction mechanism

    for (int i = 0; i < 100; ++i) {
        // Create different type name variations
        DemangleHelper::demangle(("SomeType" + std::to_string(i)).c_str());
    }

    // Cache should be limited
    EXPECT_LE(DemangleHelper::cacheSize(), AbiConfig::max_cache_size);
}

//==============================================================================
// Complex Type Tests
//==============================================================================

TEST_F(DemangleHelperTest, ComplexNestedTypes) {
    using ComplexType =
        std::tuple<std::map<std::string, std::vector<int>>,
                   std::shared_ptr<AbstractBase>,
                   std::array<std::unique_ptr<SimpleTemplate<double>>, 5>>;

    auto complexType = DemangleHelper::demangleType<ComplexType>();

    expectTypeContains(complexType, "tuple");
}

TEST_F(DemangleHelperTest, FunctionTypes) {
    using FuncPtr = void (*)(int, double);
    using MemberFuncPtr = int (std::string::*)(size_t) const;

    auto funcPtrType = DemangleHelper::demangleType<FuncPtr>();
    auto memberFuncType = DemangleHelper::demangleType<MemberFuncPtr>();

    // Should demangle without crashing
    EXPECT_FALSE(funcPtrType.empty());
    EXPECT_FALSE(memberFuncType.empty());
}

TEST_F(DemangleHelperTest, SpanTypes) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::span<int> dynamicSpan(vec);
    std::span<int, 5> fixedSpan(vec);

    auto dynamicType = DemangleHelper::demangleType(dynamicSpan);
    auto fixedType = DemangleHelper::demangleType(fixedSpan);

    expectTypeContains(dynamicType, "span");
    expectTypeContains(fixedType, "span");
}

//==============================================================================
// AbiConfig Tests
//==============================================================================

TEST_F(DemangleHelperTest, AbiConfigValues) {
    // Verify config values are sensible
    EXPECT_GT(AbiConfig::buffer_size, 0);
    EXPECT_GT(AbiConfig::max_cache_size, 0);
    EXPECT_TRUE(AbiConfig::thread_safe_cache);
}

//==============================================================================
// AbiErrorCode Tests
//==============================================================================

TEST_F(DemangleHelperTest, AbiErrorCodes) {
    EXPECT_EQ(static_cast<int>(AbiErrorCode::Success), 0);
    EXPECT_NE(static_cast<int>(AbiErrorCode::BufferTooSmall), 0);
    EXPECT_NE(static_cast<int>(AbiErrorCode::DemangleFailed), 0);
}

//==============================================================================
// AbiResult Tests
//==============================================================================

TEST_F(DemangleHelperTest, AbiResultSuccess) {
    AbiResult result;
    result.value = "test";
    result.error = AbiErrorCode::Success;

    EXPECT_TRUE(result.hasValue());
    EXPECT_TRUE(static_cast<bool>(result));
}

TEST_F(DemangleHelperTest, AbiResultError) {
    AbiResult result;
    result.error = AbiErrorCode::DemangleFailed;

    EXPECT_FALSE(result.hasValue());
    EXPECT_FALSE(static_cast<bool>(result));
}

//==============================================================================
// AbiException Tests
//==============================================================================

TEST_F(DemangleHelperTest, AbiExceptionString) {
    try {
        throw AbiException("Test error message");
    } catch (const AbiException& e) {
        std::string what(e.what());
        EXPECT_NE(what.find("Test error"), std::string::npos);
    }
}

TEST_F(DemangleHelperTest, AbiExceptionChar) {
    try {
        throw AbiException("Char message");
    } catch (const AbiException& e) {
        std::string what(e.what());
        EXPECT_NE(what.find("Char message"), std::string::npos);
    }
}

//==============================================================================
// Platform-Specific Tests
//==============================================================================

TEST_F(DemangleHelperTest, PlatformSpecificTypes) {
#ifdef _WIN32
    using Handle = void*;
    auto handleType = DemangleHelper::demangleType<Handle>();
    expectTypeContains(handleType, "void");
#else
    using FileDescriptor = int;
    auto fdType = DemangleHelper::demangleType<FileDescriptor>();
    expectTypeContains(fdType, "int");
#endif
}

//==============================================================================
// C++23 Expected Tests (if available)
//==============================================================================

#if ATOM_ABI_HAS_EXPECTED
TEST_F(DemangleHelperTest, DemangleExpectedSuccess) {
    auto result = DemangleHelper::demangleExpected(typeid(int).name());

    EXPECT_TRUE(result.has_value());
    expectTypeContains(*result, "int");
}

TEST_F(DemangleHelperTest, DemangleExpectedInvalid) {
    auto result =
        DemangleHelper::demangleExpected("definitely_not_a_valid_name_xyz123");

    // May succeed with original or return error
    // Either is acceptable behavior
    SUCCEED();
}
#endif

}  // namespace atom::meta::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
