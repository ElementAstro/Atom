/**
 * @file hex_utils.cpp
 * @brief Example demonstrating hexadecimal conversion utilities
 *
 * This example shows how to:
 * - Convert hex characters to nibble values
 * - Convert nibble values to hex characters
 * - Validate hex characters
 * - Perform round-trip conversions
 *
 * @author Atom Framework
 * @date 2024
 */

#include "atom/algorithm/core/hex_utils.hpp"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace atom::algorithm;

void printHeader(const std::string& title) {
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(50, '=') << "\n";
}

void demonstrateHexToNibble() {
    printHeader("hexToNibble Examples");

    std::cout << "Converting hex characters to nibble values:\n";

    // Demonstrate digit conversion
    std::cout << "\nDigits 0-9:\n";
    for (char c = '0'; c <= '9'; ++c) {
        auto result = hexToNibble(c);
        if (result.has_value()) {
            std::cout << "  '" << c << "' -> " << static_cast<int>(result.value()) << "\n";
        }
    }

    // Demonstrate uppercase letter conversion
    std::cout << "\nUppercase A-F:\n";
    for (char c = 'A'; c <= 'F'; ++c) {
        auto result = hexToNibble(c);
        if (result.has_value()) {
            std::cout << "  '" << c << "' -> " << static_cast<int>(result.value()) << "\n";
        }
    }

    // Demonstrate lowercase letter conversion
    std::cout << "\nLowercase a-f:\n";
    for (char c = 'a'; c <= 'f'; ++c) {
        auto result = hexToNibble(c);
        if (result.has_value()) {
            std::cout << "  '" << c << "' -> " << static_cast<int>(result.value()) << "\n";
        }
    }

    // Demonstrate error handling
    std::cout << "\nInvalid characters:\n";
    for (char c : {'G', 'z', ' ', '-'}) {
        auto result = hexToNibble(c);
        std::cout << "  '" << c << "' -> "
                  << (result.has_value() ? std::to_string(result.value()) : "error")
                  << "\n";
    }
}

void demonstrateNibbleToHex() {
    printHeader("nibbleToHex Examples");

    std::cout << "Converting nibble values to hex characters:\n";

    std::cout << "\nUppercase output:\n";
    for (u8 i = 0; i < 16; ++i) {
        std::cout << "  " << static_cast<int>(i) << " -> '" << nibbleToHex(i, true) << "'\n";
    }

    std::cout << "\nLowercase output:\n";
    for (u8 i = 0; i < 16; ++i) {
        std::cout << "  " << static_cast<int>(i) << " -> '" << nibbleToHex(i, false) << "'\n";
    }
}

void demonstrateIsHexDigit() {
    printHeader("isHexDigit Examples");

    std::cout << "Checking if characters are valid hex digits:\n\n";

    std::string testChars = "0123456789ABCDEFabcdefGHIJghij!@#";
    for (char c : testChars) {
        std::cout << "  '" << c << "' is " << (isHexDigit(c) ? "valid" : "invalid") << "\n";
    }
}

void demonstrateByteConversion() {
    printHeader("Byte to Hex String Conversion");

    std::cout << "Converting bytes to hex strings:\n\n";

    std::vector<u8> data = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF};

    std::cout << "Data bytes: ";
    for (u8 byte : data) {
        std::cout << static_cast<int>(byte) << " ";
    }
    std::cout << "\n";

    std::cout << "Hex string (uppercase): ";
    for (u8 byte : data) {
        std::cout << nibbleToHex((byte >> 4) & 0x0F, true)
                  << nibbleToHex(byte & 0x0F, true);
    }
    std::cout << "\n";

    std::cout << "Hex string (lowercase): ";
    for (u8 byte : data) {
        std::cout << nibbleToHex((byte >> 4) & 0x0F, false)
                  << nibbleToHex(byte & 0x0F, false);
    }
    std::cout << "\n";
}

int main() {
    std::cout << "Atom Algorithm - Hex Utilities Example\n";

    try {
        demonstrateHexToNibble();
        demonstrateNibbleToHex();
        demonstrateIsHexDigit();
        demonstrateByteConversion();

        std::cout << "\n" << std::string(50, '=') << "\n";
        std::cout << "All examples completed successfully!\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
