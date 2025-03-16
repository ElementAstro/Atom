// filepath: /home/max/Atom-1/atom/meta/test_overload.hpp
#ifndef ATOM_META_TEST_OVERLOAD_HPP
#define ATOM_META_TEST_OVERLOAD_HPP

#include <gtest/gtest.h>
#include "atom/meta/overload.hpp"

#include <functional>
#include <type_traits>

namespace atom::meta::test {

// Test fixture for OverloadCast and related utilities
class OverloadTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    // Test class with various overloaded methods
    class TestClass {
    public:
        // Regular methods with different overloads
        int multiply(int a, int b) { return a * b; }
        int multiply(int a, int b, int c) { return a * b * c; }

        // Const methods
        int getValue() const { return value_; }
        void setValue(int val) { value_ = val; }

        // Volatile methods
        int getValueVolatile() volatile { return value_; }
        void setValueVolatile(int val) volatile { value_ = val; }

        // Const volatile methods
        int getValueConstVolatile() const volatile { return value_; }
        void setValueConstVolatile(int val) const volatile {
            const_cast<TestClass*>(this)->value_ = val;
        }

        // Noexcept methods
        int add(int a, int b) noexcept { return a + b; }
        int subtract(int a, int b) noexcept { return a - b; }

        // Const noexcept methods
        double divide(double a, double b) const noexcept {
            return b != 0.0 ? a / b : 0.0;
        }

        // Volatile noexcept methods
        bool isGreater(int a, int b) volatile noexcept { return a > b; }

        // Const volatile noexcept methods
        bool isEqual(int a, int b) const volatile noexcept { return a == b; }

    private:
        int value_ = 0;
    };

    // Free functions for testing
    static int freeAdd(int a, int b) { return a + b; }
    static int freeMultiply(int a, int b) { return a * b; }
    static int freeMultiply(int a, int b, int c) { return a * b * c; }
    static double freeDivide(double a, double b) noexcept {
        return b != 0.0 ? a / b : 0.0;
    }
};

// Test overload_cast with regular member functions
TEST_F(OverloadTest, RegularMemberFunctions) {
    TestClass obj;

    // Get pointer to multiply(int, int)
    auto multiplyPtr = overload_cast<int, int>(&TestClass::multiply);
    EXPECT_EQ((obj.*multiplyPtr)(3, 4), 12);

    // Get pointer to multiply(int, int, int)
    auto multiplyThreePtr = overload_cast<int, int, int>(&TestClass::multiply);
    EXPECT_EQ((obj.*multiplyThreePtr)(2, 3, 4), 24);

    // Check that we get the correct function pointers
    EXPECT_NE(multiplyPtr, multiplyThreePtr);
}

// Test overload_cast with const member functions
TEST_F(OverloadTest, ConstMemberFunctions) {
    TestClass obj;
    obj.setValue(42);
    const TestClass& constObj = obj;

    // Get pointer to const getter
    auto getValuePtr = overload_cast<>(&TestClass::getValue);
    EXPECT_EQ((constObj.*getValuePtr)(), 42);

    // Check that the function pointer type is correct
    using GetterType = int (TestClass::*)() const;
    EXPECT_TRUE((std::is_same_v<decltype(getValuePtr), GetterType>));
}

// Test overload_cast with volatile member functions
TEST_F(OverloadTest, VolatileMemberFunctions) {
    TestClass obj;
    obj.setValue(42);
    volatile TestClass volatileObj = obj;

    // Get pointer to volatile getter
    auto getVolatilePtr = overload_cast<>(&TestClass::getValueVolatile);
    EXPECT_EQ((volatileObj.*getVolatilePtr)(), 42);

    // Set a new value using volatile setter
    auto setVolatilePtr = overload_cast<int>(&TestClass::setValueVolatile);
    (volatileObj.*setVolatilePtr)(99);
    EXPECT_EQ((volatileObj.*getVolatilePtr)(), 99);

    // Check that the function pointer types are correct
    using GetterType = int (TestClass::*)() volatile;
    using SetterType = void (TestClass::*)(int) volatile;
    EXPECT_TRUE((std::is_same_v<decltype(getVolatilePtr), GetterType>));
    EXPECT_TRUE((std::is_same_v<decltype(setVolatilePtr), SetterType>));
}

