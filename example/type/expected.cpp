#include "atom/type/expected.hpp"

#include <iostream>
#include <string>

using namespace atom::type;

int main() {
    // Create an expected object with a value
    expected<int> valueExpected(42);
    if (valueExpected.has_value()) {
        std::cout << "Value: " << valueExpected.value() << std::endl;
    } else {
        std::cout << "Error: " << valueExpected.error().error() << std::endl;
    }

    // Create an expected object with an error
    expected<int> errorExpected(make_unexpected("An error occurred"));
    if (errorExpected.has_value()) {
        std::cout << "Value: " << errorExpected.value() << std::endl;
    } else {
        std::cout << "Error: " << errorExpected.error().error() << std::endl;
    }

    // Use and_then to apply a function to the value if it exists
    auto result = valueExpected.and_then([](int val) -> expected<std::string> {
        return std::to_string(val);
    });
    if (result.has_value()) {
        std::cout << "Result: " << result.value() << std::endl;
    } else {
        std::cout << "Error: " << result.error().error() << std::endl;
    }

    // Use map to transform the value if it exists
    auto mappedResult = valueExpected.map([](int val) {
        return val * 2;
    });
    if (mappedResult.has_value()) {
        std::cout << "Mapped Result: " << mappedResult.value() << std::endl;
    } else {
        std::cout << "Error: " << mappedResult.error().error() << std::endl;
    }

    /*
    TODO: Fix this
    // Use transform_error to transform the error if it exists
    auto transformedError = errorExpected.transform_error([](const std::string& err) {
        return std::string("Transformed: ") + err;
    });
    if (transformedError.has_value()) {
        std::cout << "Value: " << transformedError.value() << std::endl;
    } else {
        std::cout << "Transformed Error: " << transformedError.error().error() << std::endl;
    }
    */

    // Create an expected<void> object with no value
    expected<void> voidExpected;
    if (voidExpected.has_value()) {
        std::cout << "Void expected has value" << std::endl;
    } else {
        std::cout << "Error: " << voidExpected.error().error() << std::endl;
    }

    // Create an expected<void> object with an error
    expected<void> voidErrorExpected(make_unexpected("Void error occurred"));
    if (voidErrorExpected.has_value()) {
        std::cout << "Void expected has value" << std::endl;
    } else {
        std::cout << "Error: " << voidErrorExpected.error().error() << std::endl;
    }

    // Use and_then with expected<void>
    auto voidResult = voidExpected.and_then([]() -> expected<void> {
        std::cout << "Void and_then executed" << std::endl;
        return expected<void>();
    });
    if (voidResult.has_value()) {
        std::cout << "Void result has value" << std::endl;
    } else {
        std::cout << "Error: " << voidResult.error().error() << std::endl;
    }

    /*
    TODO: Fix this
    // Use transform_error with expected<void>
    auto voidTransformedError = voidErrorExpected.transform_error([](const std::string& err) {
        return std::string("Void Transformed: ") + err;
    });
    if (voidTransformedError.has_value()) {
        std::cout << "Void transformed error has value" << std::endl;
    } else {
        std::cout << "Void Transformed Error: " << voidTransformedError.error().error() << std::endl;
    }
    */

    return 0;
}