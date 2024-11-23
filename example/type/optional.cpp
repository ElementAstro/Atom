#include "atom/type/optional.hpp"

#include <iostream>
#include <string>

using namespace atom::type;

int main() {
    // Create an empty Optional object
    Optional<int> opt1;
    std::cout << "opt1 has value: " << std::boolalpha << static_cast<bool>(opt1)
              << std::endl;

    // Create an Optional object with a value
    Optional<int> opt2(42);
    std::cout << "opt2 has value: " << std::boolalpha << static_cast<bool>(opt2)
              << std::endl;
    std::cout << "opt2 value: " << *opt2 << std::endl;

    // Reset the Optional object to an empty state
    opt2.reset();
    std::cout << "After reset, opt2 has value: " << std::boolalpha
              << static_cast<bool>(opt2) << std::endl;

    // Assign a new value to the Optional object
    opt2 = 100;
    std::cout << "After assignment, opt2 value: " << *opt2 << std::endl;

    // Create an Optional object with an rvalue
    Optional<std::string> opt3(std::string("Hello"));
    std::cout << "opt3 value: " << *opt3 << std::endl;

    // Use emplace to construct a new value in the Optional object
    opt3.emplace("World");
    std::cout << "After emplace, opt3 value: " << *opt3 << std::endl;

    // Use value_or to get the contained value or a default value
    std::cout << "opt3 value or default: " << opt3.value_or("Default")
              << std::endl;
    opt3.reset();
    std::cout << "opt3 value or default after reset: "
              << opt3.value_or("Default") << std::endl;

    // Use map to transform the contained value
    Optional<int> opt4(10);
    auto opt5 = opt4.map([](int x) { return x * 2; });
    std::cout << "opt5 value after map: " << *opt5 << std::endl;

    // Use and_then to apply a function to the contained value
    auto opt6 = opt4.and_then(
        [](int x) { return Optional<std::string>(std::to_string(x)); });
    std::cout << "opt6 value after and_then: " << *opt6 << std::endl;

    // Use or_else to get the contained value or invoke a function to generate a
    // default value
    std::cout << "opt4 value or else: " << opt4.or_else([]() { return 20; })
              << std::endl;
    opt4.reset();
    std::cout << "opt4 value or else after reset: "
              << opt4.or_else([]() { return 20; }) << std::endl;

    // Use transform_or to transform the contained value or return a default
    // value
    opt4 = 30;
    auto opt7 = opt4.transform_or([](int x) { return x + 10; }, 50);
    std::cout << "opt7 value after transform_or: " << *opt7 << std::endl;
    opt4.reset();
    auto opt8 = opt4.transform_or([](int x) { return x + 10; }, 50);
    std::cout << "opt8 value after transform_or: " << *opt8 << std::endl;

    // Use flat_map to transform the contained value and return the result
    auto opt9 = opt4.flat_map(
        [](int x) { return Optional<std::string>(std::to_string(x)); });
    std::cout << "opt9 has value: " << std::boolalpha << static_cast<bool>(opt9)
              << std::endl;

    return 0;
}