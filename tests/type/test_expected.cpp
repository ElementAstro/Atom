#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "atom/type/expected.hpp"

namespace {

using namespace atom::type;

// Custom error type for testing
struct CustomError {
    int code;
    std::string message;

    bool operator==(const CustomError& other) const {
        return code == other.code && message == other.message;
    }
};

// Test fixture for expected tests
class ExpectedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test the Error class
TEST_F(ExpectedTest, ErrorClass) {
    // Constructor with value
    Error<int> error1(42);
    EXPECT_EQ(error1.error(), 42);

    // Constructor with string literal (specialized for std::string)
    Error<std::string> error2("error message");
    EXPECT_EQ(error2.error(), "error message");

    // Copy constructor
    Error<int> error3 = error1;
    EXPECT_EQ(error3.error(), 42);

    // Equality operator
    EXPECT_TRUE(error1 == error3);
    EXPECT_FALSE(error1 == Error<int>(43));

    // Custom error type
    CustomError custom_error{404, "Not Found"};
    Error<CustomError> error4(custom_error);
    EXPECT_EQ(error4.error().code, 404);
    EXPECT_EQ(error4.error().message, "Not Found");
}

// Test the unexpected class
TEST_F(ExpectedTest, UnexpectedClass) {
    // Constructor with value
    unexpected<int> unex1(42);
    EXPECT_EQ(unex1.error(), 42);

    // Move constructor
    unexpected<std::string> unex2(std::string("error message"));
    EXPECT_EQ(unex2.error(), "error message");

    // Equality operator
    EXPECT_TRUE(unex1 == unexpected<int>(42));
    EXPECT_FALSE(unex1 == unexpected<int>(43));

    // Custom error type
    CustomError custom_error{404, "Not Found"};
    unexpected<CustomError> unex3(custom_error);
    EXPECT_EQ(unex3.error().code, 404);
    EXPECT_EQ(unex3.error().message, "Not Found");
}

// Test expected constructors with value
TEST_F(ExpectedTest, ExpectedConstructorsWithValue) {
    // Default constructor
    expected<int> exp1;
    EXPECT_TRUE(exp1.has_value());
    EXPECT_EQ(exp1.value(), 0);

    // Value constructor
    expected<int> exp2(42);
    EXPECT_TRUE(exp2.has_value());
    EXPECT_EQ(exp2.value(), 42);

    // Move constructor
    std::string str = "hello";
    expected<std::string> exp3(std::move(str));
    EXPECT_TRUE(exp3.has_value());
    EXPECT_EQ(exp3.value(), "hello");
    // str is in a valid but unspecified state after move

    // Copy constructor
    expected<int> exp4 = exp2;
    EXPECT_TRUE(exp4.has_value());
    EXPECT_EQ(exp4.value(), 42);

    // Move assignment
    expected<int> exp5;
    exp5 = std::move(exp2);
    EXPECT_TRUE(exp5.has_value());
    EXPECT_EQ(exp5.value(), 42);
    // exp2 is still valid, just contains moved-from state
}

// Test expected constructors with error
TEST_F(ExpectedTest, ExpectedConstructorsWithError) {
    // Error constructor
    Error<std::string> error("error message");
    expected<int> exp1(error);
    EXPECT_FALSE(exp1.has_value());
    EXPECT_EQ(exp1.error().error(), "error message");

    // Move error constructor
    expected<int> exp2(Error<std::string>("another error"));
    EXPECT_FALSE(exp2.has_value());
    EXPECT_EQ(exp2.error().error(), "another error");

    // Unexpected constructor
    unexpected<std::string> unex("unexpected error");
    expected<int> exp3(unex);
    EXPECT_FALSE(exp3.has_value());
    EXPECT_EQ(exp3.error().error(), "unexpected error");

    // Move unexpected constructor
    expected<int> exp4(unexpected<std::string>("moved unexpected"));
    EXPECT_FALSE(exp4.has_value());
    EXPECT_EQ(exp4.error().error(), "moved unexpected");

    // Copy constructor
    expected<int> exp5 = exp1;
    EXPECT_FALSE(exp5.has_value());
    EXPECT_EQ(exp5.error().error(), "error message");

    // Move assignment
    expected<int> exp6;
    exp6 = std::move(exp1);
    EXPECT_FALSE(exp6.has_value());
    EXPECT_EQ(exp6.error().error(), "error message");
}

// Test expected observers
TEST_F(ExpectedTest, ExpectedObservers) {
    // Value case
    expected<int> exp_val(42);
    EXPECT_TRUE(exp_val.has_value());
    EXPECT_TRUE(static_cast<bool>(exp_val));
    EXPECT_EQ(exp_val.value(), 42);

    // Const reference access
    const expected<int>& const_exp_val = exp_val;
    EXPECT_EQ(const_exp_val.value(), 42);

    // Error case
    expected<int> exp_err(Error<std::string>("error message"));
    EXPECT_FALSE(exp_err.has_value());
    EXPECT_FALSE(static_cast<bool>(exp_err));
    EXPECT_EQ(exp_err.error().error(), "error message");

    // Const reference error access
    const expected<int>& const_exp_err = exp_err;
    EXPECT_EQ(const_exp_err.error().error(), "error message");

    // Access exceptions
    EXPECT_THROW(
        {
            auto _ = exp_err.value();
            (void)_;
        },
        std::logic_error);
    EXPECT_THROW(
        {
            auto _ = exp_val.error();
            (void)_;
        },
        std::logic_error);
}

// Test move semantics with value and error access
TEST_F(ExpectedTest, MoveSemantics) {
    // Move value
    expected<std::string> exp_val(std::string("hello"));
    std::string moved_val = std::move(exp_val).value();
    EXPECT_EQ(moved_val, "hello");

    // Move error
    expected<int> exp_err(Error<std::string>("error message"));
    Error<std::string> moved_err = std::move(exp_err).error();
    EXPECT_EQ(moved_err.error(), "error message");
}

// Test and_then operation
TEST_F(ExpectedTest, AndThen) {
    auto increment = [](int val) -> expected<int> {
        return expected<int>(val + 1);
    };

    auto fail = [](int) -> expected<int> {
        return unexpected<std::string>("failed");
    };

    // Success case
    expected<int> exp_val(41);
    auto result1 = exp_val.and_then(increment);
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), 42);

    // Chained success
    auto result2 = exp_val.and_then(increment).and_then(increment);
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value(), 43);

    // Failure in chain
    auto result3 =
        exp_val.and_then(increment).and_then(fail).and_then(increment);
    EXPECT_FALSE(result3.has_value());
    EXPECT_EQ(result3.error().error(), "failed");

    // Starting with error
    expected<int> exp_err(Error<std::string>("initial error"));
    auto result4 = exp_err.and_then(increment);
    EXPECT_FALSE(result4.has_value());
    EXPECT_EQ(result4.error().error(), "initial error");

    // Test with const reference
    const expected<int>& const_exp = exp_val;
    auto result5 = const_exp.and_then(increment);
    EXPECT_TRUE(result5.has_value());
    EXPECT_EQ(result5.value(), 42);

    // Test with rvalue reference
    auto result6 = std::move(exp_val).and_then(increment);
    EXPECT_TRUE(result6.has_value());
    EXPECT_EQ(result6.value(), 42);
}

