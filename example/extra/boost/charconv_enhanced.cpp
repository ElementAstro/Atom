#include "atom/extra/boost/charconv.hpp"

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <limits>
#include <numeric>

using namespace atom::extra::boost;

void demonstrateBasicConversions() {
    std::cout << "=== Basic Conversions ===" << std::endl;

    // Integer conversions
    int value = 123456;
    std::cout << "Integer " << value << " to string: " << BoostCharConv::intToString(value) << std::endl;

    // Float conversions
    double pi = 3.14159265359;
    std::cout << "Pi to string: " << BoostCharConv::floatToString(pi) << std::endl;

    // String to number conversions
    std::string numStr = "987654";
    auto parsed = BoostCharConv::stringToInt<int>(numStr);
    std::cout << "String '" << numStr << "' to int: " << parsed << std::endl;
}

void demonstrateAdvancedFormatting() {
    std::cout << "\n=== Advanced Formatting ===" << std::endl;

    FormatOptions options;

    // Thousands separator
    options.thousandsSeparator = ',';
    options.useGrouping = true;
    int largeNumber = 1234567890;
    std::cout << "Large number with thousands separator: "
              << BoostCharConv::intToString(largeNumber, 10, options) << std::endl;

    // Uppercase hex
    options = {}; // Reset
    options.uppercase = true;
    std::cout << "Hex (uppercase): "
              << BoostCharConv::intToString(255, 16, options) << std::endl;

    // Positive sign and padding
    options = {}; // Reset
    options.showPositiveSign = true;
    options.padWithZeros = true;
    options.minimumWidth = 8;
    std::cout << "Positive number with padding: "
              << BoostCharConv::intToString(42, 10, options) << std::endl;

    // Scientific notation with precision
    options = {}; // Reset
    options.format = NumberFormat::SCIENTIFIC;
    options.precision = 3;
    double scientificNum = 0.000123456;
    std::cout << "Scientific notation: "
              << BoostCharConv::floatToString(scientificNum, options) << std::endl;
}

void demonstrateBatchOperations() {
    std::cout << "\n=== Batch Operations ===" << std::endl;

    // Create test data
    std::vector<int> integers = {1, 22, 333, 4444, 55555};
    std::vector<float> floats = {1.1f, 2.22f, 3.333f, 4.4444f, 5.55555f};

    // Manual batch integer conversion (avoiding parallel execution for now)
    std::vector<std::string> intStrings;
    for (const auto& val : integers) {
        intStrings.emplace_back(BoostCharConv::intToString(val));
    }
    std::cout << "Batch integer conversion: ";
    for (const auto& str : intStrings) {
        std::cout << str << " ";
    }
    std::cout << std::endl;

    // Manual batch float conversion
    FormatOptions floatOptions;
    floatOptions.precision = 2;
    std::vector<std::string> floatStrings;
    for (const auto& val : floats) {
        floatStrings.emplace_back(BoostCharConv::floatToString(val, floatOptions));
    }
    std::cout << "Batch float conversion: ";
    for (const auto& str : floatStrings) {
        std::cout << str << " ";
    }
    std::cout << std::endl;
}

void demonstrateSafeConversions() {
    std::cout << "\n=== Safe Conversions ===" << std::endl;

    // Valid conversion
    auto result1 = BoostCharConv::tryStringToInt<int>("12345");
    if (result1) {
        std::cout << "Valid conversion: " << *result1 << std::endl;
    }

    // Invalid conversion
    auto result2 = BoostCharConv::tryStringToInt<int>("not_a_number");
    if (!result2) {
        std::cout << "Invalid conversion detected safely" << std::endl;
    }

    // Conversion with locale-specific formatting
    auto result3 = BoostCharConv::tryStringToInt<int>("1,234,567");
    if (result3) {
        std::cout << "Locale-aware conversion: " << *result3 << std::endl;
    }
}

void demonstrateSpecialValues() {
    std::cout << "\n=== Special Values ===" << std::endl;

    // NaN and infinity
    double nan_val = std::numeric_limits<double>::quiet_NaN();
    double inf_val = std::numeric_limits<double>::infinity();
    double neg_inf_val = -std::numeric_limits<double>::infinity();

    std::cout << "NaN: " << BoostCharConv::floatToString(nan_val) << std::endl;
    std::cout << "Infinity: " << BoostCharConv::floatToString(inf_val) << std::endl;
    std::cout << "Negative Infinity: " << BoostCharConv::floatToString(neg_inf_val) << std::endl;

    // Boolean conversions
    std::cout << "Boolean true: " << BoostCharConv::boolToString(true) << std::endl;
    std::cout << "Boolean false: " << BoostCharConv::boolToString(false) << std::endl;

    // String to boolean
    try {
        bool val1 = BoostCharConv::stringToBool("true");
        bool val2 = BoostCharConv::stringToBool("1");
        bool val3 = BoostCharConv::stringToBool("false");
        std::cout << "String to bool conversions: " << val1 << ", " << val2 << ", " << val3 << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Boolean conversion error: " << e.what() << std::endl;
    }
}

void performanceTest() {
    std::cout << "\n=== Performance Test ===" << std::endl;

    const size_t test_size = 10000;  // Reduced size for simple test
    std::vector<int> test_data(test_size);
    std::iota(test_data.begin(), test_data.end(), 1);

    // Test sequential conversion performance
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::string> results;
    results.reserve(test_size);
    for (const auto& val : test_data) {
        results.emplace_back(BoostCharConv::intToString(val));
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Sequential converted " << test_size << " integers in "
              << duration.count() << " microseconds" << std::endl;
    std::cout << "Average: " << static_cast<double>(duration.count()) / test_size
              << " microseconds per conversion" << std::endl;
}

int main() {
    try {
        demonstrateBasicConversions();
        demonstrateAdvancedFormatting();
        demonstrateBatchOperations();
        demonstrateSafeConversions();
        demonstrateSpecialValues();
        performanceTest();

        std::cout << "\n=== All tests completed successfully! ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
