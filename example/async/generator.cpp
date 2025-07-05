#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Define necessary feature flags
#define ATOM_USE_BOOST_LOCKFREE
#define ATOM_USE_BOOST_LOCKS

#include "atom/async/generator.hpp"

using namespace atom::async;
using namespace std::chrono_literals;

void printSeparator(const std::string& title) {
    std::cout << "\n===== " << title << " =====\n" << std::endl;
}

// ===== Basic Usage Examples =====

// Simple integer generator
Generator<int> simpleNumberGenerator(int start, int end) {
    std::cout << "Generator started with range " << start << " to " << end
              << std::endl;
    for (int i = start; i <= end; ++i) {
        std::cout << "Yielding: " << i << std::endl;
        co_yield i;
    }
    std::cout << "Generator completed" << std::endl;
}

// Demonstrates basic usage of a generator
void basicUsageExample() {
    printSeparator("Basic Generator Usage");

    std::cout << "Creating generator for numbers 1 to 5..." << std::endl;
    Generator<int> gen = simpleNumberGenerator(1, 5);

    std::cout << "Consuming values using range-based for loop:" << std::endl;
    for (const auto& value : gen) {
        std::cout << "Received: " << value << std::endl;
    }
}

// Generator with complex type
Generator<std::string> stringGenerator() {
    co_yield "Hello";
    co_yield "World";
    co_yield "C++20";
    co_yield "Coroutines";
}

// Demonstrates using generators with different types
void differentTypesExample() {
    printSeparator("Different Return Types");

    // String generator
    std::cout << "String generator example:" << std::endl;
    Generator<std::string> strGen = stringGenerator();
    for (const auto& str : strGen) {
        std::cout << "String: " << str << std::endl;
    }

    // From range helper function
    std::cout << "\nFrom range example:" << std::endl;
    std::vector<double> values = {3.14, 2.71, 1.618, 1.414};
    auto rangeGen = from_range(values);
    for (const auto& val : rangeGen) {
        std::cout << "Value: " << val << std::endl;
    }

    // Range generator helper
    std::cout << "\nRange helper example (0 to 4 step 1):" << std::endl;
    for (const auto& num : range(0, 5)) {
        std::cout << num << " ";
    }
    std::cout << std::endl;

    // Range with custom step
    std::cout << "\nRange with step example (0 to 10 step 2):" << std::endl;
    for (const auto& num : range(0, 11, 2)) {
        std::cout << num << " ";
    }
    std::cout << std::endl;
}

// ===== Edge Cases and Boundary Values =====

// Empty generator
Generator<int> emptyGenerator() {
    if (false) {
        co_yield 42;  // Never reached
    }
}

// Infinite generator
Generator<int> infiniteGenerator(int start = 0) {
    int current = start;
    while (true) {
        co_yield current++;
    }
    // Note: This line is never reached
}

// Generator with a single value
Generator<int> singleValueGenerator(int value) { co_yield value; }

void edgeCasesExample() {
    printSeparator("Edge Cases");

    // Empty generator
    std::cout << "Empty generator example:" << std::endl;
    Generator<int> emptyGen = emptyGenerator();
    bool hasValue = false;
    for (const auto& val : emptyGen) {
        std::cout << "Value: " << val << std::endl;
        hasValue = true;
    }
    std::cout << "Generator had values: " << (hasValue ? "yes" : "no")
              << std::endl;

    // Single value generator
    std::cout << "\nSingle value generator example:" << std::endl;
    Generator<int> singleGen = singleValueGenerator(42);
    for (const auto& val : singleGen) {
        std::cout << "Value: " << val << std::endl;
    }

    // Infinite generator (limited by user intervention)
    std::cout << "\nInfinite generator example (limited to 5 values):"
              << std::endl;
    Generator<int> infGen = infiniteGenerator(10);
    int count = 0;
    for (const auto& val : infGen) {
        std::cout << "Value: " << val << std::endl;
        if (++count >= 5) {
            std::cout << "Breaking out of infinite generator after 5 values"
                      << std::endl;
            break;
        }
    }

    // Using the infinite_range helper
    std::cout << "\nInfinite range helper (limited to 5 values):" << std::endl;
    count = 0;
    for (const auto& val : infinite_range(100)) {
        std::cout << "Value: " << val << std::endl;
        if (++count >= 5) {
            std::cout << "Breaking out after 5 values" << std::endl;
            break;
        }
    }
}

