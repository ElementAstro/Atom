#include "atom/async/queue.hpp"

#include <iostream>
#include <thread>

using namespace atom::async;

int main() {
    // Create a ThreadSafeQueue for integers
    ThreadSafeQueue<int> queue;

    // Put elements into the queue
    queue.put(1);
    queue.put(2);
    queue.put(3);

    // Take an element from the queue
    auto element = queue.take();
    if (element) {
        std::cout << "Taken element: " << *element << std::endl;
    }

    // Check the size of the queue
    std::cout << "Queue size: " << queue.size() << std::endl;

    // Check if the queue is empty
    std::cout << "Is queue empty? " << (queue.empty() ? "Yes" : "No")
              << std::endl;

    // Clear the queue
    queue.clear();
    std::cout << "Queue cleared. Is queue empty? "
              << (queue.empty() ? "Yes" : "No") << std::endl;

    // Put elements into the queue again
    queue.put(4);
    queue.put(5);
    queue.put(6);

    // Get the front element
    auto frontElement = queue.front();
    if (frontElement) {
        std::cout << "Front element: " << *frontElement << std::endl;
    }

    // Get the back element
    auto backElement = queue.back();
    if (backElement) {
        std::cout << "Back element: " << *backElement << std::endl;
    }

    // Emplace an element into the queue
    queue.emplace(7);
    std::cout << "Element 7 emplaced. Queue size: " << queue.size()
              << std::endl;

    // Wait for an element that satisfies a predicate
    auto waitedElement =
        queue.waitFor([](const int& value) { return value == 5; });
    if (waitedElement) {
        std::cout << "Waited for element: " << *waitedElement << std::endl;
    }

    // Wait until the queue is empty
    std::thread waitThread([&queue]() {
        queue.waitUntilEmpty();
        std::cout << "Queue is now empty." << std::endl;
    });

    // Extract elements that satisfy a predicate
    auto extractedElements =
        queue.extractIf([](const int& value) { return value % 2 == 0; });
    std::cout << "Extracted elements: ";
    for (const auto& elem : extractedElements) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Sort the queue
    queue.put(3);
    queue.put(1);
    queue.put(2);
    queue.sort([](const int& a, const int& b) { return a < b; });
    std::cout << "Sorted queue: ";
    while (auto elem = queue.take()) {
        std::cout << *elem << " ";
    }
    std::cout << std::endl;

    // Transform the queue
    queue.put(1);
    queue.put(2);
    queue.put(3);
    auto transformedQueue =
        queue.transform<double>([](int value) { return value * 1.5; });
    std::cout << "Transformed queue: ";
    while (auto elem = transformedQueue->take()) {
        std::cout << *elem << " ";
    }
    std::cout << std::endl;

    // Group the queue by a key
    queue.put(1);
    queue.put(2);
    queue.put(3);
    queue.put(4);
    auto groupedQueues =
        queue.groupBy<int>([](const int& value) { return value % 2; });
    std::cout << "Grouped queues: " << std::endl;
    for (const auto& groupQueue : groupedQueues) {
        std::cout << "Group: ";
        while (auto elem = groupQueue->take()) {
            std::cout << *elem << " ";
        }
        std::cout << std::endl;
    }

    // Convert the queue to a vector
    queue.put(1);
    queue.put(2);
    queue.put(3);
    auto vector = queue.toVector();
    std::cout << "Queue as vector: ";
    for (const auto& elem : vector) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Apply a function to each element in the queue
    queue.forEach([](int& value) { value *= 2; });
    std::cout << "Queue after forEach: ";
    while (auto elem = queue.take()) {
        std::cout << *elem << " ";
    }
    std::cout << std::endl;

    // Try to take an element without blocking
    queue.put(1);
    auto tryElement = queue.tryTake();
    if (tryElement) {
        std::cout << "Try taken element: " << *tryElement << std::endl;
    }

    // Take an element with a timeout
    auto timeoutElement = queue.takeFor(std::chrono::milliseconds(100));
    if (timeoutElement) {
        std::cout << "Taken element with timeout: " << *timeoutElement
                  << std::endl;
    } else {
        std::cout << "Timeout occurred while taking element." << std::endl;
    }

    // Take an element until a specific time point
    auto untilElement = queue.takeUntil(std::chrono::steady_clock::now() +
                                        std::chrono::milliseconds(100));
    if (untilElement) {
        std::cout << "Taken element until time point: " << *untilElement
                  << std::endl;
    } else {
        std::cout << "Timeout occurred while taking element until time point."
                  << std::endl;
    }

    // Destroy the queue
    auto remainingQueue = queue.destroy();
    std::cout << "Queue destroyed. Remaining elements: ";
    while (!remainingQueue.empty()) {
        std::cout << remainingQueue.front() << " ";
        remainingQueue.pop();
    }
    std::cout << std::endl;

    // Join the wait thread
    waitThread.join();

    return 0;
}