// Test map operation
TEST_F(ExpectedTest, Map) {
    auto double_val = [](int val) { return val * 2; };
    auto to_string = [](int val) { return std::to_string(val); };

    // Success case
    expected<int> exp_val(21);
    auto result1 = exp_val.map(double_val);
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), 42);

    // Type transformation
    auto result2 = exp_val.map(to_string);
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value(), "21");

    // Error case propagation
    expected<int> exp_err(Error<std::string>("error"));
    auto result3 = exp_err.map(double_val);
    EXPECT_FALSE(result3.has_value());
    EXPECT_EQ(result3.error().error(), "error");

    // Test with const reference
    const expected<int>& const_exp = exp_val;
    auto result4 = const_exp.map(double_val);
    EXPECT_TRUE(result4.has_value());
    EXPECT_EQ(result4.value(), 42);

    // Test with rvalue reference
    auto result5 = std::move(exp_val).map(double_val);
    EXPECT_TRUE(result5.has_value());
    EXPECT_EQ(result5.value(), 42);
}

// Test transform_error operation
TEST_F(ExpectedTest, TransformError) {
    auto append_info = [](const std::string& err) {
        return err + " (additional info)";
    };
    auto to_custom_error = [](const std::string& err) {
        return CustomError{500, err};
    };

    // Success case (unchanged)
    expected<int> exp_val(42);
    auto result1 = exp_val.transform_error(append_info);
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), 42);

    // Error transformation
    expected<int> exp_err(Error<std::string>("error message"));
    auto result2 = exp_err.transform_error(append_info);
    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error().error(), "error message (additional info)");

    // Error type transformation
    auto result3 = exp_err.transform_error(to_custom_error);
    EXPECT_FALSE(result3.has_value());
    EXPECT_EQ(result3.error().error().code, 500);
    EXPECT_EQ(result3.error().error().message, "error message");

    // Test with const reference
    const expected<int>& const_exp_err = exp_err;
    auto result4 = const_exp_err.transform_error(append_info);
    EXPECT_FALSE(result4.has_value());
    EXPECT_EQ(result4.error().error(), "error message (additional info)");

    // Test with rvalue reference
    expected<int> exp_err_move(Error<std::string>("error message"));
    auto result5 = std::move(exp_err_move).transform_error(append_info);
    EXPECT_FALSE(result5.has_value());
    EXPECT_EQ(result5.error().error(), "error message (additional info)");
}