// Test overload_cast with const volatile member functions
TEST_F(OverloadTest, ConstVolatileMemberFunctions) {
    TestClass obj;
    obj.setValue(42);
    const volatile TestClass cvObj = obj;

    // Get pointer to const volatile getter
    auto getCVPtr = overload_cast<>(&TestClass::getValueConstVolatile);
    EXPECT_EQ((cvObj.*getCVPtr)(), 42);

    // Set a new value using const volatile setter
    auto setCVPtr = overload_cast<int>(&TestClass::setValueConstVolatile);
    (cvObj.*setCVPtr)(77);
    EXPECT_EQ((cvObj.*getCVPtr)(), 77);

    // Check that the function pointer types are correct
    using GetterType = int (TestClass::*)() const volatile;
    using SetterType = void (TestClass::*)(int) const volatile;
    EXPECT_TRUE((std::is_same_v<decltype(getCVPtr), GetterType>));
    EXPECT_TRUE((std::is_same_v<decltype(setCVPtr), SetterType>));
}

// Test overload_cast with noexcept member functions
TEST_F(OverloadTest, NoexceptMemberFunctions) {
    TestClass obj;

    // Get pointer to noexcept add method
    auto addPtr = overload_cast<int, int>(&TestClass::add);
    EXPECT_EQ((obj.*addPtr)(5, 7), 12);

    // Get pointer to noexcept subtract method
    auto subtractPtr = overload_cast<int, int>(&TestClass::subtract);
    EXPECT_EQ((obj.*subtractPtr)(10, 4), 6);

    // Check that the function pointer types are correct
    using AddType = int (TestClass::*)(int, int) noexcept;
    EXPECT_TRUE((std::is_same_v<decltype(addPtr), AddType>));

    // Verify noexcept specification is preserved
    EXPECT_TRUE(noexcept((obj.*addPtr)(1, 2)));
    EXPECT_TRUE(noexcept((obj.*subtractPtr)(1, 2)));
}

// Test overload_cast with const noexcept member functions
TEST_F(OverloadTest, ConstNoexceptMemberFunctions) {
    TestClass obj;
    const TestClass& constObj = obj;

    // Get pointer to const noexcept divide method
    auto dividePtr = overload_cast<double, double>(&TestClass::divide);
    EXPECT_DOUBLE_EQ((constObj.*dividePtr)(10.0, 2.0), 5.0);
    EXPECT_DOUBLE_EQ((constObj.*dividePtr)(5.0, 0.0),
                     0.0);  // Safe division by zero

    // Check that the function pointer type is correct
    using DivideType = double (TestClass::*)(double, double) const noexcept;
    EXPECT_TRUE((std::is_same_v<decltype(dividePtr), DivideType>));

    // Verify noexcept specification is preserved
    EXPECT_TRUE(noexcept((constObj.*dividePtr)(1.0, 2.0)));
}

// Test overload_cast with volatile noexcept member functions
TEST_F(OverloadTest, VolatileNoexceptMemberFunctions) {
    TestClass obj;
    volatile TestClass volatileObj = obj;

    // Get pointer to volatile noexcept isGreater method
    auto isGreaterPtr = overload_cast<int, int>(&TestClass::isGreater);
    EXPECT_TRUE((volatileObj.*isGreaterPtr)(10, 5));
    EXPECT_FALSE((volatileObj.*isGreaterPtr)(5, 10));

    // Check that the function pointer type is correct
    using IsGreaterType = bool (TestClass::*)(int, int) volatile noexcept;
    EXPECT_TRUE((std::is_same_v<decltype(isGreaterPtr), IsGreaterType>));

    // Verify noexcept specification is preserved
    EXPECT_TRUE(noexcept((volatileObj.*isGreaterPtr)(1, 2)));
}