// ===== Error Handling Examples =====

// Generator that throws an exception
Generator<int> exceptionGenerator() {
    std::cout << "Starting exception generator" << std::endl;
    co_yield 1;
    co_yield 2;
    throw std::runtime_error("Generator error occurred!");
    co_yield 3;  // Never reached
}

void errorHandlingExample() {
    printSeparator("Error Handling");

    // Handling exceptions from a generator
    std::cout << "Exception handling example:" << std::endl;
    Generator<int> exGen = exceptionGenerator();

    try {
        for (const auto& val : exGen) {
            std::cout << "Value before exception: " << val << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Caught exception: " << e.what() << std::endl;
    }

    // Manual error handling with iterator
    std::cout << "\nError handling with iterators:" << std::endl;
    Generator<int> exGen2 = exceptionGenerator();
    auto it = exGen2.begin();

    try {
        while (it != exGen2.end()) {
            std::cout << "Value: " << *it << std::endl;
            ++it;
        }
    } catch (const std::exception& e) {
        std::cout << "Caught exception: " << e.what() << std::endl;
    }
}

// ===== Two-Way Generator Examples =====

// Simple two-way generator that echoes with modification
TwoWayGenerator<int, int> echoGenerator() {
    int received = 0;
    while (true) {
        received = co_yield received * 2;
    }
}

// Two-way generator with void receive
TwoWayGenerator<std::string, void> messageGenerator() {
    co_yield "Hello";
    co_yield "World";
    co_yield "C++20";
    co_yield "Coroutines";
}

void twoWayGeneratorExample() {
    printSeparator("Two-Way Generator Examples");

    // Echo generator example
    std::cout << "Echo generator example:" << std::endl;
    auto echo = echoGenerator();

    for (int i = 1; i <= 5; ++i) {
        std::cout << "Sending: " << i << std::endl;
        int response = echo.next(i);
        std::cout << "Received: " << response << std::endl;
    }

    // Generator without receive type
    std::cout << "\nMessage generator example:" << std::endl;
    auto messages = messageGenerator();

    try {
        while (!messages.done()) {
            std::cout << "Message: " << messages.next() << std::endl;
        }
    } catch (const std::logic_error& e) {
        std::cout << "Generator finished: " << e.what() << std::endl;
    }
}

// ===== Advanced Examples =====

// Generator that produces a Fibonacci sequence
Generator<unsigned long long> fibonacciGenerator(int limit) {
    if (limit <= 0) {
        co_return;
    }

    unsigned long long a = 0, b = 1;
    co_yield a;

    if (limit == 1) {
        co_return;
    }

    co_yield b;
    int count = 2;

    while (count < limit) {
        unsigned long long next = a + b;
        co_yield next;
        a = b;
        b = next;
        ++count;
    }
}

// Generator that lazily processes data
Generator<std::string> lazyTransform(
    const std::vector<int>& data, std::function<std::string(int)> transformer) {
    for (const auto& item : data) {
        // Simulate expensive operation
        std::this_thread::sleep_for(50ms);
        co_yield transformer(item);
    }
}

void advancedExamples() {
    printSeparator("Advanced Generator Examples");

    // Fibonacci sequence
    std::cout << "Fibonacci sequence (first 10 numbers):" << std::endl;
    auto fib = fibonacciGenerator(10);
    for (const auto& num : fib) {
        std::cout << num << " ";
    }
    std::cout << std::endl;

    // Lazy transformation pipeline
    std::cout << "\nLazy transformation example:" << std::endl;
    std::vector<int> data = {1, 2, 3, 4, 5};

    auto start = std::chrono::high_resolution_clock::now();

    auto transformed = lazyTransform(data, [](int n) {
        return "Processed item: " + std::to_string(n * 10);
    });

    std::cout << "Generator created (lazy, no processing done yet)"
              << std::endl;

    // Consuming the transformed data (this is where the actual work happens)
    for (const auto& result : transformed) {
        std::cout << result << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Transformation took " << duration.count() << "ms"
              << std::endl;
}

#ifdef ATOM_USE_BOOST_LOCKS
void threadSafeGeneratorExample() {
    printSeparator("Thread-Safe Generator Example");

    // Create a thread-safe generator function
    auto threadSafeGen = []() -> ThreadSafeGenerator<int> {
        for (int i = 0; i < 10; ++i) {
            co_yield i;
        }
    };

    ThreadSafeGenerator<int> gen = threadSafeGen();

    // Simulate multiple threads consuming the generator
    std::vector<std::thread> threads;
    std::mutex outputMutex;

    for (int t = 0; t < 3; ++t) {
        threads.emplace_back([&gen, t, &outputMutex]() {
            try {
                // Each thread will try to consume some values
                for (const auto& val : gen) {
                    {
                        std::lock_guard<std::mutex> lock(outputMutex);
                        std::cout << "Thread " << t << " got value: " << val
                                  << std::endl;
                    }
                    std::this_thread::sleep_for(10ms);  // Simulate work
                }
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cout << "Thread " << t << " caught exception: " << e.what()
                          << std::endl;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
}
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
void concurrentGeneratorExample() {
    printSeparator("Concurrent Generator Example");

    // Create a concurrent generator from a regular generator function
    auto genFunc = []() -> Generator<int> {
        for (int i = 0; i < 20; ++i) {
            co_yield i;
            // Simulate varying processing times
            std::this_thread::sleep_for(
                std::chrono::milliseconds(10 + (i % 5) * 10));
        }
    };

    ConcurrentGenerator<int> concurrentGen(genFunc);

    // Multiple consumer threads
    std::vector<std::thread> consumerThreads;
    std::mutex outputMutex;

    for (int t = 0; t < 4; ++t) {
        consumerThreads.emplace_back([&concurrentGen, t, &outputMutex]() {
            try {
                while (!concurrentGen.done()) {
                    int value;
                    if (concurrentGen.try_next(value)) {
                        {
                            std::lock_guard<std::mutex> lock(outputMutex);
                            std::cout << "Consumer " << t
                                      << " received: " << value << std::endl;
                        }
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(15 + t * 5));
                    }
                }
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cout << "Consumer " << t << " error: " << e.what()
                          << std::endl;
            }

            {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cout << "Consumer " << t << " finished" << std::endl;
            }
        });
    }

    // Wait for consumers to finish
    for (auto& thread : consumerThreads) {
        thread.join();
    }

    std::cout << "All consumers finished" << std::endl;
}

void lockFreeTwoWayGeneratorExample() {
    printSeparator("Lock-Free Two-Way Generator Example");

    // Create a two-way generator that echoes input with transformation
    auto twoWayFunc = []() -> TwoWayGenerator<std::string, int> {
        int value = 0;
        while (true) {
            value = co_yield "Received: " + std::to_string(value) +
                    ", squared: " + std::to_string(value * value);
        }
    };

    LockFreeTwoWayGenerator<std::string, int> twoWayGen(twoWayFunc);

    // Producer thread sending values
    std::thread producerThread([&twoWayGen]() {
        for (int i = 1; i <= 10; ++i) {
            try {
                std::string response = twoWayGen.send(i);
                std::cout << "Producer sent: " << i << ", got: " << response
                          << std::endl;
                std::this_thread::sleep_for(50ms);
            } catch (const std::exception& e) {
                std::cout << "Producer error: " << e.what() << std::endl;
                break;
            }
        }
    });

    producerThread.join();
}
#endif

// Main function running all examples
int main() {
    try {
        std::cout << "C++20 Generator Examples" << std::endl;

        // Basic examples
        basicUsageExample();
        differentTypesExample();

        // Edge cases
        edgeCasesExample();

        // Error handling
        errorHandlingExample();

        // Two-way generators
        twoWayGeneratorExample();

        // Advanced examples
        advancedExamples();

        // Thread-safe generators (if ATOM_USE_BOOST_LOCKS is defined)
#ifdef ATOM_USE_BOOST_LOCKS
        threadSafeGeneratorExample();
#else
        std::cout << "\n===== Thread-Safe Generator Example =====\n"
                  << std::endl;
        std::cout << "Skipped: ATOM_USE_BOOST_LOCKS not defined" << std::endl;
#endif

        // Concurrent generators (if ATOM_USE_BOOST_LOCKFREE is defined)
#ifdef ATOM_USE_BOOST_LOCKFREE
        concurrentGeneratorExample();
        lockFreeTwoWayGeneratorExample();
#else
        std::cout << "\n===== Concurrent Generator Example =====\n"
                  << std::endl;
        std::cout << "Skipped: ATOM_USE_BOOST_LOCKFREE not defined"
                  << std::endl;
#endif

        std::cout << "\nAll examples completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
