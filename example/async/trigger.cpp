#include "atom/async/trigger.hpp"

#include <iostream>
#include <thread>

using namespace atom::async;

// Example callback function
void exampleCallback(int value) {
    std::cout << "Callback called with value: " << value << std::endl;
}

// Example callback function with high priority
void highPriorityCallback(int value) {
    std::cout << "High priority callback called with value: " << value
              << std::endl;
}

int main() {
    // Create a Trigger object
    Trigger<int> trigger;

    // Register callbacks for an event
    trigger.registerCallback("exampleEvent", exampleCallback);
    trigger.registerCallback("exampleEvent", highPriorityCallback,
                             Trigger<int>::CallbackPriority::High);

    // Trigger the event
    trigger.trigger("exampleEvent", 42);

    // Schedule a trigger for the event after a delay
    trigger.scheduleTrigger("exampleEvent", 84,
                            std::chrono::milliseconds(1000));

    // Schedule an asynchronous trigger for the event
    auto future = trigger.scheduleAsyncTrigger("exampleEvent", 126);
    future.get();  // Wait for the asynchronous trigger to complete

    // Unregister a callback
    trigger.unregisterCallback("exampleEvent", exampleCallback);

    // Trigger the event again to show that the callback has been unregistered
    trigger.trigger("exampleEvent", 168);

    // Cancel the scheduled trigger for the event
    trigger.cancelTrigger("exampleEvent");

    // Register another callback and cancel all triggers
    trigger.registerCallback("exampleEvent", exampleCallback);
    trigger.cancelAllTriggers();

    // Trigger the event to show that all triggers have been cancelled
    trigger.trigger("exampleEvent", 210);

    return 0;
}