// Test overload_cast with const volatile noexcept member functions
TEST_F(OverloadTest, ConstVolatileNoexceptMemberFunctions) {
    TestClass obj;
    const volatile TestClass cvObj = obj;

    // Get pointer to const volatile noexcept isEqual method
    auto isEqualPtr = overload_cast<int, int>(&TestClass::isEqual);
    EXPECT_TRUE((cvObj.*isEqualPtr)(5, 5));
    EXPECT_FALSE((cvObj.*isEqualPtr)(5, 10));

    // Check that the function pointer type is correct
    using IsEqualType = bool (TestClass::*)(int, int) const volatile noexcept;
    EXPECT_TRUE((std::is_same_v<decltype(isEqualPtr), IsEqualType>));

    // Verify noexcept specification is preserved
    EXPECT_TRUE(noexcept((cvObj.*isEqualPtr)(1, 2)));
}

// Test overload_cast with free functions
TEST_F(OverloadTest, FreeFunctions) {
    // Get pointer to freeAdd
    auto freeAddPtr = overload_cast<int, int>(&freeAdd);
    EXPECT_EQ((*freeAddPtr)(3, 4), 7);

    // Get pointer to freeMultiply(int, int)
    auto freeMultiplyPtr = overload_cast<int, int>(&freeMultiply);
    EXPECT_EQ((*freeMultiplyPtr)(3, 4), 12);

    // Get pointer to freeMultiply(int, int, int)
    auto freeMultiplyThreePtr = overload_cast<int, int, int>(&freeMultiply);
    EXPECT_EQ((*freeMultiplyThreePtr)(2, 3, 4), 24);

    // Check that the function pointer types are correct
    using AddFuncType = int (*)(int, int);
    using MultFuncType = int (*)(int, int);
    EXPECT_TRUE((std::is_same_v<decltype(freeAddPtr), AddFuncType>));
    EXPECT_TRUE((std::is_same_v<decltype(freeMultiplyPtr), MultFuncType>));
}

// Test overload_cast with noexcept free functions
TEST_F(OverloadTest, NoexceptFreeFunctions) {
    // Get pointer to freeDivide
    auto freeDividePtr = overload_cast<double, double>(&freeDivide);
    EXPECT_DOUBLE_EQ((*freeDividePtr)(10.0, 2.0), 5.0);

    // Check that the function pointer type is correct
    using DivideFuncType = double (*)(double, double) noexcept;
    EXPECT_TRUE((std::is_same_v<decltype(freeDividePtr), DivideFuncType>));

    // Verify noexcept specification is preserved
    EXPECT_TRUE(noexcept((*freeDividePtr)(10.0, 2.0)));
}

// Test compile-time usage with static_assert
TEST_F(OverloadTest, CompileTimeUsage) {
    // Verify that overload_cast produces constexpr results
    constexpr auto compileTimePtr =
        overload_cast<int, int>(&OverloadTest::freeAdd);
    static_assert(compileTimePtr != nullptr,
                  "Function pointer should not be null");
}

