/**
 * @file gpio_advanced.cpp
 * @brief Advanced example demonstrating comprehensive GPIO capabilities
 *
 * This example showcases advanced GPIO functionality including:
 * - PWM (Pulse Width Modulation) control
 * - GPIO interrupts and callbacks
 * - GPIO groups and batch operations
 * - Button debouncing
 * - Interrupt counting
 * - Edge detection and monitoring
 * - Advanced pin configuration
 *
 * @warning GPIO operations can damage hardware if used incorrectly
 * @warning PWM operations require careful electrical design
 * @warning Always verify pin assignments and electrical specifications
 * @note Requires appropriate hardware and permissions
 * @note Linux: Requires root privileges or GPIO group membership
 * @author Atom Framework
 * @date 2024
 */

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>
#include "atom/system/gpio.hpp"

using namespace atom::system;

// Global variables for interrupt handling
std::atomic<int> interruptCount{0};
std::atomic<bool> buttonPressed{false};

/**
 * @brief Callback function for GPIO value changes
 */
void gpioChangeCallback(bool newValue) {
    std::cout << "[CALLBACK] GPIO value changed to: "
              << (newValue ? "HIGH" : "LOW") << " at "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now().time_since_epoch())
                     .count()
              << "ms" << std::endl;
}

/**
 * @brief Callback function for button press detection
 */
void buttonPressCallback() {
    buttonPressed = true;
    std::cout << "[BUTTON] Button press detected!" << std::endl;
}

/**
 * @brief Demonstrate PWM functionality
 */
