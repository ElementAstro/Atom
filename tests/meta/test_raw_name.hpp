// filepath: /home/max/Atom-1/atom/function/test_raw_name.hpp
#ifndef ATOM_META_TEST_RAW_NAME_HPP
#define ATOM_META_TEST_RAW_NAME_HPP

#include <gtest/gtest.h>
#include "atom/function/raw_name.hpp"

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace atom::meta::test {

// Basic test fixture for raw_name tests
class RawNameTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    // Test types
    struct TestStruct {
        int x;
        double y;
    };

    class TestClass {
    public:
        void method() {}
        int value = 0;
    };

    enum class TestEnum { First, Second, Third };

    template <typename T>
    struct TemplateStruct {
        T value;
    };

    template <typename T, typename U>
    struct ComplexTemplate {
        T first;
        U second;
    };
};

// Test raw_name_of with basic types
TEST_F(RawNameTest, BasicTypes) {
    // Test standard types
    auto intName = raw_name_of<int>();
    EXPECT_TRUE(intName.find("int") != std::string_view::npos);

    auto doubleName = raw_name_of<double>();
    EXPECT_TRUE(doubleName.find("double") != std::string_view::npos);

    auto boolName = raw_name_of<bool>();
    EXPECT_TRUE(boolName.find("bool") != std::string_view::npos);

    // Test standard template library types
    auto vecName = raw_name_of<std::vector<int>>();
    EXPECT_TRUE(vecName.find("vector") != std::string_view::npos);
    EXPECT_TRUE(vecName.find("int") != std::string_view::npos);

    auto stringName = raw_name_of<std::string>();
    EXPECT_TRUE(stringName.find("string") != std::string_view::npos ||
                stringName.find("basic_string") != std::string_view::npos);
}

// Test raw_name_of with custom types
TEST_F(RawNameTest, CustomTypes) {
    // Test struct
    auto structName = raw_name_of<TestStruct>();
    EXPECT_TRUE(structName.find("TestStruct") != std::string_view::npos);

    // Test class
    auto className = raw_name_of<TestClass>();
    EXPECT_TRUE(className.find("TestClass") != std::string_view::npos);

    // Test enum class
    auto enumName = raw_name_of<TestEnum>();
    EXPECT_TRUE(enumName.find("TestEnum") != std::string_view::npos);
}

// Test raw_name_of with template types
TEST_F(RawNameTest, TemplateTypes) {
    // Test simple template
    auto simpleTplName = raw_name_of<TemplateStruct<int>>();
    EXPECT_TRUE(simpleTplName.find("TemplateStruct") != std::string_view::npos);
    EXPECT_TRUE(simpleTplName.find("int") != std::string_view::npos);

    // Test complex template
    auto complexTplName = raw_name_of<ComplexTemplate<int, double>>();
    EXPECT_TRUE(complexTplName.find("ComplexTemplate") !=
                std::string_view::npos);
    EXPECT_TRUE(complexTplName.find("int") != std::string_view::npos);
    EXPECT_TRUE(complexTplName.find("double") != std::string_view::npos);

    // Test nested templates
    auto nestedTplName = raw_name_of<TemplateStruct<std::vector<int>>>();
    EXPECT_TRUE(nestedTplName.find("TemplateStruct") != std::string_view::npos);
    EXPECT_TRUE(nestedTplName.find("vector") != std::string_view::npos);
}

// Test raw_name_of with const, volatile, and reference types
TEST_F(RawNameTest, TypeQualifiers) {
    // Test const qualifier
    auto constName = raw_name_of<const int>();
    EXPECT_TRUE(constName.find("int") != std::string_view::npos);

    // Test volatile qualifier
    auto volatileName = raw_name_of<volatile double>();
    EXPECT_TRUE(volatileName.find("double") != std::string_view::npos);

    // Test reference
    auto refName = raw_name_of<int&>();
    EXPECT_TRUE(refName.find("int") != std::string_view::npos);

    // Test rvalue reference
    auto rvalueName = raw_name_of<int&&>();
    EXPECT_TRUE(rvalueName.find("int") != std::string_view::npos);

    // Test pointer
    auto pointerName = raw_name_of<int*>();
    EXPECT_TRUE(pointerName.find("int") != std::string_view::npos);
}

// Test raw_name_of with auto template argument
TEST_F(RawNameTest, AutoValues) {
    // Test with integral constants
    constexpr int kIntValue = 42;
    auto intValueName = raw_name_of<kIntValue>();
    EXPECT_FALSE(intValueName.empty());

    constexpr double kDoubleValue = 3.14;
    auto doubleValueName = raw_name_of<kDoubleValue>();
    EXPECT_FALSE(doubleValueName.empty());

    // Test with enum values
    constexpr TestEnum kEnumValue = TestEnum::Second;
    auto enumValueName = raw_name_of<kEnumValue>();
    EXPECT_FALSE(enumValueName.empty());
}

