/**
 * @file trackable_examples.cpp
 * @brief Comprehensive examples demonstrating the Trackable class
 * functionality.
 *
 * This file showcases all features of the Trackable<T> template class including
 * basic value tracking, observers, deferred notifications, and more.
 */

#include "../atom/type/trackable.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <complex>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n=== " << title << " ===" << std::endl;
    std::cout << std::string(title.length() + 8, '=') << std::endl;
}

// Helper function to print subsection headers
void printSubsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

// Example 1: Basic Usage
void basicUsageExample() {
    printSection("Basic Usage");

    // Create a Trackable with an initial integer value
    Trackable<int> trackableInt(42);

    // Get the current value
    std::cout << "Initial value: " << trackableInt.get() << std::endl;

    // Get the type name
    std::cout << "Type name: " << trackableInt.getTypeName() << std::endl;

    // Modify the value using assignment
    trackableInt = 100;
    std::cout << "After assignment: " << trackableInt.get() << std::endl;

    // Use conversion operator to convert to the underlying type
    int plainInt = static_cast<int>(trackableInt);
    std::cout << "Value after static_cast: " << plainInt << std::endl;

    // Arithmetic compound assignments
    trackableInt += 50;
    std::cout << "After += 50: " << trackableInt.get() << std::endl;

    trackableInt -= 25;
    std::cout << "After -= 25: " << trackableInt.get() << std::endl;

    trackableInt *= 2;
    std::cout << "After *= 2: " << trackableInt.get() << std::endl;

    trackableInt /= 5;
    std::cout << "After /= 5: " << trackableInt.get() << std::endl;
}

// Example 2: Observer Pattern
void observerPatternExample() {
    printSection("Observer Pattern");

    // Create a Trackable with an initial string value
    Trackable<std::string> trackableString("Hello");

    printSubsection("Subscribe Method");

    // Add an observer that prints changes
    trackableString.subscribe(
        [](const std::string& oldVal, const std::string& newVal) {
            std::cout << "Value changed from \"" << oldVal << "\" to \""
                      << newVal << "\"" << std::endl;
        });

    // Add another observer that counts characters
    trackableString.subscribe(
        [](const std::string& oldVal, const std::string& newVal) {
            std::cout << "Character count changed from " << oldVal.length()
                      << " to " << newVal.length() << std::endl;
        });

    // Modify the value to trigger notifications
    std::cout << "Changing value to trigger notifications:" << std::endl;
    trackableString = "Hello, World!";

    // Check if we have subscribers
    std::cout << "Has subscribers: "
              << (trackableString.hasSubscribers() ? "Yes" : "No") << std::endl;

    printSubsection("setOnChangeCallback Method");

    // Set an onChange callback that only receives the new value
    trackableString.setOnChangeCallback([](const std::string& newVal) {
        std::cout << "OnChange callback received new value: \"" << newVal
                  << "\"" << std::endl;
    });

    // Change the value again to trigger both observers and the onChange
    // callback
    std::cout << "Changing value again:" << std::endl;
    trackableString = "Changed again!";

    printSubsection("unsubscribeAll Method");

    // Unsubscribe all observers
    std::cout << "Unsubscribing all observers..." << std::endl;
    trackableString.unsubscribeAll();

    // Verify no subscribers remain
    std::cout << "Has subscribers after unsubscribe: "
              << (trackableString.hasSubscribers() ? "Yes" : "No") << std::endl;

    // Change the value one more time - the onChange callback should still work
    std::cout << "Changing value after unsubscribe:" << std::endl;
    trackableString = "Final change";
}

// Example 3: Custom Types
struct Point {
    int x;
    int y;

    // Define equality operator for Trackable to detect changes
    bool operator!=(const Point& other) const {
        return x != other.x || y != other.y;
    }

    // Define arithmetic operators for compound assignments
    Point operator+(const Point& other) const {
        return {x + other.x, y + other.y};
    }

    Point operator-(const Point& other) const {
        return {x - other.x, y - other.y};
    }

    Point operator*(const Point& other) const {
        return {x * other.x, y * other.y};
    }

    Point operator/(const Point& other) const {
        return {x / (other.x ? other.x : 1), y / (other.y ? other.y : 1)};
    }
};

// Formatter for Point to use with iostream
std::ostream& operator<<(std::ostream& os, const Point& p) {
    os << "(" << p.x << ", " << p.y << ")";
    return os;
}

