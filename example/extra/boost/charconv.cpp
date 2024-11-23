#include "atom/extra/boost/charconv.hpp"

#include <iostream>
#include <string>

using namespace atom::extra::boost;

int main() {
    // Integer to string conversion
    try {
        int intValue = 123456;
        std::string intStr = BoostCharConv::intToString(intValue);
        std::cout << "Integer to string: " << intStr << std::endl;

        // Integer to string with options
        FormatOptions intOptions;
        intOptions.thousandsSeparator = ',';
        intOptions.uppercase = true;
        std::string intStrWithOptions = BoostCharConv::intToString(intValue, 10, intOptions);
        std::cout << "Integer to string with options: " << intStrWithOptions << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Integer to string conversion failed: " << e.what() << std::endl;
    }

    // Floating-point to string conversion
    try {
        double floatValue = 12345.6789;
        std::string floatStr = BoostCharConv::floatToString(floatValue);
        std::cout << "Floating-point to string: " << floatStr << std::endl;

        // Floating-point to string with options
        FormatOptions floatOptions;
        floatOptions.format = NumberFormat::SCIENTIFIC;
        floatOptions.precision = 2;
        floatOptions.uppercase = true;
        std::string floatStrWithOptions = BoostCharConv::floatToString(floatValue, floatOptions);
        std::cout << "Floating-point to string with options: " << floatStrWithOptions << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Floating-point to string conversion failed: " << e.what() << std::endl;
    }

    // String to integer conversion
    try {
        std::string intStr = "123456";
        int intValue = BoostCharConv::stringToInt<int>(intStr);
        std::cout << "String to integer: " << intValue << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "String to integer conversion failed: " << e.what() << std::endl;
    }

    // String to floating-point conversion
    try {
        std::string floatStr = "12345.6789";
        double floatValue = BoostCharConv::stringToFloat<double>(floatStr);
        std::cout << "String to floating-point: " << floatValue << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "String to floating-point conversion failed: " << e.what() << std::endl;
    }

    // General toString and fromString conversions
    try {
        int intValue = 123456;
        std::string intStr = BoostCharConv::toString(intValue);
        std::cout << "General toString (int): " << intStr << std::endl;

        double floatValue = 12345.6789;
        std::string floatStr = BoostCharConv::toString(floatValue);
        std::cout << "General toString (float): " << floatStr << std::endl;

        int intValueFromStr = BoostCharConv::fromString<int>(intStr);
        std::cout << "General fromString (int): " << intValueFromStr << std::endl;

        double floatValueFromStr = BoostCharConv::fromString<double>(floatStr);
        std::cout << "General fromString (float): " << floatValueFromStr << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "General conversion failed: " << e.what() << std::endl;
    }

    // Special value to string conversion
    try {
        double nanValue = std::nan("");
        std::string nanStr = BoostCharConv::specialValueToString(nanValue);
        std::cout << "Special value to string (NaN): " << nanStr << std::endl;

        double infValue = std::numeric_limits<double>::infinity();
        std::string infStr = BoostCharConv::specialValueToString(infValue);
        std::cout << "Special value to string (Inf): " << infStr << std::endl;

        double negInfValue = -std::numeric_limits<double>::infinity();
        std::string negInfStr = BoostCharConv::specialValueToString(negInfValue);
        std::cout << "Special value to string (Neg Inf): " << negInfStr << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Special value to string conversion failed: " << e.what() << std::endl;
    }

    return 0;
}