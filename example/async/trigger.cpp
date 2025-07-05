#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// Uncomment below to use Boost locks functionality
// #define ATOM_USE_BOOST_LOCKS

// Uncomment below to use Boost lockfree queue functionality
// #define ATOM_USE_BOOST_LOCKFREE

#include "atom/async/trigger.hpp"

// Helper macros for formatted output
#define SECTION(name) std::cout << "\n=== " << name << " ===\n"
#define LOG(msg) std::cout << "[" << __LINE__ << "] " << msg << std::endl

// Simple event data structure
struct EventData {
    std::string message;
    int value;

    // Support copy construction and assignment
    EventData(const std::string& msg = "", int val = 0)
        : message(msg), value(val) {}
};

// Print event data
void printEventData(const EventData& data) {
    std::cout << "  Message: \"" << data.message << "\", Value: " << data.value
              << std::endl;
}

int main() {
    using namespace std::chrono_literals;

    std::cout << "===== atom::async::Trigger Usage Examples =====\n\n";

    //==============================================================
    // 1. Basic Usage
    //==============================================================
    SECTION("1. Basic Usage");
    {
        // Create a trigger using EventData type parameter
        atom::async::Trigger<EventData> trigger;

        // Register a basic callback function
        auto callbackId =
            trigger.registerCallback("basic_event", [](const EventData& data) {
                std::cout << "Received basic event: ";
                printEventData(data);
            });

        LOG("Registered callback ID: " + std::to_string(callbackId));

        // Trigger the event
        EventData basicData{"This is a basic event", 42};
        size_t count = trigger.trigger("basic_event", basicData);
        LOG("Number of triggered callbacks: " + std::to_string(count));

        // Check callback count
        LOG("Event has callbacks: " +
            std::string(trigger.hasCallbacks("basic_event") ? "Yes" : "No"));
        LOG("Callback count: " +
            std::to_string(trigger.callbackCount("basic_event")));

        // Unregister callback
        bool unregistered =
            trigger.unregisterCallback("basic_event", callbackId);
        LOG("Unregistration successful: " +
            std::string(unregistered ? "Yes" : "No"));
        LOG("Callback count after unregistration: " +
            std::to_string(trigger.callbackCount("basic_event")));

        // Trigger again, no callbacks should be executed
        count = trigger.trigger("basic_event", basicData);
        LOG("Number of callbacks triggered again: " + std::to_string(count));
    }

    //==============================================================
    // 2. Callbacks with Priority
    //==============================================================
    SECTION("2. Callbacks with Priority");
    {
        atom::async::Trigger<EventData> trigger;

        // Register callbacks with different priorities
        [[maybe_unused]] auto lowId = trigger.registerCallback(
            "priority_event",
            [](const EventData& data) {
                std::cout << "  Low priority callback: ";
                printEventData(data);
            },
            atom::async::Trigger<EventData>::CallbackPriority::Low);

        [[maybe_unused]] auto normalId = trigger.registerCallback(
            "priority_event",
            [](const EventData& data) {
                std::cout << "  Normal priority callback: ";
                printEventData(data);
            },
            atom::async::Trigger<EventData>::CallbackPriority::Normal);

        [[maybe_unused]] auto highId = trigger.registerCallback(
            "priority_event",
            [](const EventData& data) {
                std::cout << "  High priority callback: ";
                printEventData(data);
            },
            atom::async::Trigger<EventData>::CallbackPriority::High);

        // Trigger event, callbacks should be executed in priority order (high
        // -> normal -> low)
        LOG("Triggering callbacks with different priorities:");
        EventData data{"Priority test", 100};
        trigger.trigger("priority_event", data);
    }

    //==============================================================
    // 3. Delayed Triggering
    //==============================================================
    SECTION("3. Delayed Triggering");
    {
        atom::async::Trigger<EventData> trigger;

        // Register callback
        [[maybe_unused]] auto callbackId = trigger.registerCallback(
            "delayed_event", [](const EventData& data) {
                std::cout << "  Received delayed event: ";
                printEventData(data);
                std::cout << "  Receive time: "
                          << std::chrono::system_clock::now()
                                 .time_since_epoch()
                                 .count()
                          << std::endl;
            });

        EventData delayData{"Delayed by 500 milliseconds", 500};

        // Record current time
        auto now = std::chrono::system_clock::now();
        std::cout << "  Trigger time: " << now.time_since_epoch().count()
                  << std::endl;

        // Schedule to trigger after 500ms
        auto cancelFlag =
            trigger.scheduleTrigger("delayed_event", delayData, 500ms);
        LOG("Scheduled delayed trigger, waiting...");

        // Wait for trigger to complete
        std::this_thread::sleep_for(600ms);

        // Schedule again, but cancel immediately this time
        LOG("Scheduling another delayed trigger, but canceling immediately");
        auto cancelFlag2 = trigger.scheduleTrigger(
            "delayed_event", EventData{"This should not be triggered", 999},
            300ms);
        *cancelFlag2 = true;  // Directly set cancel flag

        // Ensure it wasn't triggered
        std::this_thread::sleep_for(400ms);
        LOG("Canceled trigger should not have executed");
    }

    //==============================================================
    // 4. Asynchronous Triggering
    //==============================================================
    SECTION("4. Asynchronous Triggering");
    {
        atom::async::Trigger<EventData> trigger;

        // Register several callbacks
        for (int i = 1; i <= 3; ++i) {
            [[maybe_unused]] auto callbackId = trigger.registerCallback(
                "async_event", [i](const EventData& data) {
                    std::cout << "  Async callback #" << i << ": ";
                    printEventData(data);
                });
        }

        // Trigger asynchronously
        LOG("Starting async trigger");
        auto future = trigger.scheduleAsyncTrigger(
            "async_event", EventData{"Async execution", 42});

        // Get result
        auto count = future.get();  // Wait for async operation to complete
        LOG("Async trigger completed, number of executed callbacks: " +
            std::to_string(count));
    }

    //==============================================================
    // 5. Canceling Triggers
    //==============================================================
    SECTION("5. Canceling Triggers");
    {
        atom::async::Trigger<EventData> trigger;

        // Register callback
        [[maybe_unused]] auto callbackId =
            trigger.registerCallback("cancel_event", [](const EventData& data) {
                std::cout << "  Cancel event callback: ";
                printEventData(data);
            });

        // Schedule multiple delayed triggers
        LOG("Scheduling multiple delayed triggers");
        auto flag1 = trigger.scheduleTrigger("cancel_event",
                                             EventData{"Delay 1", 1}, 500ms);
        auto flag2 = trigger.scheduleTrigger("cancel_event",
                                             EventData{"Delay 2", 2}, 700ms);
        auto flag3 = trigger.scheduleTrigger("cancel_event",
                                             EventData{"Delay 3", 3}, 900ms);

        // Cancel all triggers for specific event
        size_t canceled = trigger.cancelTrigger("cancel_event");
        LOG("Number of canceled triggers: " + std::to_string(canceled));

        // Wait long enough to ensure triggers won't execute
        std::this_thread::sleep_for(1000ms);
        LOG("After waiting, all triggers should be canceled");

        // Schedule another set of triggers
        LOG("Scheduling another set of triggers");
        auto eventFlag1 =
            trigger.scheduleTrigger("event1", EventData{"Event 1", 1}, 300ms);
        auto eventFlag2 =
            trigger.scheduleTrigger("event2", EventData{"Event 2", 2}, 300ms);

        // Cancel all triggers
        canceled = trigger.cancelAllTriggers();
        LOG("Canceled all triggers, count: " + std::to_string(canceled));

        // Wait to ensure no triggers execute
        std::this_thread::sleep_for(500ms);
    }

    //==============================================================
    // 6. Multi-Event Triggering
    //==============================================================
    SECTION("6. Multi-Event Triggering");
    {
        atom::async::Trigger<EventData> trigger;

        // Register callbacks for multiple events
        [[maybe_unused]] auto callbackA =
            trigger.registerCallback("event_a", [](const EventData& data) {
                std::cout << "  Event A callback: ";
                printEventData(data);
            });

        [[maybe_unused]] auto callbackB =
            trigger.registerCallback("event_b", [](const EventData& data) {
                std::cout << "  Event B callback: ";
                printEventData(data);
            });

        [[maybe_unused]] auto callbackC =
            trigger.registerCallback("event_c", [](const EventData& data) {
                std::cout << "  Event C callback: ";
                printEventData(data);
            });

        // Trigger multiple different events
        LOG("Triggering multiple different events");
        trigger.trigger("event_a", EventData{"From Event A", 10});
        trigger.trigger("event_b", EventData{"From Event B", 20});
        trigger.trigger("event_c", EventData{"From Event C", 30});
    }

    //==============================================================
    // 7. Error Handling
    //==============================================================
    SECTION("7. Error Handling");
    {
        atom::async::Trigger<EventData> trigger;

        // Register a callback that throws an exception
        [[maybe_unused]] auto errorCallbackId = trigger.registerCallback(
            "error_event", [](const EventData& /* data */) {
                std::cout << "  Callback attempting to throw exception"
                          << std::endl;
                throw std::runtime_error("This error should be caught");
            });

        // Register a normal callback that should still execute after exception
        [[maybe_unused]] auto normalCallbackId =
            trigger.registerCallback(
                "error_event", [](const EventData& data) {
                    std::cout
                        << "  Normal callback should execute after exception: ";
                    printEventData(data);
                });

        // Trigger the event
        LOG("Triggering event that may throw exception");
        try {
            size_t count = trigger.trigger("error_event",
                                           EventData{"Error handling", 500});
            LOG("Successfully executed callback count: " +
                std::to_string(count) + " (exception was caught internally)");
        } catch (const std::exception& e) {
            LOG("Caught exception: " + std::string(e.what()) +
                " (should not happen)");
        }

        // Try registering a null callback function
        LOG("Trying to register null callback function");
        try {
            [[maybe_unused]] auto id =
                trigger.registerCallback("empty_callback", nullptr);
        } catch (const atom::async::TriggerException& e) {
            LOG("Caught expected exception: " + std::string(e.what()));
        }

        // Try registering to empty event name
        LOG("Trying to register to empty event name");
        try {
            [[maybe_unused]] auto id =
                trigger.registerCallback("", [](const EventData&) {});
        } catch (const atom::async::TriggerException& e) {
            LOG("Caught expected exception: " + std::string(e.what()));
        }

        // Try triggering with negative delay
        LOG("Trying to trigger with negative delay");
        try {
            [[maybe_unused]] auto flag =
                trigger.scheduleTrigger("negative_delay", EventData{}, -100ms);
        } catch (const atom::async::TriggerException& e) {
            LOG("Caught expected exception: " + std::string(e.what()));
        }
    }

    //==============================================================
    // 8. Edge Cases
    //==============================================================
    SECTION("8. Edge Cases");
    {
        atom::async::Trigger<EventData> trigger;

        // Test triggering non-existent event
        LOG("Triggering non-existent event");
        size_t count = trigger.trigger("nonexistent_event", EventData{});
        LOG("Number of executed callbacks: " + std::to_string(count) +
            " (should be 0)");

        // Test unregistering non-existent callback
        LOG("Unregistering non-existent callback");
        bool result = trigger.unregisterCallback("nonexistent_event", 999);
        LOG("Unregistration result: " +
            std::string(result ? "Success" : "Failure") + " (should fail)");

        // Test triggering with zero delay
        LOG("Triggering with zero delay");
        auto zeroDelayFlag = trigger.scheduleTrigger(
            "zero_delay", EventData{"Zero delay", 0}, 0ms);
        std::this_thread::sleep_for(100ms);
        LOG("Zero delay trigger should execute immediately");

        // Test empty parameters
        LOG("Empty parameters test");
        [[maybe_unused]] auto emptyParamId =
            trigger.registerCallback("empty_param", [](const EventData& data) {
                std::cout << "  Received empty param event: ";
                printEventData(data);
            });
        trigger.trigger("empty_param", EventData{});
    }

#ifdef ATOM_USE_BOOST_LOCKFREE
    //==============================================================
    // 9. Lockfree Queue Functionality (only available when
    // ATOM_USE_BOOST_LOCKFREE is defined)
    //==============================================================
    SECTION("9. Lockfree Queue Functionality");
    {
        atom::async::Trigger<EventData> trigger;

        // Register callback for test event
        [[maybe_unused]] auto lockfreeCallbackId = trigger.registerCallback(
            "lockfree_event", [](const EventData& data) {
                std::cout << "  Lockfree queue event callback: ";
                printEventData(data);
            });

        // Create lockfree trigger queue
        LOG("Creating lockfree trigger queue");
        auto queue = trigger.createLockFreeTriggerQueue(100);

        // Add event to queue
        bool pushed = queue->push(
            {"lockfree_event", EventData{"Lockfree queue message", 42}});
        LOG("Added to queue: " + std::string(pushed ? "Success" : "Failure"));

        // Process events in queue
        LOG("Processing events in queue");
        size_t processed = trigger.processLockFreeTriggers(*queue);
        LOG("Number of processed events: " + std::to_string(processed));

        // Add multiple events
        LOG("Adding multiple events to queue");
        queue->push({"lockfree_event", EventData{"Batch 1", 1}});
        queue->push({"lockfree_event", EventData{"Batch 2", 2}});
        queue->push({"lockfree_event", EventData{"Batch 3", 3}});

        // Process partial events with maxEvents param
        LOG("Processing partial events (maxEvents=2)");
        processed = trigger.processLockFreeTriggers(*queue, 2);
        LOG("Number of processed events: " + std::to_string(processed) +
            " (should be 2)");

        // Process remaining events
        LOG("Processing remaining events");
        processed = trigger.processLockFreeTriggers(*queue);
        LOG("Number of processed events: " + std::to_string(processed) +
            " (should be 1)");
    }
#endif

    //==============================================================
    // 10. Complex Scenario: Multi-threading
    //==============================================================
    SECTION("10. Complex Scenario: Multi-threading");
    {
        atom::async::Trigger<EventData> trigger;

        // Register multiple callbacks
        for (int i = 1; i <= 5; ++i) {
            [[maybe_unused]] auto threadCallbackId = trigger.registerCallback(
                "thread_event", [i](const EventData& data) {
                    std::cout << "  Thread " << std::this_thread::get_id()
                              << " processing callback #" << i << ": "
                              << data.message << ", Value: " << data.value
                              << std::endl;
                    // Simulate processing time
                    std::this_thread::sleep_for(50ms);
                });
        }

        LOG("Triggering events from multiple threads");

        // Create multiple threads to trigger events simultaneously
        std::vector<std::thread> threads;
        for (int i = 1; i <= 3; ++i) {
            threads.emplace_back([&trigger, i]() {
                EventData data{"Thread" + std::to_string(i), i * 100};
                size_t count = trigger.trigger("thread_event", data);
                std::cout << "  Thread " << std::this_thread::get_id()
                          << " triggered " << count << " callbacks"
                          << std::endl;
            });
        }

        // Wait for all threads to complete
        for (auto& t : threads) {
            t.join();
        }

        LOG("Multi-thread triggering complete");
    }

    //==============================================================
    // 11. Using Trigger with Custom Types
    //==============================================================
    SECTION("11. Using Trigger with Custom Types");
    {
        // Using trigger with simple type
        atom::async::Trigger<int> intTrigger;
        [[maybe_unused]] auto intCallbackId =
            intTrigger.registerCallback("int_event", [](int value) {
                std::cout << "  Int trigger received value: " << value
                          << std::endl;
            });

        intTrigger.trigger("int_event", 42);

        // Using trigger with string
        atom::async::Trigger<std::string> stringTrigger;
        [[maybe_unused]] auto stringCallbackId = stringTrigger.registerCallback(
            "string_event", [](const std::string& msg) {
                std::cout << "  String trigger received: " << msg << std::endl;
            });

        stringTrigger.trigger("string_event", "Hello, World!");
    }

    std::cout << "\n===== Examples Complete =====\n";
    return 0;
}
