#include "atom/type/trackable.hpp"
#include <iostream>

int main() {
    // Create a Trackable object with an initial value
    Trackable<int> trackable(10);
    std::cout << "Initial value: " << trackable.get() << std::endl;

    // Subscribe to value changes
    trackable.subscribe([](const int& oldVal, const int& newVal) {
        std::cout << "Value changed from " << oldVal << " to " << newVal
                  << std::endl;
    });

    // Set a callback for value changes
    trackable.setOnChangeCallback([](const int& newVal) {
        std::cout << "New value: " << newVal << std::endl;
    });

    // Change the value
    trackable = 20;

    // Perform arithmetic operations
    trackable += 5;
    trackable -= 3;
    trackable *= 2;
    trackable /= 4;

    // Check if there are any subscribers
    bool hasSubscribers = trackable.hasSubscribers();
    std::cout << "Has subscribers: " << std::boolalpha << hasSubscribers
              << std::endl;

    // Get the current value
    int currentValue = trackable.get();
    std::cout << "Current value: " << currentValue << std::endl;

    // Get the type name of the stored value
    std::string typeName = trackable.getTypeName();
    std::cout << "Type name: " << typeName << std::endl;

    // Unsubscribe all observers
    trackable.unsubscribeAll();
    trackable = 30;

    // Defer notifications
    {
        auto defer = trackable.deferScoped();
        trackable = 40;
        trackable = 50;
    }  // Notifications are triggered here

    return 0;
}