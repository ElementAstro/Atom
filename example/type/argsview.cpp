#include "atom/type/argsview.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <sstream>

using namespace atom;

template <typename T>
std::string to_string_custom(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

int main() {
    // Create an ArgsView object with multiple arguments
    ArgsView<int, double, std::string> argsView(42, 3.14, "example");

    // Get the argument at the specified index
    int intValue = argsView.get<0>();
    double doubleValue = argsView.get<1>();
    std::string strValue = argsView.get<2>();

    std::cout << "Integer value: " << intValue << std::endl;
    std::cout << "Double value: " << doubleValue << std::endl;
    std::cout << "String value: " << strValue << std::endl;

    // Get the number of arguments
    std::size_t size = argsView.size();
    std::cout << "Number of arguments: " << size << std::endl;

    // Check if there are no arguments
    bool isEmpty = argsView.empty();
    std::cout << "Is empty: " << std::boolalpha << isEmpty << std::endl;

    // Apply a function to each argument
    argsView.forEach([](const auto& arg) {
        std::cout << "Argument: " << arg << std::endl;
    });

    // Transform the arguments using a function
    auto transformedView = argsView.transform([](const auto& arg) {
        return to_string_custom(arg);
    });
    transformedView.forEach([](const auto& arg) {
        std::cout << "Transformed argument: " << arg << std::endl;
    });

    // Convert the arguments to a tuple
    auto tuple = argsView.toTuple();
    std::cout << "Tuple: (" << std::get<0>(tuple) << ", " << std::get<1>(tuple) << ", " << std::get<2>(tuple) << ")" << std::endl;
    
    /*
    TODO: Fix this
    // Accumulate the arguments using a function and an initial value
    int sum = argsView.accumulate([](int a, const auto& b) {
        return a + static_cast<int>(b);
    }, 0);
    std::cout << "Sum of arguments: " << sum << std::endl;
    */
    

    // Apply a function to the arguments
    auto result = argsView.apply([](int a, double b, const std::string& c) {
        return a + b + static_cast<double>(c.size());
    });
    std::cout << "Result of apply: " << result << std::endl;

    /*
    TODO: Fix this
    // Filter the arguments using a predicate
    auto filteredView = argsView.filter([](const auto& arg) {
        return arg != 42;
    });
    filteredView.forEach([](const auto& arg) {
        std::cout << "Filtered argument: " << arg << std::endl;
    });
    */
    

    /*
    TODO: Fix this
    // Find the first argument that satisfies a predicate
    auto found = argsView.find([](const auto& arg) {
        return arg == 3.14;
    });
    if (found) {
        std::cout << "Found argument: " << *found << std::endl;
    } else {
        std::cout << "Argument not found" << std::endl;
    }
    */

    // Check if the arguments contain a specific value
    bool containsValue = argsView.contains(42);
    std::cout << "Contains 42: " << std::boolalpha << containsValue << std::endl;

    // Sum the arguments
    int totalSum = sum(1, 2, 3, 4, 5);
    std::cout << "Total sum: " << totalSum << std::endl;

    // Concatenate the arguments into a string
    std::string concatenated = concat("Hello, ", "world", "!");
    std::cout << "Concatenated string: " << concatenated << std::endl;

    /*
    TODO: Fix this
    // Apply a function to the arguments in an ArgsView
    auto appliedResult = apply([](int a, double b, const std::string& c) {
        return a * b * static_cast<double>(c.size());
    }, argsView);
    std::cout << "Applied result: " << appliedResult << std::endl;
    */
    
    // Apply a function to each argument in an ArgsView
    forEach([](const auto& arg) {
        std::cout << "ForEach argument: " << arg << std::endl;
    }, argsView);

    /*
    TODO: Fix this
    // Accumulate the arguments in an ArgsView using a function and an initial value
    int accumulatedResult = accumulate([](int a, const auto& b) {
        return a + static_cast<int>(b);
    }, 0, argsView);
    std::cout << "Accumulated result: " << accumulatedResult << std::endl;
    */
    
    // Create an ArgsView from the given arguments
    auto newArgsView = makeArgsView(1, 2.5, "test");
    newArgsView.forEach([](const auto& arg) {
        std::cout << "New ArgsView argument: " << arg << std::endl;
    });

    // Get the argument at the specified index in an ArgsView
    int newIntValue = get<0>(newArgsView);
    std::cout << "New integer value: " << newIntValue << std::endl;

    // Compare two ArgsView objects for equality
    bool areEqual = (argsView == newArgsView);
    std::cout << "ArgsView objects are equal: " << std::boolalpha << areEqual << std::endl;

    // Compare two ArgsView objects for inequality
    bool areNotEqual = (argsView != newArgsView);
    std::cout << "ArgsView objects are not equal: " << std::boolalpha << areNotEqual << std::endl;

    // Compare two ArgsView objects using less-than operator
    bool isLessThan = (argsView < newArgsView);
    std::cout << "ArgsView is less than newArgsView: " << std::boolalpha << isLessThan << std::endl;

    // Compare two ArgsView objects using less-than-or-equal-to operator
    bool isLessThanOrEqual = (argsView <= newArgsView);
    std::cout << "ArgsView is less than or equal to newArgsView: " << std::boolalpha << isLessThanOrEqual << std::endl;

    // Compare two ArgsView objects using greater-than operator
    bool isGreaterThan = (argsView > newArgsView);
    std::cout << "ArgsView is greater than newArgsView: " << std::boolalpha << isGreaterThan << std::endl;

    // Compare two ArgsView objects using greater-than-or-equal-to operator
    bool isGreaterThanOrEqual = (argsView >= newArgsView);
    std::cout << "ArgsView is greater than or equal to newArgsView: " << std::boolalpha << isGreaterThanOrEqual << std::endl;

    return 0;
}