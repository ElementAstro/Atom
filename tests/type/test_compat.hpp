#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <type_traits>

#include "atom/type/compat.hpp"

using namespace atom::type;
using atom::type::compat::expected;
using atom::type::compat::unexpected;

// Test fixture for compatibility layer tests
class CompatTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Basic Expected Type Tests
TEST_F(CompatTest, ExpectedTypeExists) {
    // Test that the expected type alias exists and is usable
    expected<int> result = 42;
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST_F(CompatTest, ExpectedWithCustomErrorType) {
    // Test expected with custom error type
    expected<int, std::string> result = 42;
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST_F(CompatTest, ExpectedDefaultErrorType) {
    // Test that default error type is std::string
    expected<int> result = 42;

    // This should compile, confirming std::string is the default error type
    static_assert(std::is_same_v<decltype(result), expected<int, std::string>>);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

// Unexpected Type Tests
TEST_F(CompatTest, UnexpectedTypeExists) {
    // Test that the unexpected type alias exists and is usable
    auto error = unexpected<std::string>("error message");

    expected<int, std::string> result = error;
    EXPECT_FALSE(result.has_value());
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    EXPECT_EQ(result.error(), "error message");
#else
    EXPECT_EQ(result.error().error(), "error message");
#endif
}

TEST_F(CompatTest, UnexpectedWithDifferentTypes) {
    // Test unexpected with different error types
    auto int_error = unexpected<int>(404);
    auto string_error = unexpected<std::string>("not found");

    expected<std::string, int> result1 = int_error;
    expected<int, std::string> result2 = string_error;

    EXPECT_FALSE(result1.has_value());
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    EXPECT_EQ(result1.error(), 404);
#else
    EXPECT_EQ(result1.error().error(), 404);
#endif

    EXPECT_FALSE(result2.has_value());
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    EXPECT_EQ(result2.error(), "not found");
#else
    EXPECT_EQ(result2.error().error(), "not found");
#endif
}

// Success Cases Tests
TEST_F(CompatTest, SuccessfulExpected) {
    expected<std::string> result = std::string("success");

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result);  // Should be convertible to bool
    EXPECT_EQ(result.value(), "success");
    EXPECT_EQ(*result, "success");  // Should support dereference
}

TEST_F(CompatTest, SuccessfulExpectedWithComplexType) {
    struct ComplexType {
        int id;
        std::string name;

        bool operator==(const ComplexType& other) const {
            return id == other.id && name == other.name;
        }
    };

    ComplexType obj{42, "test"};
    expected<ComplexType> result = obj;

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().id, 42);
    EXPECT_EQ(result.value().name, "test");
}

// Error Cases Tests
TEST_F(CompatTest, ErrorExpected) {
    expected<int> result = unexpected<std::string>("error occurred");

    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(result);  // Should be convertible to bool

    // Handle both std::expected and custom implementation
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    EXPECT_EQ(result.error(), "error occurred");
#else
    EXPECT_EQ(result.error().error(), "error occurred");
#endif
}

TEST_F(CompatTest, ErrorExpectedWithCustomErrorType) {
    enum class ErrorCode {
        NOT_FOUND = 404,
        INTERNAL_ERROR = 500
    };

    expected<std::string, ErrorCode> result = unexpected<ErrorCode>(ErrorCode::NOT_FOUND);

    EXPECT_FALSE(result.has_value());
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    EXPECT_EQ(result.error(), ErrorCode::NOT_FOUND);
#else
    EXPECT_EQ(result.error().error(), ErrorCode::NOT_FOUND);
#endif
}

// Value Access Tests
TEST_F(CompatTest, ValueAccess) {
    expected<int> success_result = 42;
    expected<int> error_result = unexpected<std::string>("error");

    // Test value() method
    EXPECT_EQ(success_result.value(), 42);
    EXPECT_THROW([[maybe_unused]] auto _ = error_result.value(), std::exception);  // Should throw on error

    // Test dereference operator
    EXPECT_EQ(*success_result, 42);

    // Test arrow operator (if available)
    struct TestStruct {
        int getValue() const { return 100; }
    };

    expected<TestStruct> struct_result = TestStruct{};
    EXPECT_EQ(struct_result->getValue(), 100);
}

TEST_F(CompatTest, ValueOrMethod) {
    expected<int> success_result = 42;
    expected<int> error_result = unexpected<std::string>("error");

    // Test value_or method
    EXPECT_EQ(success_result.value_or(0), 42);
    EXPECT_EQ(error_result.value_or(100), 100);
}

// Error Access Tests
TEST_F(CompatTest, ErrorAccess) {
    expected<int> error_result = unexpected<std::string>("test error");

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    EXPECT_EQ(error_result.error(), "test error");
#else
    EXPECT_EQ(error_result.error().error(), "test error");
#endif
}

// Monadic Operations Tests (if available in the implementation)
TEST_F(CompatTest, MonadicOperations) {
    expected<int> success_result = 42;
    expected<int> error_result = unexpected<std::string>("error");

    // Test and_then (if available)
    auto doubled = success_result.and_then([](int value) -> expected<int> {
        return value * 2;
    });

    EXPECT_TRUE(doubled.has_value());
    EXPECT_EQ(doubled.value(), 84);

    // Test and_then with error
    auto error_doubled = error_result.and_then([](int value) -> expected<int> {
        return value * 2;
    });

    EXPECT_FALSE(error_doubled.has_value());
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    EXPECT_EQ(error_doubled.error(), "error");
#else
    EXPECT_EQ(error_doubled.error().error(), "error");
#endif
}

// Note: transform method may not be available in all implementations
// This test is commented out as it's not universally supported
/*
TEST_F(CompatTest, TransformOperation) {
    expected<int> success_result = 42;
    expected<int> error_result = unexpected<std::string>("error");

    // Test transform (if available)
    auto doubled = success_result.transform([](int value) {
        return value * 2;
    });

    EXPECT_TRUE(doubled.has_value());
    EXPECT_EQ(doubled.value(), 84);

    // Test transform with error
    auto error_doubled = error_result.transform([](int value) {
        return value * 2;
    });

    EXPECT_FALSE(error_doubled.has_value());
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    EXPECT_EQ(error_doubled.error(), "error");
#else
    EXPECT_EQ(error_doubled.error().error(), "error");
#endif
}
*/

// Type Traits Tests
TEST_F(CompatTest, TypeTraits) {
    // Test that expected and unexpected are the correct types
    static_assert(std::is_same_v<expected<int>, expected<int, std::string>>);

    // Test that we can detect if we're using std::expected or custom implementation
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    // Using std::expected
    static_assert(std::is_same_v<expected<int>, std::expected<int, std::string>>);
    static_assert(std::is_same_v<unexpected<std::string>, std::unexpected<std::string>>);
#else
    // Using custom implementation
    static_assert(std::is_same_v<expected<int>, ::atom::type::expected<int, std::string>>);
    static_assert(std::is_same_v<unexpected<std::string>, ::atom::type::unexpected<std::string>>);
#endif
}

// Compatibility Tests
TEST_F(CompatTest, CrossPlatformCompatibility) {
    // Test that the same code works regardless of which implementation is used
    auto create_result = [](bool success) -> expected<int> {
        if (success) {
            return 42;
        } else {
            return unexpected<std::string>("operation failed");
        }
    };

    auto success_result = create_result(true);
    auto error_result = create_result(false);

    EXPECT_TRUE(success_result.has_value());
    EXPECT_EQ(success_result.value(), 42);

    EXPECT_FALSE(error_result.has_value());
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    EXPECT_EQ(error_result.error(), "operation failed");
#else
    EXPECT_EQ(error_result.error().error(), "operation failed");
#endif
}

// Edge Cases Tests
TEST_F(CompatTest, VoidExpected) {
    // Test expected<void> if supported
    expected<void> success_result;
    expected<void> error_result = unexpected<std::string>("void error");

    EXPECT_TRUE(success_result.has_value());
    EXPECT_FALSE(error_result.has_value());
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    EXPECT_EQ(error_result.error(), "void error");
#else
    EXPECT_EQ(error_result.error().error(), "void error");
#endif
}

TEST_F(CompatTest, MoveSemantics) {
    // Test move semantics
    std::string large_string(1000, 'x');
    expected<std::string> result = std::move(large_string);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1000);

    // Test moving the result
    auto moved_result = std::move(result);
    EXPECT_TRUE(moved_result.has_value());
    EXPECT_EQ(moved_result.value().size(), 1000);
}

TEST_F(CompatTest, ConstCorrectness) {
    const expected<int> const_result = 42;

    EXPECT_TRUE(const_result.has_value());
    EXPECT_EQ(const_result.value(), 42);
    EXPECT_EQ(*const_result, 42);
}

// Performance Tests
TEST_F(CompatTest, NoThrowOperations) {
    // Test that basic operations are noexcept where expected
    expected<int> result = 42;

    EXPECT_TRUE(noexcept(result.has_value()));
    EXPECT_TRUE(noexcept(static_cast<bool>(result)));
}
