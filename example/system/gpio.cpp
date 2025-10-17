/**
 * @file gpio.cpp
 * @brief Basic example demonstrating fundamental GPIO operations
 *
 * This example provides a gentle introduction to GPIO control using the
 * Atom System module. It covers:
 * - Basic pin configuration and control
 * - Digital input and output operations
 * - Simple value reading and writing
 * - Basic GPIO notifications
 *
 * @warning GPIO operations can damage hardware if used incorrectly
 * @warning Always verify pin assignments and electrical specifications
 * @note Requires appropriate hardware and permissions
 * @note Linux: Requires root privileges or GPIO group membership
 * @author Atom Framework
 * @date 2024
 */

#include "atom/system/hardware/gpio.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace atom::system;

int main() {
    try {
        std::cout << "=== Basic GPIO Operations Example ===" << std::endl;
        std::cout << "Learning fundamental GPIO control\n" << std::endl;

        // Safety warning
        std::cout << "⚠️  WARNING: GPIO operations can damage hardware!"
                  << std::endl;
        std::cout << "   - Verify pin assignments before proceeding"
                  << std::endl;
        std::cout << "   - Use appropriate resistors and protection circuits"
                  << std::endl;
        std::cout << "   - Test on development boards, not production systems\n"
                  << std::endl;

        // Create a GPIO object for a specific pin
        std::string pinNumber = "17";
        GPIO gpio(pinNumber, GPIO::Direction::OUTPUT);
        std::cout << "Created GPIO object for pin " << pinNumber << std::endl;

        // Set the value of the GPIO pin
        gpio.setValue(true);
        std::cout << "GPIO pin " << pinNumber << " set to HIGH" << std::endl;

        // Get the value of the GPIO pin
        bool value = gpio.getValue();
        std::cout << "GPIO pin " << pinNumber
                  << " value: " << (value ? "HIGH" : "LOW") << std::endl;

        // Get the direction of the GPIO pin
        auto direction = gpio.getDirection();
        std::cout << "GPIO pin " << pinNumber << " direction: "
                  << (direction == GPIO::Direction::OUTPUT ? "OUTPUT" : "INPUT")
                  << std::endl;

        // Set up a notification callback for changes on the GPIO pin
        bool callbackSet = gpio.onValueChange([pinNumber](bool newValue) {
            std::cout << "GPIO pin " << pinNumber
                      << " changed to: " << (newValue ? "HIGH" : "LOW")
                      << std::endl;
        });

        if (callbackSet) {
            std::cout << "Set up notification callback for GPIO pin "
                      << pinNumber << std::endl;
        } else {
            std::cout << "Failed to set up notification callback (may not be "
                         "supported)"
                      << std::endl;
        }

        // Demonstrate basic blinking
        std::cout << "\nBlinking LED for 3 seconds..." << std::endl;
        for (int i = 0; i < 6; i++) {
            gpio.setValue(i % 2 == 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Set pin to LOW for cleanup
        gpio.setValue(false);
        std::cout << "GPIO pin set to LOW for cleanup" << std::endl;

        std::cout << "\n=== Basic GPIO Operations Complete ===" << std::endl;
        std::cout << "For advanced GPIO features, see gpio_advanced.cpp!"
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "GPIO Error: " << e.what() << std::endl;
        std::cerr << "\nPossible causes:" << std::endl;
        std::cerr << "- Insufficient permissions (try running as root)"
                  << std::endl;
        std::cerr << "- GPIO pin not available or already in use" << std::endl;
        std::cerr << "- GPIO sysfs interface not available" << std::endl;
        std::cerr << "- Hardware not supported" << std::endl;
        return 1;
    }

    return 0;
}