void customTypesExample() {
    printSection("Custom Types");

    // Create a Trackable with a custom type
    Trackable<Point> trackablePoint({10, 20});

    // Print initial value
    const Point& initialPoint = trackablePoint.get();
    std::cout << "Initial point: " << initialPoint << std::endl;

    // Add an observer for the Point type
    trackablePoint.subscribe([](const Point& oldPoint, const Point& newPoint) {
        std::cout << "Point changed from " << oldPoint << " to " << newPoint
                  << std::endl;
    });

    // Modify using assignment
    std::cout << "Assigning new point..." << std::endl;
    trackablePoint = Point{30, 40};

    // Modify using compound operators
    std::cout << "Using += operator..." << std::endl;
    trackablePoint += Point{5, 10};
    std::cout << "Point after +=: " << trackablePoint.get() << std::endl;

    std::cout << "Using -= operator..." << std::endl;
    trackablePoint -= Point{10, 5};
    std::cout << "Point after -=: " << trackablePoint.get() << std::endl;

    std::cout << "Using *= operator..." << std::endl;
    trackablePoint *= Point{2, 3};
    std::cout << "Point after *=: " << trackablePoint.get() << std::endl;

    std::cout << "Using /= operator..." << std::endl;
    trackablePoint /= Point{5, 5};
    std::cout << "Point after /=: " << trackablePoint.get() << std::endl;

    // Type information
    std::cout << "Type name: " << trackablePoint.getTypeName() << std::endl;
}

// Example 4: Deferred Notifications
void deferredNotificationsExample() {
    printSection("Deferred Notifications");

    // Create a trackable value
    Trackable<double> trackableDouble(1.0);

    // Add an observer
    int notificationCount = 0;
    trackableDouble.subscribe(
        [&notificationCount](const double& oldVal, const double& newVal) {
            notificationCount++;
            std::cout << "Notification #" << notificationCount << ": " << oldVal
                      << " -> " << newVal << std::endl;
        });

    printSubsection("Regular Updates (Not Deferred)");

    // Make multiple changes - each will trigger a notification
    trackableDouble = 2.0;
    trackableDouble = 3.0;
    trackableDouble = 4.0;
    std::cout << "Notifications after individual updates: " << notificationCount
              << std::endl;

    printSubsection("Manual Deferred Notifications");

    // Enable deferred notifications
    trackableDouble.deferNotifications(true);

    // Make multiple changes - notifications will be deferred
    trackableDouble = 5.0;
    trackableDouble = 6.0;
    trackableDouble = 7.0;
    std::cout << "Current value during deferral: " << trackableDouble.get()
              << std::endl;
    std::cout << "Notifications before ending deferral: " << notificationCount
              << std::endl;

    // End deferral - this should trigger a single notification with the
    // first and last values in the deferred period
    trackableDouble.deferNotifications(false);
    std::cout << "Notifications after ending deferral: " << notificationCount
              << std::endl;

    printSubsection("Scoped Deferred Notifications");

    // Use scoped deferral
    {
        std::cout << "Entering scoped deferral..." << std::endl;
        auto deferralGuard = trackableDouble.deferScoped();

        // Make more changes
        trackableDouble = 8.0;
        trackableDouble = 9.0;
        trackableDouble = 10.0;

        std::cout << "Notifications during scoped deferral: "
                  << notificationCount << std::endl;
        std::cout << "Exiting scoped deferral (should trigger notification)..."
                  << std::endl;
        // deferralGuard goes out of scope here, which ends the deferral
    }

    std::cout << "Final notifications count: " << notificationCount
              << std::endl;
    std::cout << "Final value: " << trackableDouble.get() << std::endl;
}