void demonstratePWM(GPIO& gpio) {
    std::cout << "\n=== PWM Demonstration ===" << std::endl;

    try {
        // Start with 50% duty cycle at 1000 Hz
        double frequency = 1000.0;  // 1 kHz
        double dutyCycle = 0.5;     // 50%

        std::cout << "Starting PWM: " << frequency << " Hz, "
                  << (dutyCycle * 100) << "% duty cycle" << std::endl;

        bool pwmStarted =
            gpio.setPwm(frequency, dutyCycle, GPIO::PwmMode::SOFTWARE);
        if (!pwmStarted) {
            std::cout << "Failed to start PWM (hardware may not support it)"
                      << std::endl;
            return;
        }

        std::cout << "PWM started successfully" << std::endl;

        // Gradually change duty cycle from 0% to 100%
        std::cout << "Sweeping duty cycle from 0% to 100%..." << std::endl;
        for (int i = 0; i <= 100; i += 10) {
            dutyCycle = i / 100.0;
            gpio.updatePwmDutyCycle(dutyCycle);
            std::cout << "Duty cycle: " << std::setw(3) << i << "%"
                      << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Sweep back down
        std::cout << "Sweeping duty cycle from 100% to 0%..." << std::endl;
        for (int i = 100; i >= 0; i -= 10) {
            dutyCycle = i / 100.0;
            gpio.updatePwmDutyCycle(dutyCycle);
            std::cout << "Duty cycle: " << std::setw(3) << i << "%"
                      << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Stop PWM
        gpio.stopPwm();
        std::cout << "PWM stopped" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "PWM error: " << e.what() << std::endl;
    }
}

/**
 * @brief Demonstrate GPIO groups
 */
void demonstrateGPIOGroups() {
    std::cout << "\n=== GPIO Groups Demonstration ===" << std::endl;

    try {
        // Create a group of GPIO pins
        std::vector<std::string> pins = {"20", "21", "22", "23"};
        GPIO::GPIOGroup group(pins);

        std::cout << "Created GPIO group with pins: ";
        for (const auto& pin : pins) {
            std::cout << pin << " ";
        }
        std::cout << std::endl;

        // Set all pins as outputs
        group.setDirection(GPIO::Direction::OUTPUT);
        std::cout << "Set all pins as OUTPUT" << std::endl;

        // Test different patterns
        std::vector<std::vector<bool>> patterns = {
            {true, false, true, false},   // Alternating pattern
            {false, true, false, true},   // Reverse alternating
            {true, true, false, false},   // Two on, two off
            {false, false, true, true},   // Reverse two on, two off
            {true, true, true, true},     // All on
            {false, false, false, false}  // All off
        };

        std::cout << "Testing different patterns..." << std::endl;
        for (size_t i = 0; i < patterns.size(); i++) {
            std::cout << "Pattern " << (i + 1) << ": ";
            for (bool value : patterns[i]) {
                std::cout << (value ? "1" : "0") << " ";
            }
            std::cout << std::endl;

            group.setValues(patterns[i]);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        // Read back values
        auto values = group.getValues();
        std::cout << "Final values: ";
        for (bool value : values) {
            std::cout << (value ? "1" : "0") << " ";
        }
        std::cout << std::endl;

    } catch (const std::exception& e) {
        std::cout << "GPIO group error: " << e.what() << std::endl;
        std::cout << "This is normal if the specified pins are not available"
                  << std::endl;
    }
}

int main() {
    try {
        std::cout << "=== Advanced GPIO Operations Example ===" << std::endl;
        std::cout << "Demonstrating comprehensive GPIO capabilities\n"
                  << std::endl;

        // Enhanced safety warning
        std::cout << "⚠️  CRITICAL SAFETY WARNING ⚠️" << std::endl;
        std::cout << "Advanced GPIO operations can damage hardware!"
                  << std::endl;
        std::cout << "- Verify ALL pin assignments before proceeding"
                  << std::endl;
        std::cout << "- Use appropriate resistors and protection circuits"
                  << std::endl;
        std::cout << "- PWM operations require careful electrical design"
                  << std::endl;
        std::cout << "- Test on development boards, not production systems"
                  << std::endl;
        std::cout << "- Understand electrical specifications of your hardware"
                  << std::endl;
        std::cout << "- Ensure proper grounding and power supply" << std::endl;
        std::cout << "- Have emergency shutdown procedures ready\n"
                  << std::endl;

        // 1. Advanced GPIO Configuration
        std::cout << "[1. Advanced GPIO Configuration]" << std::endl;

        std::string outputPin = "18";
        GPIO outputGpio(outputPin, GPIO::Direction::OUTPUT, false);
        std::cout << "Created output GPIO on pin " << outputPin << std::endl;

        // 2. PWM Demonstration
        demonstratePWM(outputGpio);

        // 3. GPIO Interrupts and Callbacks
        std::cout << "\n[3. GPIO Interrupts and Callbacks]" << std::endl;

        try {
            std::string inputPin = "17";
            GPIO inputGpio(inputPin, GPIO::Direction::INPUT);
            std::cout << "Created input GPIO on pin " << inputPin << std::endl;

            // Set up edge detection
            inputGpio.setEdge(GPIO::Edge::BOTH);
            std::cout << "Set edge detection to BOTH (rising and falling)"
                      << std::endl;

            // Set up value change callback
            bool callbackSet = inputGpio.onValueChange(gpioChangeCallback);
            if (callbackSet) {
                std::cout << "Value change callback registered" << std::endl;

                // Monitor for changes for 5 seconds
                std::cout << "Monitoring for value changes for 5 seconds..."
                          << std::endl;
                std::cout << "(Try connecting/disconnecting the pin to ground)"
                          << std::endl;

                auto startTime = std::chrono::steady_clock::now();
                auto endTime = startTime + std::chrono::seconds(5);

                while (std::chrono::steady_clock::now() < endTime) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                std::cout << "Monitoring complete" << std::endl;
            } else {
                std::cout << "Failed to register value change callback"
                          << std::endl;
            }

        } catch (const std::exception& e) {
            std::cout << "Input GPIO error: " << e.what() << std::endl;
        }

        // 4. Button Debouncing
        std::cout << "\n[4. Button Debouncing]" << std::endl;

        try {
            std::string buttonPin = "16";
            GPIO buttonGpio(buttonPin, GPIO::Direction::INPUT);
            std::cout << "Created button GPIO on pin " << buttonPin
                      << std::endl;

            // Set up button debouncing
            unsigned int debounceTime = 50;  // 50ms debounce
            bool debounceSet = buttonGpio.setupButtonDebounce(
                buttonPressCallback, debounceTime);

            if (debounceSet) {
                std::cout << "Button debouncing set up with " << debounceTime
                          << "ms debounce time" << std::endl;
                std::cout << "Monitoring for button presses for 5 seconds..."
                          << std::endl;
                std::cout << "(Try pressing a button connected to the pin)"
                          << std::endl;

                buttonPressed = false;
                auto startTime = std::chrono::steady_clock::now();
                auto endTime = startTime + std::chrono::seconds(5);

                while (std::chrono::steady_clock::now() < endTime) {
                    if (buttonPressed) {
                        std::cout << "Button press detected and processed!"
                                  << std::endl;
                        buttonPressed = false;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                std::cout << "Button monitoring complete" << std::endl;
            } else {
                std::cout << "Failed to set up button debouncing" << std::endl;
            }

        } catch (const std::exception& e) {
            std::cout << "Button GPIO error: " << e.what() << std::endl;
        }

        // 5. GPIO Groups
        demonstrateGPIOGroups();

        // 6. Advanced Pin Operations
        std::cout << "\n[6. Advanced Pin Operations]" << std::endl;

        // Toggle operation
        std::cout << "Testing toggle operation..." << std::endl;
        for (int i = 0; i < 5; i++) {
            bool newValue = outputGpio.toggle();
            std::cout << "Toggle " << (i + 1) << ": "
                      << (newValue ? "HIGH" : "LOW") << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }

        // Pulse operation
        std::cout << "Testing pulse operation..." << std::endl;
        outputGpio.setValue(false);  // Start LOW
        std::cout << "Sending HIGH pulse for 1 second..." << std::endl;
        outputGpio.pulse(true, std::chrono::milliseconds(1000));
        std::cout << "Pulse complete, pin should be back to LOW" << std::endl;

        // 7. Cleanup
        std::cout << "\n[7. Cleanup]" << std::endl;
        outputGpio.setValue(false);
        std::cout << "Set all output pins to safe state (LOW)" << std::endl;

        std::cout << "\n=== Advanced GPIO Operations Complete ===" << std::endl;
        std::cout << "This example demonstrated:" << std::endl;
        std::cout << "- PWM (Pulse Width Modulation) control" << std::endl;
        std::cout << "- GPIO interrupts and callbacks" << std::endl;
        std::cout << "- GPIO groups and batch operations" << std::endl;
        std::cout << "- Button debouncing" << std::endl;
        std::cout << "- Edge detection and monitoring" << std::endl;
        std::cout << "- Advanced pin operations (toggle, pulse)" << std::endl;
        std::cout << "- Comprehensive error handling" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Advanced GPIO Error: " << e.what() << std::endl;
        std::cerr << "\nPossible causes:" << std::endl;
        std::cerr << "- Insufficient permissions (try running as root)"
                  << std::endl;
        std::cerr << "- GPIO pins not available or already in use" << std::endl;
        std::cerr << "- Hardware does not support advanced features"
                  << std::endl;
        std::cerr << "- GPIO sysfs interface not available" << std::endl;
        std::cerr << "- Electrical configuration issues" << std::endl;
        return 1;
    }

    return 0;
}
