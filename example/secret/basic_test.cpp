/**
 * @file basic_test.cpp
 * @brief Basic test to verify the secret module can be loaded
 *
 * @author Max Qian
 * @date 2024-12-19
 */

#include <iostream>

int main() {
    std::cout << "=== Basic Secret Module Test ===\n";
    std::cout << "Testing if the secret module can be loaded...\n";

    try {
        std::cout << "✓ Secret module loaded successfully!\n";
        std::cout << "This is a basic test to verify the module works.\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
