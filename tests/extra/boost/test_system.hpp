#ifndef ATOM_EXTRA_BOOST_TEST_SYSTEM_HPP
#define ATOM_EXTRA_BOOST_TEST_SYSTEM_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "atom/extra/boost/system.hpp"

namespace atom::extra::boost::test {

using ::testing::_;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::ThrowsMessage;

class ErrorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up common error codes for testing
        // Invalid argument error
        invalidArgError = Error(::boost::system::errc::invalid_argument,
                                ::boost::system::generic_category());

        // File not found error
        fileNotFoundError =
            Error(::boost::system::errc::no_such_file_or_directory,
                  ::boost::system::generic_category());

        // Default error (no error)
        defaultError = Error();
    }

    Error invalidArgError;
    Error fileNotFoundError;
    Error defaultError;
};

TEST_F(ErrorTest, DefaultConstructor) {
    // Default-constructed error should be falsy
    EXPECT_FALSE(static_cast<bool>(defaultError));

    // Default error should have value 0
    EXPECT_EQ(defaultError.value(), 0);

    // Default error's message shouldn't be empty
    EXPECT_FALSE(defaultError.message().empty());
}

TEST_F(ErrorTest, ConstructFromErrorCode) {
    ::boost::system::error_code ec(::boost::system::errc::permission_denied,
                                   ::boost::system::generic_category());

    Error error(ec);

    EXPECT_TRUE(static_cast<bool>(error));
    EXPECT_EQ(error.value(),
              static_cast<int>(::boost::system::errc::permission_denied));
    EXPECT_EQ(error.category(), ::boost::system::generic_category());
    EXPECT_THAT(error.message(), HasSubstr("Permission denied"));
}

TEST_F(ErrorTest, ConstructFromValueAndCategory) {
    Error error(
        static_cast<int>(::boost::system::errc::operation_not_permitted),
        ::boost::system::generic_category());

    EXPECT_TRUE(static_cast<bool>(error));
    EXPECT_EQ(error.value(),
              static_cast<int>(::boost::system::errc::operation_not_permitted));
    EXPECT_EQ(error.category(), ::boost::system::generic_category());
    EXPECT_THAT(error.message(), HasSubstr("Operation not permitted"));
}

TEST_F(ErrorTest, ValueMethod) {
    EXPECT_EQ(invalidArgError.value(),
              static_cast<int>(::boost::system::errc::invalid_argument));
    EXPECT_EQ(
        fileNotFoundError.value(),
        static_cast<int>(::boost::system::errc::no_such_file_or_directory));
    EXPECT_EQ(defaultError.value(), 0);
}

TEST_F(ErrorTest, CategoryMethod) {
    EXPECT_EQ(invalidArgError.category(), ::boost::system::generic_category());
    EXPECT_EQ(fileNotFoundError.category(),
              ::boost::system::generic_category());
}

TEST_F(ErrorTest, MessageMethod) {
    EXPECT_THAT(invalidArgError.message(), HasSubstr("Invalid argument"));
    EXPECT_THAT(fileNotFoundError.message(),
                HasSubstr("No such file or directory"));
}

TEST_F(ErrorTest, BoolConversion) {
    EXPECT_TRUE(static_cast<bool>(invalidArgError));
    EXPECT_TRUE(static_cast<bool>(fileNotFoundError));
    EXPECT_FALSE(static_cast<bool>(defaultError));
}

TEST_F(ErrorTest, ToBoostErrorCode) {
    auto ec = invalidArgError.toBoostErrorCode();
    EXPECT_EQ(ec.value(), invalidArgError.value());
    EXPECT_EQ(ec.category(), invalidArgError.category());

    ec = defaultError.toBoostErrorCode();
    EXPECT_EQ(ec.value(), 0);
}

TEST_F(ErrorTest, EqualityOperator) {
    // Same error codes should be equal
    Error error1(::boost::system::errc::invalid_argument,
                 ::boost::system::generic_category());
    Error error2(::boost::system::errc::invalid_argument,
                 ::boost::system::generic_category());
    EXPECT_EQ(error1, error2);

    // Different error codes should be unequal
    EXPECT_NE(invalidArgError, fileNotFoundError);

    // Default error should be equal to itself
    EXPECT_EQ(defaultError, defaultError);

    // Default error should not equal other errors
    EXPECT_NE(defaultError, invalidArgError);
}

TEST_F(ErrorTest, InequalityOperator) {
    Error error1(::boost::system::errc::invalid_argument,
                 ::boost::system::generic_category());
    Error error2(::boost::system::errc::invalid_argument,
                 ::boost::system::generic_category());

    EXPECT_FALSE(error1 != error2);
    EXPECT_TRUE(invalidArgError != fileNotFoundError);
    EXPECT_FALSE(defaultError != defaultError);
    EXPECT_TRUE(defaultError != invalidArgError);
}

class ExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up errors for testing exceptions
        invalidArgError = Error(::boost::system::errc::invalid_argument,
                                ::boost::system::generic_category());

        fileNotFoundError =
            Error(::boost::system::errc::no_such_file_or_directory,
                  ::boost::system::generic_category());
    }

    Error invalidArgError;
    Error fileNotFoundError;
};

TEST_F(ExceptionTest, Construction) {
    // Test that exceptions can be constructed from errors
    EXPECT_NO_THROW({ Exception ex(invalidArgError); });

    Exception ex(invalidArgError);
    EXPECT_EQ(ex.code().value(), invalidArgError.value());
    EXPECT_EQ(ex.code().category(), invalidArgError.category());
    EXPECT_THAT(ex.what(), HasSubstr("Invalid argument"));
}

TEST_F(ExceptionTest, ErrorMethod) {
    Exception ex(invalidArgError);
    Error error = ex.error();

    EXPECT_EQ(error.value(), invalidArgError.value());
    // Note: The category might be different due to how std::system_error stores
    // the category
    EXPECT_THAT(error.message(), HasSubstr("Invalid argument"));
}

TEST_F(ExceptionTest, InheritanceAndCatching) {
    try {
        throw Exception(fileNotFoundError);
    } catch (const std::system_error& e) {
        EXPECT_EQ(e.code().value(), fileNotFoundError.value());
        EXPECT_THAT(e.what(), HasSubstr("No such file or directory"));
    }

    try {
        throw Exception(fileNotFoundError);
    } catch (const std::exception& e) {
        EXPECT_THAT(e.what(), HasSubstr("No such file or directory"));
    }
}

class ResultTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up errors for testing results
        invalidArgError = Error(::boost::system::errc::invalid_argument,
                                ::boost::system::generic_category());

        fileNotFoundError =
            Error(::boost::system::errc::no_such_file_or_directory,
                  ::boost::system::generic_category());
    }

    Error invalidArgError;
    Error fileNotFoundError;
};

TEST_F(ResultTest, ValueConstructor) {
    // Test with int
    Result<int> intResult(42);
    EXPECT_TRUE(intResult.hasValue());
    EXPECT_EQ(intResult.value(), 42);

    // Test with string
    Result<std::string> stringResult(std::string("test"));
    EXPECT_TRUE(stringResult.hasValue());
    EXPECT_EQ(stringResult.value(), "test");

    // Test with bool
    Result<bool> boolResult(true);
    EXPECT_TRUE(boolResult.hasValue());
    EXPECT_TRUE(boolResult.value());
}

TEST_F(ResultTest, ErrorConstructor) {
    // Test with int
    Result<int> intResult(invalidArgError);
    EXPECT_FALSE(intResult.hasValue());
    EXPECT_THROW(intResult.value(), Exception);
    EXPECT_EQ(intResult.error(), invalidArgError);

    // Test with string
    Result<std::string> stringResult(fileNotFoundError);
    EXPECT_FALSE(stringResult.hasValue());
    EXPECT_THROW(stringResult.value(), Exception);
    EXPECT_EQ(stringResult.error(), fileNotFoundError);
}

TEST_F(ResultTest, HasValueMethod) {
    Result<int> successResult(42);
    EXPECT_TRUE(successResult.hasValue());

    Result<int> errorResult(invalidArgError);
    EXPECT_FALSE(errorResult.hasValue());
}

TEST_F(ResultTest, ValueMethodLValue) {
    Result<int> successResult(42);
    EXPECT_EQ(successResult.value(), 42);

    Result<int> errorResult(invalidArgError);
    EXPECT_THROW(
        {
            try {
                errorResult.value();
            } catch (const Exception& e) {
                EXPECT_EQ(e.error().value(), invalidArgError.value());
                throw;
            }
        },
        Exception);
}

TEST_F(ResultTest, ValueMethodRValue) {
    // Testing r-value reference return of value() method
    auto getValue = [](Result<std::string> result) {
        return std::move(result).value();
    };

    Result<std::string> successResult(std::string("test"));
    std::string value = getValue(successResult);
    EXPECT_EQ(value, "test");

    Result<std::string> errorResult(invalidArgError);
    EXPECT_THROW(getValue(errorResult), Exception);
}

TEST_F(ResultTest, ErrorMethod) {
    Result<int> successResult(42);
    EXPECT_FALSE(static_cast<bool>(successResult.error()));

    Result<int> errorResult(invalidArgError);
    EXPECT_EQ(errorResult.error(), invalidArgError);

    // Test r-value reference return
    auto getError = [](Result<int> result) {
        return std::move(result).error();
    };

    Error error = getError(errorResult);
    EXPECT_EQ(error, invalidArgError);
}