// Test equality operators
TEST_F(ExpectedTest, EqualityOperators) {
    expected<int> exp1(42);
    expected<int> exp2(42);
    expected<int> exp3(43);
    expected<int> exp4(Error<std::string>("error1"));
    expected<int> exp5(Error<std::string>("error1"));
    expected<int> exp6(Error<std::string>("error2"));

    // Same value
    EXPECT_TRUE(exp1 == exp2);
    EXPECT_FALSE(exp1 != exp2);

    // Different value
    EXPECT_FALSE(exp1 == exp3);
    EXPECT_TRUE(exp1 != exp3);

    // Same error
    EXPECT_TRUE(exp4 == exp5);
    EXPECT_FALSE(exp4 != exp5);

    // Different error
    EXPECT_FALSE(exp4 == exp6);
    EXPECT_TRUE(exp4 != exp6);

    // Value vs error
    EXPECT_FALSE(exp1 == exp4);
    EXPECT_TRUE(exp1 != exp4);
}

// Test helper functions
TEST_F(ExpectedTest, HelperFunctions) {
    // make_expected
    auto exp1 = make_expected(42);
    EXPECT_TRUE(exp1.has_value());
    EXPECT_EQ(exp1.value(), 42);

    // make_unexpected with value
    auto unex1 = make_unexpected(std::string("error"));
    expected<int> exp2(unex1);
    EXPECT_FALSE(exp2.has_value());
    EXPECT_EQ(exp2.error().error(), "error");

    // make_unexpected with move
    auto unex2 = make_unexpected(std::string("moved error"));
    expected<int> exp3(unex2);
    EXPECT_FALSE(exp3.has_value());
    EXPECT_EQ(exp3.error().error(), "moved error");

    // make_unexpected with C-string
    auto unex3 = make_unexpected("C-string error");
    expected<int> exp4(unex3);
    EXPECT_FALSE(exp4.has_value());
    EXPECT_EQ(exp4.error().error(), "C-string error");
}