// Test raw_name_of_enum
TEST_F(RawNameTest, EnumNames) {
    // Test enum class values
    constexpr TestEnum kFirst = TestEnum::First;
    auto firstName = raw_name_of_enum<kFirst>();
    EXPECT_TRUE(firstName.find("First") != std::string_view::npos);

    constexpr TestEnum kSecond = TestEnum::Second;
    auto secondName = raw_name_of_enum<kSecond>();
    EXPECT_TRUE(secondName.find("Second") != std::string_view::npos);
}

// Test raw_name_of_template
TEST_F(RawNameTest, TemplateTraits) {
    // Note: This test depends on the implementation of template_traits
    // which might need special handling depending on the compiler

    // Basic test to ensure it doesn't crash
    auto vecTraits = raw_name_of_template<std::vector<int>>();
    EXPECT_FALSE(vecTraits.empty());

    auto mapTraits = raw_name_of_template<std::map<int, std::string>>();
    EXPECT_FALSE(mapTraits.empty());
}

#ifdef ATOM_CPP_20_SUPPORT
// Test raw_name_of_member (C++20 only)
TEST_F(RawNameTest, MemberNames) {
    // TODO: Implement raw_name_of_member function before enabling this test
    /*
    // Create a wrapper with a member
    Wrapper wrapper{42};

    // Test member access
    auto memberName = raw_name_of_member<wrapper>();
    EXPECT_FALSE(memberName.empty());

    // Test with different types
    Wrapper<std::string> strWrapper{"test"};
    auto strMemberName = raw_name_of_member<strWrapper>();
    EXPECT_FALSE(strMemberName.empty());
    */
}
#endif  // ATOM_CPP_20_SUPPORT

// Regression tests for specific cases
TEST_F(RawNameTest, RegressionTests) {
    // Test with smart pointers
    auto uniquePtrName = raw_name_of<std::unique_ptr<int>>();
    EXPECT_TRUE(uniquePtrName.find("unique_ptr") != std::string_view::npos);

    auto sharedPtrName = raw_name_of<std::shared_ptr<TestClass>>();
    EXPECT_TRUE(sharedPtrName.find("shared_ptr") != std::string_view::npos);
    EXPECT_TRUE(sharedPtrName.find("TestClass") != std::string_view::npos);

    // Test with function pointers
    using FuncPtr = int (*)(int, int);
    auto funcPtrName = raw_name_of<FuncPtr>();
    EXPECT_FALSE(funcPtrName.empty());

    // Test with lambda (result might vary by compiler)
    auto lambda = [](int x) { return x * 2; };
    auto lambdaName = raw_name_of<decltype(lambda)>();
    EXPECT_FALSE(lambdaName.empty());
}

// Test cross-platform consistency
TEST_F(RawNameTest, CrossPlatformConsistency) {
    // This test ensures that the basic functionality works across platforms
    // even if the exact string format differs

    // Core types should be identifiable on all platforms
    auto intName = raw_name_of<int>();
    EXPECT_FALSE(intName.empty());

    auto vecIntName = raw_name_of<std::vector<int>>();
    EXPECT_FALSE(vecIntName.empty());

    // Our custom types should be identifiable too
    auto structName = raw_name_of<TestStruct>();
    EXPECT_FALSE(structName.empty());

    // Print some values for manual inspection if needed
    std::cout << "int name: " << intName << std::endl;
    std::cout << "vector<int> name: " << vecIntName << std::endl;
    std::cout << "TestStruct name: " << structName << std::endl;
}

// Test error cases and edge cases
TEST_F(RawNameTest, EdgeCases) {
    // Test with void
    auto voidName = raw_name_of<void>();
    EXPECT_FALSE(voidName.empty());

    // Test with function types
    using FuncType = int(int, int);
    auto funcTypeName = raw_name_of<FuncType>();
    EXPECT_FALSE(funcTypeName.empty());

    // Test with arrays
    using ArrayType = int[10];
    auto arrayName = raw_name_of<ArrayType>();
    EXPECT_FALSE(arrayName.empty());

    // Test with multi-dimensional arrays
    using Matrix = int[3][3];
    auto matrixName = raw_name_of<Matrix>();
    EXPECT_FALSE(matrixName.empty());
}

// Compile-time tests using static_assert
// These tests verify that raw_name_of can be used in constexpr contexts
TEST_F(RawNameTest, CompileTimeUsage) {
    // Test if the results are available at compile time
    constexpr auto intName = raw_name_of<int>();
    static_assert(!intName.empty(), "raw_name_of<int>() should not be empty");

    constexpr auto boolName = raw_name_of<bool>();
    static_assert(!boolName.empty(), "raw_name_of<bool>() should not be empty");

    constexpr int kValue = 42;
    constexpr auto valueName = raw_name_of<kValue>();
    static_assert(!valueName.empty(),
                  "raw_name_of<kValue>() should not be empty");
}

}  // namespace atom::meta::test

// Main function to run the tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#endif  // ATOM_META_TEST_RAW_NAME_HPP