// Example 5: Thread Safety
void threadSafetyExample() {
    printSection("Thread Safety");

    // Create a trackable counter
    Trackable<int> counter(0);

    // Track the number of notifications
    std::atomic<int> notificationCount(0);

    // Add an observer that just counts notifications
    counter.subscribe(
        [&notificationCount](const int&, const int&) { notificationCount++; });

    // Create multiple threads that increment the counter
    std::vector<std::thread> threads;
    const int numThreads = 5;
    const int incrementsPerThread = 100;

    std::cout << "Starting " << numThreads << " threads to increment counter..."
              << std::endl;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&counter]() {
            for (int j = 0; j < incrementsPerThread; ++j) {
                // Get current value
                int currentValue = counter.get();

                // Increment and set new value
                counter = currentValue + 1;

                // Small sleep to increase chance of race conditions if not
                // thread-safe
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    std::cout << "All threads completed" << std::endl;
    std::cout << "Expected final value: " << numThreads * incrementsPerThread
              << std::endl;
    std::cout << "Actual final value: " << counter.get() << std::endl;
    std::cout << "Number of notifications: " << notificationCount << std::endl;
}

// Example 6: Exception Handling in Observers
void exceptionHandlingExample() {
    printSection("Exception Handling in Observers");

    // Create a trackable value
    Trackable<int> trackableInt(0);

    printSubsection("Handling Exceptions in Observers");

    // Add a well-behaved observer
    trackableInt.subscribe([](const int& oldVal, const int& newVal) {
        std::cout << "Observer 1: " << oldVal << " -> " << newVal << std::endl;
    });

    // Add an observer that throws an exception
    trackableInt.subscribe([](const int& oldVal, const int& newVal) {
        std::cout << "Observer 2 (before throw): " << oldVal << " -> " << newVal
                  << std::endl;
        throw std::runtime_error("Intentional exception in observer");
    });

    // Add another well-behaved observer that should still be called
    trackableInt.subscribe([](const int& oldVal, const int& newVal) {
        std::cout << "Observer 3: " << oldVal << " -> " << newVal << std::endl;
    });

    // Trigger the observers
    try {
        std::cout << "Changing value to trigger observers (including one that "
                     "throws):"
                  << std::endl;
        trackableInt = 1;
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
    }

    printSubsection("Handling Exceptions in OnChangeCallback");

    // Set an onChange callback that throws
    trackableInt.setOnChangeCallback([](const int& newVal) {
        std::cout << "OnChange callback (before throw): " << newVal
                  << std::endl;
        throw std::runtime_error("Intentional exception in onChange callback");
    });

    // Trigger the callback
    try {
        std::cout
            << "Changing value to trigger onChange callback (that throws):"
            << std::endl;
        trackableInt = 2;
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
    }
}

// Example 7: Complex Data Structures
void complexDataStructuresExample() {
    printSection("Complex Data Structures");

    // Create a trackable vector of strings
    Trackable<std::vector<std::string>> trackableVector(
        std::vector<std::string>{"apple", "banana", "cherry"});

    // Add an observer
    trackableVector.subscribe([](const auto& oldVec, const auto& newVec) {
        std::cout << "Vector changed:" << std::endl;
        std::cout << "  Old size: " << oldVec.size()
                  << ", New size: " << newVec.size() << std::endl;

        std::cout << "  Old elements: ";
        for (const auto& item : oldVec) {
            std::cout << "\"" << item << "\" ";
        }
        std::cout << std::endl;

        std::cout << "  New elements: ";
        for (const auto& item : newVec) {
            std::cout << "\"" << item << "\" ";
        }
        std::cout << std::endl;
    });

    // Modify the vector
    std::vector<std::string> newVector = trackableVector.get();
    newVector.push_back("date");
    newVector.push_back("elderberry");

    std::cout << "Assigning modified vector..." << std::endl;
    trackableVector = newVector;

    // Create another vector and use compound assignment
    std::vector<std::string> additionalFruits{"fig", "grape"};

    // Define how to "add" two vectors for the += operator
    auto addVectors = [](const std::vector<std::string>& a,
                         const std::vector<std::string>& b) {
        std::vector<std::string> result = a;
        result.insert(result.end(), b.begin(), b.end());
        return result;
    };

    // Manually implement the += operation since std::vector doesn't have a
    // built-in += operator
    std::cout << "Adding more elements..." << std::endl;
    trackableVector = addVectors(trackableVector.get(), additionalFruits);

    // Show final state
    const auto& finalVector = trackableVector.get();
    std::cout << "Final vector contents: ";
    for (const auto& item : finalVector) {
        std::cout << "\"" << item << "\" ";
    }
    std::cout << std::endl;
}

// Example 8: Practical Use Cases
void practicalUseCasesExample() {
    printSection("Practical Use Cases");

    printSubsection("UI Data Binding Example");

    // Simulate a model value that would be bound to a UI element
    Trackable<std::string> userName("John Doe");

    // Simulate UI element update when model changes
    userName.subscribe([](const std::string&, const std::string& newVal) {
        std::cout << "UI updated to display name: " << newVal << std::endl;
    });

    // Simulate user interaction changing the model
    std::cout << "User edits their name in the UI..." << std::endl;
    userName = "Jane Smith";

    printSubsection("Configuration Change Propagation");

    // Simulate a configuration value
    Trackable<bool> darkModeEnabled(false);

    // Observers that respond to configuration changes
    darkModeEnabled.subscribe([](const bool& oldVal, const bool& newVal) {
        std::cout << "Theme system: Dark mode changed from "
                  << (oldVal ? "enabled" : "disabled") << " to "
                  << (newVal ? "enabled" : "disabled") << std::endl;
        std::cout << "Theme system: Applying new color palette..." << std::endl;
    });

    darkModeEnabled.subscribe([](const bool&, const bool& newVal) {
        std::cout << "UI Components: Updating all components to "
                  << (newVal ? "dark" : "light") << " theme" << std::endl;
    });

    // Change configuration
    std::cout << "User toggles dark mode setting..." << std::endl;
    darkModeEnabled = true;

    printSubsection("Progress Tracking");

    // Simulate a progress value (0-100%)
    Trackable<double> progressValue(0.0);

    // Progress bar observer
    progressValue.subscribe([](const double&, const double& newVal) {
        int barWidth = 50;
        int position = static_cast<int>(newVal / 100.0 * barWidth);

        std::cout << "[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < position)
                std::cout << "=";
            else if (i == position)
                std::cout << ">";
            else
                std::cout << " ";
        }
        std::cout << "] " << static_cast<int>(newVal) << "%" << std::endl;
    });

    // Simulate a process updating progress
    for (int i = 0; i <= 100; i += 10) {
        progressValue = static_cast<double>(i);
        // In a real application, we would do actual work here
        if (i < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// Example 9: Template Specialization Examples
void templateSpecializationExample() {
    printSection("Template Specialization Examples");

    // Different value types
    printSubsection("Various Value Types");

    // Integer
    Trackable<int> intValue(42);
    std::cout << "Integer type: " << intValue.getTypeName() << std::endl;

    // Double
    Trackable<double> doubleValue(3.14159);
    std::cout << "Double type: " << doubleValue.getTypeName() << std::endl;

    // String
    Trackable<std::string> stringValue("Hello");
    std::cout << "String type: " << stringValue.getTypeName() << std::endl;

    // Boolean
    Trackable<bool> boolValue(true);
    std::cout << "Boolean type: " << boolValue.getTypeName() << std::endl;

    // Complex number
    Trackable<std::complex<double>> complexValue(
        std::complex<double>(1.0, 2.0));
    std::cout << "Complex type: " << complexValue.getTypeName() << std::endl;

    // Custom class
    Trackable<Point> pointValue({1, 2});
    std::cout << "Custom type: " << pointValue.getTypeName() << std::endl;

    // STL container
    Trackable<std::vector<int>> vectorValue({1, 2, 3});
    std::cout << "Vector type: " << vectorValue.getTypeName() << std::endl;
}

// Example 10: Performance Considerations
void performanceConsiderationsExample() {
    printSection("Performance Considerations");

    // Create a trackable value for performance testing
    Trackable<int> trackableInt(0);

    // Add an observer that does minimal work
    trackableInt.subscribe([](const int&, const int&) {
        // Do nothing
    });

    printSubsection("Update Performance");

    // Measure time for multiple updates
    const int updateCount = 100000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < updateCount; ++i) {
        trackableInt = i;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Time to perform " << updateCount
              << " updates with notification: " << duration.count() << " ms"
              << std::endl;

    printSubsection("Deferred Update Performance");

    // Reset the value
    trackableInt = 0;

    // Measure time for deferred updates
    start = std::chrono::high_resolution_clock::now();

    {
        auto deferGuard = trackableInt.deferScoped();
        for (int i = 0; i < updateCount; ++i) {
            trackableInt = i;
        }
    }

    end = std::chrono::high_resolution_clock::now();
    duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Time to perform " << updateCount
              << " updates with deferred notification: " << duration.count()
              << " ms" << std::endl;
    std::cout << "Final value: " << trackableInt.get() << std::endl;

    printSubsection("Memory Usage");

    // Create trackable values with different numbers of observers
    std::vector<std::unique_ptr<Trackable<int>>> trackables;
    std::vector<std::function<void(const int&, const int&)>> observers;

    // Create some observer functions
    for (int i = 0; i < 1000; ++i) {
        observers.push_back([](const int&, const int&) {
            // Do nothing
        });
    }

    // Create trackables with different numbers of observers
    std::vector<int> observerCounts = {0, 1, 10, 100, 1000};
    for (int count : observerCounts) {
        auto trackable = std::make_unique<Trackable<int>>(0);

        for (int i = 0; i < count; ++i) {
            trackable->subscribe(observers[i]);
        }

        trackables.push_back(std::move(trackable));
    }

    // Report
    std::cout << "Created " << trackables.size()
              << " trackable objects with varying numbers of observers"
              << std::endl;
    std::cout
        << "Note: Actual memory usage would require specialized profiling tools"
        << std::endl;
}

int main() {
    std::cout << "=====================================" << std::endl;
    std::cout << "  Trackable<T> Comprehensive Examples" << std::endl;
    std::cout << "=====================================" << std::endl;

    try {
        // Run all examples
        basicUsageExample();
        observerPatternExample();
        customTypesExample();
        deferredNotificationsExample();
        threadSafetyExample();
        exceptionHandlingExample();
        complexDataStructuresExample();
        practicalUseCasesExample();
        templateSpecializationExample();
        performanceConsiderationsExample();

        std::cout << "\nAll examples completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception caught in main: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
