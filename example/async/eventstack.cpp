#include "atom/async/eventstack.hpp"

#include <iostream>
#include <string>

int main() {
    using namespace atom::async;

    // Create an EventStack for strings
    EventStack<std::string> eventStack;

    // Push events onto the stack
    eventStack.pushEvent("Event1");
    eventStack.pushEvent("Event2");
    eventStack.pushEvent("Event3");

    // Pop an event from the stack
    auto poppedEvent = eventStack.popEvent();
    if (poppedEvent) {
        std::cout << "Popped event: " << *poppedEvent << std::endl;
    }

    // Check if the stack is empty
    bool isEmpty = eventStack.isEmpty();
    std::cout << "Is stack empty? " << (isEmpty ? "Yes" : "No") << std::endl;

    // Get the number of events in the stack
    size_t size = eventStack.size();
    std::cout << "Number of events in stack: " << size << std::endl;

    // Peek the top event without removing it
    auto topEvent = eventStack.peekTopEvent();
    if (topEvent) {
        std::cout << "Top event: " << *topEvent << std::endl;
    }

    // Copy the current stack
    EventStack<std::string> copiedStack = eventStack.copyStack();

    // Filter events based on a custom filter function
    eventStack.filterEvents(
        [](const std::string& event) { return event != "Event2"; });
    std::cout << "Events after filtering: ";
    for (size_t i = 0; i < eventStack.size(); ++i) {
        auto event = eventStack.popEvent();
        if (event) {
            std::cout << *event << " ";
        }
    }
    std::cout << std::endl;

    // Serialize the stack into a string
    eventStack.pushEvent("Event4");
    eventStack.pushEvent("Event5");
    std::string serializedStack = eventStack.serializeStack();
    std::cout << "Serialized stack: " << serializedStack << std::endl;

    // Deserialize a string into the stack
    EventStack<std::string> newStack;
    newStack.deserializeStack(serializedStack);
    std::cout << "Deserialized stack: ";
    for (size_t i = 0; i < newStack.size(); ++i) {
        auto event = newStack.popEvent();
        if (event) {
            std::cout << *event << " ";
        }
    }
    std::cout << std::endl;

    // Remove duplicate events from the stack
    newStack.pushEvent("Event4");
    newStack.pushEvent("Event4");
    newStack.removeDuplicates();
    std::cout << "Stack after removing duplicates: ";
    for (size_t i = 0; i < newStack.size(); ++i) {
        auto event = newStack.popEvent();
        if (event) {
            std::cout << *event << " ";
        }
    }
    std::cout << std::endl;

    // Sort the events in the stack
    newStack.pushEvent("Event6");
    newStack.pushEvent("Event5");
    newStack.sortEvents(
        [](const std::string& a, const std::string& b) { return a < b; });
    std::cout << "Sorted stack: ";
    for (size_t i = 0; i < newStack.size(); ++i) {
        auto event = newStack.popEvent();
        if (event) {
            std::cout << *event << " ";
        }
    }
    std::cout << std::endl;

    // Reverse the order of events in the stack
    newStack.pushEvent("Event7");
    newStack.pushEvent("Event8");
    newStack.reverseEvents();
    std::cout << "Reversed stack: ";
    for (size_t i = 0; i < newStack.size(); ++i) {
        auto event = newStack.popEvent();
        if (event) {
            std::cout << *event << " ";
        }
    }
    std::cout << std::endl;

    // Count the number of events that satisfy a predicate
    newStack.pushEvent("Event9");
    newStack.pushEvent("Event10");
    size_t count = newStack.countEvents([](const std::string& event) {
        return event.find("Event") != std::string::npos;
    });
    std::cout << "Number of events containing 'Event': " << count << std::endl;

    // Find the first event that satisfies a predicate
    auto foundEvent = newStack.findEvent(
        [](const std::string& event) { return event == "Event9"; });
    if (foundEvent) {
        std::cout << "Found event: " << *foundEvent << std::endl;
    }

    // Check if any event in the stack satisfies a predicate
    bool anyEvent = newStack.anyEvent(
        [](const std::string& event) { return event == "Event10"; });
    std::cout << "Is there any event 'Event10'? " << (anyEvent ? "Yes" : "No")
              << std::endl;

    // Check if all events in the stack satisfy a predicate
    bool allEvents = newStack.allEvents([](const std::string& event) {
        return event.find("Event") != std::string::npos;
    });
    std::cout << "Do all events contain 'Event'? " << (allEvents ? "Yes" : "No")
              << std::endl;

    // Clear all events from the stack
    newStack.clearEvents();
    std::cout << "Stack cleared. Is stack empty? "
              << (newStack.isEmpty() ? "Yes" : "No") << std::endl;

    return 0;
}