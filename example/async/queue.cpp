#include "../atom/async/queue.hpp"
#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n====== " << title << " ======\n";
}

// Helper function to print subsection headers
void printSubsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---\n";
}

// Helper function for generating random integers
int getRandomInt(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(min, max);
    return dist(gen);
}

// Example 1: Basic usage of ThreadSafeQueue
void basicUsageExample() {
    printSection("Basic Usage Example");

    // Create a queue of integers
    atom::async::ThreadSafeQueue<int> queue;

    // Check if queue is empty
    std::cout << "Is queue empty? " << (queue.empty() ? "Yes" : "No")
              << std::endl;

    // Add elements to the queue
    std::cout << "Adding elements to queue: 10, 20, 30" << std::endl;
    queue.put(10);
    queue.put(20);
    queue.put(30);

    std::cout << "Queue size: " << queue.size() << std::endl;

    // Retrieve elements from the queue
    std::cout << "Taking elements from queue:" << std::endl;
    while (auto element = queue.tryTake()) {
        std::cout << "Got: " << *element << std::endl;
    }

    std::cout << "Queue size after removal: " << queue.size() << std::endl;
    std::cout << "Is queue empty? " << (queue.empty() ? "Yes" : "No")
              << std::endl;
}

struct Person {
    std::string name;
    int age;

    Person(std::string n, int a) : name(std::move(n)), age(a) {}

    friend std::ostream& operator<<(std::ostream& os, const Person& p) {
        return os << p.name << " (" << p.age << ")";
    }
};

// Example 2: Working with different data types
void dataTypesExample() {
    printSection("Different Data Types Example");

    // String queue example
    printSubsection("String Queue");
    atom::async::ThreadSafeQueue<std::string> stringQueue;

    stringQueue.put("Hello");
    stringQueue.put("World");
    stringQueue.emplace("from ThreadSafeQueue");

    std::cout << "String queue size: " << stringQueue.size() << std::endl;

    while (auto element = stringQueue.tryTake()) {
        std::cout << "String element: " << *element << std::endl;
    }

    // Custom struct example
    printSubsection("Custom Struct Queue");

    atom::async::ThreadSafeQueue<Person> personQueue;

    personQueue.put(Person("Alice", 30));
    personQueue.emplace("Bob", 25);

    std::cout << "Person queue size: " << personQueue.size() << std::endl;

    while (auto person = personQueue.tryTake()) {
        std::cout << "Person: " << *person << std::endl;
    }

    // std::unique_ptr queue example
    printSubsection("std::unique_ptr Queue");
    atom::async::ThreadSafeQueue<std::unique_ptr<int>> ptrQueue;

    ptrQueue.put(std::make_unique<int>(42));
    ptrQueue.put(std::make_unique<int>(100));

    std::cout << "Pointer queue size: " << ptrQueue.size() << std::endl;

    while (auto ptr = ptrQueue.tryTake()) {
        std::cout << "Pointer value: " << **ptr << std::endl;
    }
}

