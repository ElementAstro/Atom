#include "atom/type/qvariant.hpp"

#include <iostream>

using namespace atom::type;

int main() {
    // Create a VariantWrapper with an initial value
    VariantWrapper<int, double, std::string> var1(42);
    std::cout << "var1 type: " << var1.typeName()
              << ", value: " << var1.get<int>() << std::endl;

    // Assign a new value to the VariantWrapper
    var1 = 3.14;
    std::cout << "var1 type: " << var1.typeName()
              << ", value: " << var1.get<double>() << std::endl;

    // Check if the VariantWrapper holds a specific type
    bool isDouble = var1.is<double>();
    std::cout << "var1 holds double: " << std::boolalpha << isDouble
              << std::endl;

    // Print the current value of the VariantWrapper
    var1.print();

    /*
    // Use visit to apply a visitor to the VariantWrapper
    var1.visit([](auto&& value) {
        std::cout << "Visited value: " << value << std::endl;
    });
    */

    // Get the index of the currently held type
    std::size_t index = var1.index();
    std::cout << "var1 index: " << index << std::endl;

    // Try to get the value of a specific type
    auto intValue = var1.tryGet<int>();
    if (intValue) {
        std::cout << "var1 int value: " << *intValue << std::endl;
    } else {
        std::cout << "var1 does not hold an int" << std::endl;
    }

    // Convert the current value to an int
    auto intConversion = var1.toInt();
    if (intConversion) {
        std::cout << "var1 converted to int: " << *intConversion << std::endl;
    } else {
        std::cout << "var1 cannot be converted to int" << std::endl;
    }

    // Convert the current value to a double
    auto doubleConversion = var1.toDouble();
    if (doubleConversion) {
        std::cout << "var1 converted to double: " << *doubleConversion
                  << std::endl;
    } else {
        std::cout << "var1 cannot be converted to double" << std::endl;
    }

    // Convert the current value to a bool
    auto boolConversion = var1.toBool();
    if (boolConversion) {
        std::cout << "var1 converted to bool: " << std::boolalpha
                  << *boolConversion << std::endl;
    } else {
        std::cout << "var1 cannot be converted to bool" << std::endl;
    }

    // Convert the current value to a string
    std::string strValue = var1.toString();
    std::cout << "var1 converted to string: " << strValue << std::endl;

    // Reset the VariantWrapper to hold std::monostate
    var1.reset();
    std::cout << "var1 after reset, has value: " << std::boolalpha
              << var1.hasValue() << std::endl;

    // Create another VariantWrapper and compare for equality
    VariantWrapper<int, double, std::string> var2(3.14);
    bool areEqual = (var1 == var2);
    std::cout << "var1 and var2 are equal: " << std::boolalpha << areEqual
              << std::endl;

    return 0;
}