// Test void specialization
TEST_F(ExpectedTest, VoidSpecialization) {
    // Success case
    expected<void> exp1;
    EXPECT_TRUE(exp1.has_value());
    EXPECT_NO_THROW(exp1.value());

    // Error case
    expected<void> exp2(Error<std::string>("void error"));
    EXPECT_FALSE(exp2.has_value());
    EXPECT_EQ(exp2.error().error(), "void error");

    // and_then operation
    auto succeed = []() -> expected<int> { return expected<int>(42); };
    auto fail = []() -> expected<int> {
        return unexpected<std::string>("operation failed");
    };

    auto result1 = exp1.and_then(succeed);
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), 42);

    auto result2 = exp2.and_then(succeed);
    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error().error(), "void error");

    auto result3 = exp1.and_then(fail);
    EXPECT_FALSE(result3.has_value());
    EXPECT_EQ(result3.error().error(), "operation failed");

    // transform_error operation
    auto append_void = [](const std::string& err) {
        return err + " (void context)";
    };

    auto result4 = exp2.transform_error(
        [&](const std::string& err) { return append_void(err); });
    EXPECT_FALSE(result4.has_value());
    EXPECT_EQ(result4.error().error(), "void error (void context)");

    // Equality operators
    expected<void> exp3;
    expected<void> exp4(Error<std::string>("void error"));
    expected<void> exp5(Error<std::string>("void error"));
    expected<void> exp6(Error<std::string>("another error"));

    EXPECT_TRUE(exp1 == exp3);   // Both have value
    EXPECT_TRUE(exp4 == exp5);   // Same error
    EXPECT_FALSE(exp4 == exp6);  // Different error
    EXPECT_FALSE(exp1 == exp4);  // Value vs error
}

// Test complex operations
TEST_F(ExpectedTest, ComplexOperations) {
    // Create a chain of expected transformations
    auto starts_with_a = [](const std::string& s) -> expected<std::string> {
        if (s.empty() || s[0] != 'a') {
            return unexpected<std::string>("String doesn't start with 'a'");
        }
        return s;
    };

    auto to_uppercase = [](const std::string& s) -> expected<std::string> {
        std::string result;
        result.reserve(s.size());
        for (char c : s) {
            result.push_back(std::toupper(c));
        }
        return result;
    };

    auto to_length = [](const std::string& s) -> int {
        return static_cast<int>(s.size());
    };

    // Success chain
    expected<std::string> exp1("apple");
    auto result1 =
        exp1.and_then(starts_with_a).and_then(to_uppercase).map(to_length);

    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), 5);

    // Failure in chain
    expected<std::string> exp2("banana");
    auto result2 =
        exp2.and_then(starts_with_a).and_then(to_uppercase).map(to_length);

    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error().error(), "String doesn't start with 'a'");

    // Error transformation
    auto add_code = [](const std::string& err) -> std::string {
        return "Error code 101: " + err;
    };

    auto result3 = result2.transform_error(add_code);
    EXPECT_FALSE(result3.has_value());
    EXPECT_EQ(result3.error().error(),
              "Error code 101: String doesn't start with 'a'");
}

// Test with custom types
struct Person {
    std::string name;
    int age;

    bool operator==(const Person& other) const {
        return name == other.name && age == other.age;
    }
};

TEST_F(ExpectedTest, CustomTypes) {
    Person person{"Alice", 30};
    expected<Person> exp1(person);

    EXPECT_TRUE(exp1.has_value());
    EXPECT_EQ(exp1.value().name, "Alice");
    EXPECT_EQ(exp1.value().age, 30);

    auto get_name = [](const Person& p) -> std::string { return p.name; };

    auto age_check = [](const Person& p) -> expected<Person> {
        if (p.age < 18) {
            return unexpected<std::string>("Person is underage");
        }
        return p;
    };

    auto result1 = exp1.and_then(age_check).map(get_name);
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), "Alice");

    // Test with underage person
    Person young{"Bob", 15};
    expected<Person> exp2(young);
    auto result2 = exp2.and_then(age_check);
    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error().error(), "Person is underage");
}

}  // namespace