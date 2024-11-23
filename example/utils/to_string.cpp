#include "atom/utils/to_string.hpp"

#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <variant>
#include <vector>

using namespace atom::utils;

int main() {
    // Convert a string type to std::string
    std::string str = "Hello, World!";
    std::cout << "String: " << toString(str) << std::endl;

    // Convert a char type to std::string
    char ch = 'A';
    std::cout << "Char: " << toString(ch) << std::endl;

    // Convert an enum type to std::string
    enum class Color { Red, Green, Blue };
    Color color = Color::Green;
    std::cout << "Enum: " << toString(color) << std::endl;

    // Convert a pointer type to std::string
    int intValue = 42;
    int* intPtr = &intValue;
    std::cout << "Pointer: " << toString(intPtr) << std::endl;

    // Convert a smart pointer type to std::string
    std::shared_ptr<int> smartPtr = std::make_shared<int>(42);
    std::cout << "SmartPointer: " << toString(smartPtr) << std::endl;

    // Convert a container type to std::string
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::cout << "Vector: " << toString(vec) << std::endl;

    // Convert a map type to std::string
    std::map<std::string, int> map = {{"one", 1}, {"two", 2}, {"three", 3}};
    std::cout << "Map: " << toString(map) << std::endl;

    // Convert a general type to std::string
    double doubleValue = 3.14159;
    std::cout << "Double: " << toString(doubleValue) << std::endl;

    // Join multiple arguments into a single command line string
    std::string cmdLine = joinCommandLine("arg1", 42, 3.14, "arg4");
    std::cout << "Command line: " << cmdLine << std::endl;

    // Convert an array to std::string
    std::array<int, 3> arr = {1, 2, 3};
    std::cout << "Array: " << toString(arr) << std::endl;

    // Convert a range to std::string
    std::cout << "Range: " << toStringRange(vec.begin(), vec.end())
              << std::endl;

    // Convert a tuple to std::string
    std::tuple<int, std::string, double> tpl = {1, "tuple", 3.14};
    std::cout << "Tuple: " << toString(tpl) << std::endl;

    // Convert an optional to std::string
    std::optional<int> opt = 42;
    std::cout << "Optional: " << toString(opt) << std::endl;

    // Convert a variant to std::string
    std::variant<int, std::string> var = "variant";
    std::cout << "Variant: " << toString(var) << std::endl;

    return 0;
}