// Example 3: Multi-threading with ThreadSafeQueue
void multithreadingExample() {
    printSection("Multi-threading Example");

    atom::async::ThreadSafeQueue<int> sharedQueue;
    std::atomic<bool> producerDone = false;

    // Create a producer thread that adds elements to the queue
    std::thread producer([&sharedQueue, &producerDone]() {
        for (int i = 1; i <= 10; ++i) {
            std::cout << "Producer: Adding " << i << std::endl;
            sharedQueue.put(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "Producer: Done" << std::endl;
        producerDone = true;
    });

    // Create consumer threads that take elements from the queue
    std::thread consumer1([&sharedQueue, &producerDone]() {
        while (!producerDone || !sharedQueue.empty()) {
            if (auto element = sharedQueue.tryTake()) {
                std::cout << "Consumer 1: Got " << *element << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
        std::cout << "Consumer 1: Done" << std::endl;
    });

    std::thread consumer2([&sharedQueue, &producerDone]() {
        while (!producerDone || !sharedQueue.empty()) {
            if (auto element = sharedQueue.tryTake()) {
                std::cout << "Consumer 2: Got " << *element << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        std::cout << "Consumer 2: Done" << std::endl;
    });

    // Wait for all threads to complete
    producer.join();
    consumer1.join();
    consumer2.join();

    std::cout << "All threads have finished" << std::endl;
}

// Example 4: Handle edge cases and boundary values
void edgeCasesExample() {
    printSection("Edge Cases Example");

    printSubsection("Empty Queue Operations");
    atom::async::ThreadSafeQueue<int> emptyQueue;

    // Try operations on an empty queue
    std::cout << "Is empty queue empty? " << (emptyQueue.empty() ? "Yes" : "No")
              << std::endl;
    std::cout << "Empty queue size: " << emptyQueue.size() << std::endl;

    auto frontElement = emptyQueue.front();
    std::cout << "Front element exists? "
              << (frontElement.has_value() ? "Yes" : "No") << std::endl;

    auto backElement = emptyQueue.back();
    std::cout << "Back element exists? "
              << (backElement.has_value() ? "Yes" : "No") << std::endl;

    auto takeResult = emptyQueue.tryTake();
    std::cout << "Can take from empty? "
              << (takeResult.has_value() ? "Yes" : "No") << std::endl;

    printSubsection("Queue Destruction With Elements");
    {
        atom::async::ThreadSafeQueue<int> tempQueue;
        tempQueue.put(1);
        tempQueue.put(2);
        tempQueue.put(3);
        std::cout << "Created queue with 3 elements, size: " << tempQueue.size()
                  << std::endl;
        std::cout << "Queue will be destroyed now..." << std::endl;
        // Queue gets destroyed here when it goes out of scope
    }
    std::cout << "Queue destroyed successfully" << std::endl;

    printSubsection("Concurrent Access Edge Cases");
    atom::async::ThreadSafeQueue<int> concurrentQueue;

    // Add 1000 items
    for (int i = 0; i < 1000; ++i) {
        concurrentQueue.put(i);
    }

    std::cout << "Queue loaded with 1000 elements" << std::endl;

    // Spawn 5 threads to simultaneously take from the queue
    std::vector<std::thread> threads;
    std::atomic<int> totalTaken = 0;

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&concurrentQueue, &totalTaken, i]() {
            int threadTotal = 0;
            while (auto item = concurrentQueue.tryTake()) {
                threadTotal++;
                // No output to avoid console flooding
            }
            std::cout << "Thread " << i << " took " << threadTotal << " items"
                      << std::endl;
            totalTaken += threadTotal;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "All threads finished, total taken: " << totalTaken
              << std::endl;
    std::cout << "Queue size after concurrent taking: "
              << concurrentQueue.size() << std::endl;
    assert(concurrentQueue.empty());
}

// Example 5: Timeouts and waiting for elements
void timeoutExample() {
    printSection("Timeout Example");

    atom::async::ThreadSafeQueue<int> queue;

    printSubsection("Timeout while waiting for element");

    // Start a thread that waits with timeout
    std::thread waitThread([&queue]() {
        std::cout << "Thread starts waiting for element with 2s timeout..."
                  << std::endl;
        auto startTime = std::chrono::steady_clock::now();

        auto result = queue.takeFor(std::chrono::seconds(2));

        auto endTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        if (result) {
            std::cout << "Received element " << *result << " after "
                      << elapsed.count() << "ms" << std::endl;
        } else {
            std::cout << "Timeout after " << elapsed.count() << "ms"
                      << std::endl;
        }
    });

    // Wait 1 second and add an element (before timeout)
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Adding element to queue after 1s" << std::endl;
    queue.put(42);

    waitThread.join();

    // Clear queue
    queue.clear();

    printSubsection("Wait until specific time point");

    std::thread waitUntilThread([&queue]() {
        auto futureTime =
            std::chrono::steady_clock::now() + std::chrono::seconds(3);
        std::cout << "Thread starts waiting until specific time point (3s from "
                     "now)..."
                  << std::endl;

        auto result = queue.takeUntil(futureTime);

        if (result) {
            std::cout << "Received element: " << *result << std::endl;
        } else {
            std::cout << "Timed out waiting until specific time point"
                      << std::endl;
        }
    });

    // This time, don't add anything to the queue
    waitUntilThread.join();
}

// Example 6: Advanced features - filtering, transforming, batch processing
void advancedFeaturesExample() {
    printSection("Advanced Features Example");

    atom::async::ThreadSafeQueue<int> queue;

    // Fill queue with numbers
    for (int i = 1; i <= 20; ++i) {
        queue.put(i);
    }

    printSubsection("Filter elements");
    std::cout << "Initial queue size: " << queue.size() << std::endl;

    // Filter out odd numbers
    queue.filter([](const int& value) { return value % 2 == 0; });

    std::cout << "Size after filtering (even numbers only): " << queue.size()
              << std::endl;
    std::cout << "Elements after filtering: ";
    auto filteredElements = queue.toVector();
    for (const auto& elem : filteredElements) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Clear and refill queue
    queue.clear();
    for (int i = 1; i <= 10; ++i) {
        queue.put(i);
    }

    printSubsection("Transform elements");

    // Transform: multiply each element by 10
    auto transformedQueue =
        queue.transform<int>([](int value) { return value * 10; });

    std::cout << "Original queue: ";
    for (const auto& elem : queue.toVector()) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    std::cout << "Transformed queue: ";
    for (const auto& elem : transformedQueue->toVector()) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    printSubsection("Process elements in batches");

    // Clear and refill queue with more elements
    queue.clear();
    for (int i = 1; i <= 50; ++i) {
        queue.put(i);
    }

    std::cout << "Processing " << queue.size() << " elements in batches of 10"
              << std::endl;

    size_t batchCount = queue.processBatches(10, [](std::span<int> batch) {
        std::cout << "Batch of size " << batch.size() << ": ";
        for (size_t i = 0; i < std::min<size_t>(batch.size(), 5); ++i) {
            std::cout << batch[i] << " ";
        }
        if (batch.size() > 5) {
            std::cout << "... ";
        }
        std::cout << std::endl;

        // Simulate batch processing
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    });

    std::cout << "Processed " << batchCount << " batches" << std::endl;

    printSubsection("Extracting elements");

    // Extract all elements greater than 25
    auto extracted =
        queue.filterOut([](const int& value) -> bool { return value > 25; });
    if (extracted) {
        auto extractedVec = extracted->toVector();
        // 使用 extractedVec 的代码
    }
    auto extractedVec = extracted->toVector();

    std::cout << "Extracted " << extractedVec.size() << " elements > 25"
              << std::endl;
    std::cout << "Queue size after extraction: " << queue.size() << std::endl;

    std::cout << "First few extracted elements: ";
    for (size_t i = 0; i < std::min<size_t>(5, extractedVec.size()); ++i) {
        std::cout << extractedVec[i] << " ";
    }
    std::cout << (extractedVec.size() > 5 ? "..." : "") << std::endl;
}

// Example 7: Use every queue method once
void completeAPIExample() {
    printSection("Complete API Example");

    atom::async::ThreadSafeQueue<std::string> queue;

    printSubsection("Basic queue operations");

    // Check empty
    std::cout << "New queue is empty: " << (queue.empty() ? "yes" : "no")
              << std::endl;

    // Put elements
    queue.put("first");
    queue.put("second");

    // Use emplace
    queue.emplace("third");

    // Size
    std::cout << "Size after adding 3 items: " << queue.size() << std::endl;

    // Front and back
    std::cout << "Front element: " << *queue.front() << std::endl;
    std::cout << "Back element: " << *queue.back() << std::endl;

    // Take and tryTake
    std::cout << "Taking first element: " << *queue.take() << std::endl;
    std::cout << "Try-taking second element: " << *queue.tryTake() << std::endl;

    // Check if queue has one element left
    std::cout << "Queue size: " << queue.size() << std::endl;
    std::cout << "Queue is empty: " << (queue.empty() ? "yes" : "no")
              << std::endl;

    printSubsection("Advanced operations");

    // Add more elements
    for (int i = 1; i <= 5; ++i) {
        queue.put("item-" + std::to_string(i));
    }

    // Convert to vector
    auto vec = queue.toVector();
    std::cout << "Queue as vector (size=" << vec.size() << "): ";
    for (const auto& item : vec) {
        std::cout << item << " ";
    }
    std::cout << std::endl;

    // Use forEach
    std::cout << "Using forEach to print items: " << std::endl;
    queue.forEach(
        [](std::string& item) { std::cout << "Item: " << item << std::endl; });

    // Sort the queue
    queue.sort([](const std::string& a, const std::string& b) {
        // Sort in reverse
        return a > b;
    });

    std::cout << "Queue after sorting (reversed): ";
    for (const auto& item : queue.toVector()) {
        std::cout << item << " ";
    }
    std::cout << std::endl;

    // Group by first character
    printSubsection("Grouping elements");

    queue.clear();
    queue.put("apple");
    queue.put("banana");
    queue.put("apricot");
    queue.put("berry");
    queue.put("cherry");
    queue.put("cantaloupe");

    auto groupedQueues = queue.groupBy<char>(
        [](const std::string& s) { return s.empty() ? ' ' : s[0]; });

    std::cout << "Grouped " << queue.size() << " fruits into "
              << groupedQueues.size() << " groups by first letter" << std::endl;

    for (const auto& groupQueue : groupedQueues) {
        if (groupQueue->empty())
            continue;

        char groupKey = groupQueue->front()->at(0);
        std::cout << "Group '" << groupKey << "' contains: ";

        for (const auto& item : groupQueue->toVector()) {
            std::cout << item << " ";
        }
        std::cout << std::endl;
    }

    // Wait for specific item
    printSubsection("Wait for specific item");

    queue.clear();

    std::thread waiterThread([&queue]() {
        std::cout << "Waiting for an item starting with 'z'..." << std::endl;

        auto startTime = std::chrono::steady_clock::now();

        auto result = queue.waitFor(
            [](const std::string& s) { return !s.empty() && s[0] == 'z'; });

        auto endTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        if (result) {
            std::cout << "Found item '" << *result << "' after "
                      << elapsed.count() << "ms" << std::endl;
        } else {
            std::cout << "Wait returned without result after "
                      << elapsed.count() << "ms" << std::endl;
        }
    });

    // Wait a bit then add items
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Adding items to queue..." << std::endl;
    queue.put("apple");
    queue.put("banana");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    queue.put("zebra");  // This should trigger the waiter

    waiterThread.join();

    // Destroy queue with items
    printSubsection("Destroying queue");

    std::cout << "Queue size before destroy: " << queue.size() << std::endl;
    auto remainingItems = queue.destroy();
    std::cout << "Queue destroyed, retrieved " << remainingItems.size()
              << " remaining items" << std::endl;

    std::cout << "Remaining items: ";
    while (!remainingItems.empty()) {
        std::cout << remainingItems.front() << " ";
        remainingItems.pop();
    }
    std::cout << std::endl;
}

// Example 8: Error handling scenarios
void errorHandlingExample() {
    printSection("Error Handling Example");

    atom::async::ThreadSafeQueue<int> queue;

    printSubsection("Handle invalid batch size");

    try {
        std::cout << "Trying to process with batch size 0..." << std::endl;
        queue.processBatches(0, [](std::span<int>) {
            // This should never execute
        });
    } catch (const std::exception& e) {
        std::cout << "Caught exception as expected: " << e.what() << std::endl;
    }

    printSubsection("Handle cancellation during waiting");

    std::thread waitThread([&queue]() {
        std::cout << "Thread waiting for element..." << std::endl;
        auto element = queue.take();
        if (element) {
            std::cout << "Got element: " << *element << std::endl;
        } else {
            std::cout << "Waiting cancelled, received nullopt" << std::endl;
        }
    });

    // Wait a bit then destroy the queue
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Destroying queue while thread is waiting..." << std::endl;
    auto discarded = queue.destroy();
    std::cout << "Discarded " << discarded.size() << " elements" << std::endl;

    waitThread.join();

    printSubsection("Recover from errors");

    atom::async::ThreadSafeQueue<std::unique_ptr<int>> ptrQueue;

    // Try to put null pointers
    std::cout << "Adding valid and null pointers to queue" << std::endl;
    ptrQueue.put(std::make_unique<int>(42));
    ptrQueue.put(nullptr);  // This will be stored as a null unique_ptr
    ptrQueue.put(std::make_unique<int>(100));

    std::cout << "Queue size: " << ptrQueue.size() << std::endl;

    // Process safely
    std::cout << "Safely processing potentially null pointers:" << std::endl;
    while (auto ptr = ptrQueue.tryTake()) {
        if (*ptr) {
            std::cout << "Valid pointer with value: " << **ptr << std::endl;
        } else {
            std::cout << "Encountered null pointer" << std::endl;
        }
    }
}

// Main function to run all examples
int main() {
    std::cout << "ThreadSafeQueue Examples\n";
    std::cout << "======================\n\n";

    try {
        // Run individual examples
        basicUsageExample();
        dataTypesExample();
        multithreadingExample();
        edgeCasesExample();
        timeoutExample();
        advancedFeaturesExample();
        completeAPIExample();
        errorHandlingExample();

        std::cout << "\n======================\n";
        std::cout << "All examples completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