// Test decayCopy function
TEST_F(OverloadTest, DecayCopy) {
    // Test with various types

    // Basic types
    int i = 42;
    auto i_copy = decayCopy(i);
    EXPECT_EQ(i_copy, 42);
    static_assert(std::is_same_v<decltype(i_copy), int>);

    // References
    int& ref = i;
    auto ref_copy = decayCopy(ref);
    EXPECT_EQ(ref_copy, 42);
    static_assert(std::is_same_v<decltype(ref_copy), int>);

    // Const
    const int ci = 100;
    auto ci_copy = decayCopy(ci);
    EXPECT_EQ(ci_copy, 100);
    static_assert(std::is_same_v<decltype(ci_copy), int>);

    // Arrays decay to pointers
    int arr[3] = {1, 2, 3};
    auto arr_copy = decayCopy(arr);
    EXPECT_EQ(arr_copy[0], 1);
    EXPECT_EQ(arr_copy[1], 2);
    static_assert(std::is_same_v<decltype(arr_copy), int*>);

    // String literals decay to const char*
    auto str_copy = decayCopy("hello");
    EXPECT_STREQ(str_copy, "hello");
    static_assert(std::is_same_v<decltype(str_copy), const char*>);

    // Function pointers remain function pointers
    auto func_copy = decayCopy(&OverloadTest::freeAdd);
    EXPECT_EQ(func_copy(5, 3), 8);
    static_assert(std::is_same_v<decltype(func_copy), int (*)(int, int)>);

    // Test with move-only type
    std::unique_ptr<int> ptr = std::make_unique<int>(42);
    auto ptr_copy = decayCopy(std::move(ptr));
    EXPECT_EQ(*ptr_copy, 42);
    EXPECT_EQ(ptr, nullptr);  // Original should be moved-from
    static_assert(std::is_same_v<decltype(ptr_copy), std::unique_ptr<int>>);

    // Test noexcept specification is preserved properly
    struct NoexceptCopyable {
        NoexceptCopyable() = default;
        NoexceptCopyable(const NoexceptCopyable&) noexcept = default;
    };

    struct ThrowingCopyable {
        ThrowingCopyable() = default;
        ThrowingCopyable(const ThrowingCopyable&) noexcept(false) {}
    };

    NoexceptCopyable ne;
    static_assert(noexcept(decayCopy(ne)));

    ThrowingCopyable tc;
    static_assert(!noexcept(decayCopy(tc)));
}

// Test real-world usage scenarios
TEST_F(OverloadTest, RealWorldUsage) {
    TestClass obj;

    // Scenario 1: Resolving ambiguous overloads when passing to STL algorithms
    auto multiplyBy2 = [&obj](int value) {
        return (obj.*overload_cast<int, int>(&TestClass::multiply))(value, 2);
    };

    EXPECT_EQ(multiplyBy2(5), 10);

    // Scenario 2: Creating function objects from member functions
    std::function<int(int, int)> addFunc =
        std::bind(overload_cast<int, int>(&TestClass::add), &obj,
                  std::placeholders::_1, std::placeholders::_2);

    EXPECT_EQ(addFunc(10, 20), 30);

    // Scenario 3: Using with std::invoke
    auto subtractPtr = overload_cast<int, int>(&TestClass::subtract);
    EXPECT_EQ(std::invoke(subtractPtr, obj, 20, 5), 15);

    // Scenario 4: Using with auto for clean syntax
    auto isEqual = overload_cast<int, int>(&TestClass::isEqual);
    const volatile TestClass cvObj = obj;
    EXPECT_TRUE((cvObj.*isEqual)(10, 10));
}

// Test edge cases and error handling
TEST_F(OverloadTest, EdgeCases) {
    // Test that overload_cast works with empty argument lists
    TestClass obj;
    obj.setValue(42);

    auto getValuePtr = overload_cast<>(&TestClass::getValue);
    EXPECT_EQ((obj.*getValuePtr)(), 42);

    // Test with function that has unusual argument types
    struct ComplexArg {
        int value;
    };

    class ComplexClass {
    public:
        int processComplex(const ComplexArg& arg, int multiplier = 1) const {
            return arg.value * multiplier;
        }
    };

    ComplexClass complexObj;
    auto processPtr =
        overload_cast<const ComplexArg&, int>(&ComplexClass::processComplex);

    ComplexArg arg{10};
    EXPECT_EQ((complexObj.*processPtr)(arg, 2), 20);

    // Default arguments aren't preserved in function pointers, so we need to
    // pass it explicitly
    EXPECT_EQ((complexObj.*processPtr)(arg, 1),
              10);  // Explicitly pass the default argument value
}

}  // namespace atom::meta::test

#endif  // ATOM_META_TEST_OVERLOAD_HPP