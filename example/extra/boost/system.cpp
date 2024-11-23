#include "atom/extra/boost/system.hpp"

#include <boost/system/error_code.hpp>
#include <iostream>

using namespace atom::extra::boost;

int main() {
    // Create an Error object using a Boost.System error code
    boost::system::error_code ec(boost::system::errc::make_error_code(
        boost::system::errc::invalid_argument));
    Error error(ec);

    // Get the error value
    int errorValue = error.value();
    std::cout << "Error value: " << errorValue << std::endl;

    // Get the error category
    const auto& errorCategory = error.category();
    std::cout << "Error category: " << errorCategory.name() << std::endl;

    // Get the error message
    std::string errorMessage = error.message();
    std::cout << "Error message: " << errorMessage << std::endl;

    // Check if the error code is valid
    bool isValid = static_cast<bool>(error);
    std::cout << "Is error valid: " << std::boolalpha << isValid << std::endl;

    // Convert to a Boost.System error code
    boost::system::error_code boostEc = error.toBoostErrorCode();
    std::cout << "Boost error code message: " << boostEc.message() << std::endl;

    // Create an Exception object from an Error
    Exception exception(error);
    std::cout << "Exception message: " << exception.what() << std::endl;

    // Create a Result object with a value
    Result<int> resultValue(42);

    // Check if the Result has a value
    if (resultValue.hasValue()) {
        // Get the result value
        int value = resultValue.value();
        std::cout << "Result value: " << value << std::endl;
    }

    // Create a Result object with an Error
    Result<int> resultError(error);

    // Check if the Result has a value
    if (!resultError.hasValue()) {
        // Get the associated Error
        Error resultErr = resultError.error();
        std::cout << "Result error message: " << resultErr.message()
                  << std::endl;
    }

    // Get the result value or a default value
    int defaultValue = resultError.valueOr(100);
    std::cout << "Result value or default: " << defaultValue << std::endl;

    // Apply a function to the result value if it exists
    auto mappedResult = resultValue.map([](int x) { return x * 2; });
    if (mappedResult.hasValue()) {
        std::cout << "Mapped result value: " << mappedResult.value()
                  << std::endl;
    }

    // Apply a function to the result value if it exists
    auto andThenResult =
        resultValue.andThen([](int x) { return Result<int>(x * 2); });
    if (andThenResult.hasValue()) {
        std::cout << "AndThen result value: " << andThenResult.value()
                  << std::endl;
    }

    // Create a Result<void> object with an Error
    Result<void> resultVoidError(error);

    // Check if the Result<void> has a value
    if (!resultVoidError.hasValue()) {
        // Get the associated Error
        Error voidError = resultVoidError.error();
        std::cout << "Result<void> error message: " << voidError.message()
                  << std::endl;
    }

    // Create a Result from a function
    auto resultFromFunction = makeResult([]() -> int {
        // Simulate some operation that may throw an exception
        throw Exception(Error(boost::system::errc::make_error_code(
            boost::system::errc::invalid_argument)));
        return 42;
    });

    // Check if the Result from the function has a value
    if (!resultFromFunction.hasValue()) {
        // Get the associated Error
        Error functionError = resultFromFunction.error();
        std::cout << "Result from function error message: "
                  << functionError.message() << std::endl;
    }

    return 0;
}