TEST_F(ResultTest, BoolConversion) {
    Result<int> successResult(42);
    EXPECT_TRUE(static_cast<bool>(successResult));

    Result<int> errorResult(invalidArgError);
    EXPECT_FALSE(static_cast<bool>(errorResult));
}

TEST_F(ResultTest, ValueOrMethod) {
    Result<int> successResult(42);
    EXPECT_EQ(successResult.valueOr(0), 42);

    Result<int> errorResult(invalidArgError);
    EXPECT_EQ(errorResult.valueOr(0), 0);

    // Test with different return type that's convertible
    Result<long> longResult(42L);
    EXPECT_EQ(longResult.valueOr(0), 42L);

    // Test with default value of different type
    Result<std::string> strResult(invalidArgError);
    EXPECT_EQ(strResult.valueOr("default"), "default");
}

TEST_F(ResultTest, MapMethod) {
    // Test with successful result
    Result<int> successResult(42);
    auto mappedSuccess = successResult.map([](int value) { return value * 2; });

    static_assert(std::is_same_v<decltype(mappedSuccess), Result<int>>,
                  "map should return a Result with the mapped type");

    EXPECT_TRUE(mappedSuccess.hasValue());
    EXPECT_EQ(mappedSuccess.value(), 84);

    // Test with error result
    Result<int> errorResult(invalidArgError);
    auto mappedError = errorResult.map([](int value) { return value * 2; });

    EXPECT_FALSE(mappedError.hasValue());
    EXPECT_EQ(mappedError.error(), invalidArgError);

    // Test with type conversion
    auto mappedType =
        successResult.map([](int value) { return std::to_string(value); });

    static_assert(std::is_same_v<decltype(mappedType), Result<std::string>>,
                  "map should handle type conversion");

    EXPECT_TRUE(mappedType.hasValue());
    EXPECT_EQ(mappedType.value(), "42");
}

TEST_F(ResultTest, AndThenMethod) {
    // Test with successful result
    Result<int> successResult(42);
    auto chainedSuccess = successResult.andThen(
        [](int value) { return Result<std::string>(std::to_string(value)); });

    static_assert(std::is_same_v<decltype(chainedSuccess), Result<std::string>>,
                  "andThen should return the result of the function");

    EXPECT_TRUE(chainedSuccess.hasValue());
    EXPECT_EQ(chainedSuccess.value(), "42");

    // Test with error result
    Result<int> errorResult(invalidArgError);
    auto chainedError = errorResult.andThen(
        [](int value) { return Result<std::string>(std::to_string(value)); });

    EXPECT_FALSE(chainedError.hasValue());
    EXPECT_EQ(chainedError.error(), invalidArgError);

    // Test with function returning error
    auto chainedToError = successResult.andThen(
        [this](int) { return Result<std::string>(this->fileNotFoundError); });

    EXPECT_FALSE(chainedToError.hasValue());
    EXPECT_EQ(chainedToError.error(), fileNotFoundError);
}

class ResultVoidTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up errors for testing void results
        invalidArgError = Error(::boost::system::errc::invalid_argument,
                                ::boost::system::generic_category());

        fileNotFoundError =
            Error(::boost::system::errc::no_such_file_or_directory,
                  ::boost::system::generic_category());
    }

    Error invalidArgError;
    Error fileNotFoundError;
};

TEST_F(ResultVoidTest, DefaultConstructor) {
    Result<void> result;
    EXPECT_TRUE(result.hasValue());
    EXPECT_FALSE(static_cast<bool>(result.error()));
}

TEST_F(ResultVoidTest, ErrorConstructor) {
    Result<void> result(invalidArgError);
    EXPECT_FALSE(result.hasValue());
    EXPECT_EQ(result.error(), invalidArgError);
}

TEST_F(ResultVoidTest, HasValueMethod) {
    Result<void> successResult;
    EXPECT_TRUE(successResult.hasValue());

    Result<void> errorResult(invalidArgError);
    EXPECT_FALSE(errorResult.hasValue());
}

TEST_F(ResultVoidTest, ErrorMethod) {
    Result<void> successResult;
    EXPECT_FALSE(static_cast<bool>(successResult.error()));

    Result<void> errorResult(invalidArgError);
    EXPECT_EQ(errorResult.error(), invalidArgError);

    // Test r-value reference return
    auto getError = [](Result<void> result) {
        return std::move(result).error();
    };

    Error error = getError(errorResult);
    EXPECT_EQ(error, invalidArgError);
}

TEST_F(ResultVoidTest, BoolConversion) {
    Result<void> successResult;
    EXPECT_TRUE(static_cast<bool>(successResult));

    Result<void> errorResult(invalidArgError);
    EXPECT_FALSE(static_cast<bool>(errorResult));
}

class MakeResultTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up errors for testing makeResult
        invalidArgError = Error(::boost::system::errc::invalid_argument,
                                ::boost::system::generic_category());
    }

    Error invalidArgError;
};

TEST_F(MakeResultTest, SuccessfulFunction) {
    auto result = makeResult([]() { return 42; });

    static_assert(std::is_same_v<decltype(result), Result<int>>,
                  "makeResult should deduce the correct return type");

    EXPECT_TRUE(result.hasValue());
    EXPECT_EQ(result.value(), 42);
}

TEST_F(MakeResultTest, FunctionThrowingException) {
    auto result = makeResult([]() -> std::string {
        throw Exception(Error(::boost::system::errc::invalid_argument,
                              ::boost::system::generic_category()));
    });

    EXPECT_FALSE(result.hasValue());
    EXPECT_EQ(result.error().value(),
              static_cast<int>(::boost::system::errc::invalid_argument));
}

TEST_F(MakeResultTest, FunctionThrowingStdException) {
    auto result =
        makeResult([]() -> double { throw std::runtime_error("Test error"); });

    EXPECT_FALSE(result.hasValue());
    EXPECT_EQ(result.error().value(),
              static_cast<int>(::boost::system::errc::invalid_argument));
}

TEST_F(MakeResultTest, VoidFunction) {
    bool executed = false;

    auto result = makeResult([&executed]() { executed = true; });

    static_assert(std::is_same_v<decltype(result), Result<void>>,
                  "makeResult should work with void functions");

    EXPECT_TRUE(result.hasValue());
    EXPECT_TRUE(executed);
}

TEST_F(MakeResultTest, VoidFunctionThrowingException) {
    auto result = makeResult([]() {
        throw Exception(Error(::boost::system::errc::invalid_argument,
                              ::boost::system::generic_category()));
    });

    EXPECT_FALSE(result.hasValue());
    EXPECT_EQ(result.error().value(),
              static_cast<int>(::boost::system::errc::invalid_argument));
}

// Integration tests
class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up common resources for integration tests
    }
};

TEST_F(IntegrationTest, ErrorToExceptionToResult) {
    // Create an error
    Error error(::boost::system::errc::invalid_argument,
                ::boost::system::generic_category());

    // Convert to exception
    Exception exception(error);

    // Capture in a result
    auto result = makeResult([&exception]() -> int {
        throw exception;
        return 0;
    });

    EXPECT_FALSE(result.hasValue());
    EXPECT_EQ(result.error().value(), error.value());
}

TEST_F(IntegrationTest, ResultChaining) {
    // Define some functions that return Results
    auto parseNumber = [](const std::string& str) -> Result<int> {
        try {
            return Result<int>(std::stoi(str));
        } catch (const std::exception&) {
            return Result<int>(Error(::boost::system::errc::invalid_argument,
                                     ::boost::system::generic_category()));
        }
    };

    auto doubleNumber = [](int value) -> Result<int> {
        return Result<int>(value * 2);
    };

    auto numberToString = [](int value) -> Result<std::string> {
        return Result<std::string>(std::to_string(value));
    };

    // Chain successful operations
    auto result1 =
        parseNumber("21").andThen(doubleNumber).andThen(numberToString);

    EXPECT_TRUE(result1.hasValue());
    EXPECT_EQ(result1.value(), "42");

    // Chain with an error in the middle
    auto result2 =
        parseNumber("invalid").andThen(doubleNumber).andThen(numberToString);

    EXPECT_FALSE(result2.hasValue());
    EXPECT_EQ(result2.error().value(),
              static_cast<int>(::boost::system::errc::invalid_argument));
}

TEST_F(IntegrationTest, ResultMapping) {
    // Define a function that returns a Result
    auto parseNumber = [](const std::string& str) -> Result<int> {
        try {
            return Result<int>(std::stoi(str));
        } catch (const std::exception&) {
            return Result<int>(Error(::boost::system::errc::invalid_argument,
                                     ::boost::system::generic_category()));
        }
    };

    // Map successful result
    auto result1 = parseNumber("21")
                       .map([](int value) { return value * 2; })
                       .map([](int value) { return std::to_string(value); });

    EXPECT_TRUE(result1.hasValue());
    EXPECT_EQ(result1.value(), "42");

    // Map with an error
    auto result2 =
        parseNumber("invalid").map([](int value) { return value * 2; });

    EXPECT_FALSE(result2.hasValue());
    EXPECT_EQ(result2.error().value(),
              static_cast<int>(::boost::system::errc::invalid_argument));
}

}  // namespace atom::extra::boost::test

#endif  // ATOM_EXTRA_BOOST_TEST_SYSTEM_HPP