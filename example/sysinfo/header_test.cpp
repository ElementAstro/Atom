/**
 * @file header_test.cpp
 * @brief Basic test to verify sysinfo headers can be included
 *
 * @author Max Qian
 * @date 2024-12-19
 */

#include <iostream>

// Test including sysinfo headers
#include "atom/sysinfo/os.hpp"

int main() {
    std::cout << "=== Sysinfo Header Test ===\n";
    std::cout << "Testing if sysinfo headers can be included...\n";

    try {
        std::cout << "✓ Sysinfo headers included successfully!\n";
        std::cout << "This is a basic test to verify the headers work.\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
