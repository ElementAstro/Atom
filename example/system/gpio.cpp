#include "atom/system/gpio.hpp"

#include <iostream>

using namespace atom::system;

int main() {
    // Create a GPIO object for a specific pin
    GPIO gpio("17");

    // Set the value of the GPIO pin
    gpio.setValue(true);
    std::cout << "GPIO pin 17 set to HIGH" << std::endl;

    // Get the value of the GPIO pin
    bool value = gpio.getValue();
    std::cout << "GPIO pin 17 value: " << (value ? "HIGH" : "LOW") << std::endl;

    // Set the direction of the GPIO pin
    gpio.setDirection("out");
    std::cout << "GPIO pin 17 direction set to output" << std::endl;

    // Set up a notification callback for changes on the GPIO pin
    GPIO::notifyOnChange("17", [](bool newValue) {
        std::cout << "GPIO pin 17 changed to: " << (newValue ? "HIGH" : "LOW")
                  << std::endl;
    });

    return 0;
}